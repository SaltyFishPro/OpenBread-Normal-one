#!/usr/bin/env python3
"""Pull latest GitHub Release firmware and regenerate OTA manifest.json on server.

Designed for 1Panel scheduled tasks:
1. Query latest GitHub release.
2. Download firmware asset (firmware-*.bin).
3. Calculate SHA256/size.
4. Generate manifest.json for device OTA checks.
"""

from __future__ import annotations

import argparse
import hashlib
import json
import os
import re
import sys
import tempfile
import urllib.error
import urllib.request
from pathlib import Path
from typing import Any


def log(msg: str) -> None:
    print(f"[OTA_SYNC] {msg}")


def fail(msg: str) -> None:
    print(f"[ERR][OTA_SYNC] {msg}", file=sys.stderr)
    raise RuntimeError(msg)


def fetch_json(url: str, token: str | None, timeout: int) -> dict[str, Any]:
    req = urllib.request.Request(url)
    req.add_header("Accept", "application/vnd.github+json")
    req.add_header("User-Agent", "openbread-ota-sync")
    if token:
        req.add_header("Authorization", f"Bearer {token}")

    with urllib.request.urlopen(req, timeout=timeout) as resp:
        if resp.status != 200:
            fail(f"GitHub API returned HTTP {resp.status}")
        raw = resp.read().decode("utf-8")
    return json.loads(raw)


def parse_build_from_release(release: dict[str, Any]) -> int:
    body = str(release.get("body") or "")
    name = str(release.get("name") or "")
    tag = str(release.get("tag_name") or "")
    source = "\n".join((body, name, tag))
    match = re.search(r"(?:^|\b)build\s*[:=]\s*(\d+)(?:\b|$)", source, flags=re.IGNORECASE | re.MULTILINE)
    if not match:
        fail("Cannot find build number in release. Add 'build: <int>' in release body/title/tag.")
    return int(match.group(1))


def parse_version_from_release(release: dict[str, Any], firmware_name: str) -> str:
    tag = str(release.get("tag_name") or "").strip()
    if tag:
        return tag[1:] if tag.startswith("v") else tag

    match = re.search(r"firmware-([0-9A-Za-z._-]+)\.bin$", firmware_name)
    if match:
        return match.group(1)
    fail("Cannot infer version from release tag or firmware filename.")
    return ""


def choose_asset(release: dict[str, Any], asset_regex: str) -> dict[str, Any]:
    assets = release.get("assets") or []
    if not isinstance(assets, list) or not assets:
        fail("Release has no assets.")

    regex = re.compile(asset_regex)
    for asset in assets:
        name = str(asset.get("name") or "")
        if regex.match(name):
            return asset
    fail(f"No asset matched regex: {asset_regex}")
    return {}


def download_file(url: str, dst: Path, token: str | None, timeout: int) -> None:
    req = urllib.request.Request(url)
    req.add_header("User-Agent", "openbread-ota-sync")
    if token:
        req.add_header("Authorization", f"Bearer {token}")
    with urllib.request.urlopen(req, timeout=timeout) as resp:
        if resp.status != 200:
            fail(f"Download failed HTTP {resp.status} url={url}")
        with dst.open("wb") as f:
            while True:
                chunk = resp.read(1024 * 1024)
                if not chunk:
                    break
                f.write(chunk)


def sha256_file(path: Path) -> str:
    h = hashlib.sha256()
    with path.open("rb") as f:
        for chunk in iter(lambda: f.read(1024 * 1024), b""):
            h.update(chunk)
    return h.hexdigest()


def first_release_note_line(release: dict[str, Any]) -> str:
    body = str(release.get("body") or "")
    for line in body.splitlines():
        text = line.strip()
        if text:
            return text
    return "Release published from GitHub."


def write_json_atomic(path: Path, payload: dict[str, Any]) -> None:
    path.parent.mkdir(parents=True, exist_ok=True)
    with tempfile.NamedTemporaryFile("w", encoding="utf-8", dir=str(path.parent), delete=False) as tmp:
        json.dump(payload, tmp, indent=2, ensure_ascii=False)
        tmp.write("\n")
        tmp_path = Path(tmp.name)
    tmp_path.replace(path)
    os.chmod(path, 0o755)


def main() -> int:
    parser = argparse.ArgumentParser()
    parser.add_argument("--repo", required=True, help="GitHub repo in owner/name format")
    parser.add_argument("--product", required=True)
    parser.add_argument("--channel", default="stable")
    parser.add_argument("--output-dir", required=True, help="Server OTA directory")
    parser.add_argument("--public-base-url", required=True)
    parser.add_argument("--asset-regex", default=r"^firmware-.*\.bin$")
    parser.add_argument("--github-token", default=os.getenv("GITHUB_TOKEN", ""))
    parser.add_argument("--timeout", type=int, default=30)
    args = parser.parse_args()

    if "/" not in args.repo:
        fail("--repo must be owner/name")

    output_dir = Path(args.output_dir)
    output_dir.mkdir(parents=True, exist_ok=True)

    release_url = f"https://api.github.com/repos/{args.repo}/releases/latest"
    log(f"fetch latest release: {release_url}")
    release = fetch_json(release_url, args.github_token or None, args.timeout)

    asset = choose_asset(release, args.asset_regex)
    asset_name = str(asset.get("name") or "")
    asset_url = str(asset.get("browser_download_url") or "")
    if not asset_name or not asset_url:
        fail("Selected asset missing name or download url.")

    firmware_path = output_dir / asset_name
    log(f"download asset: {asset_name}")
    download_file(asset_url, firmware_path, args.github_token or None, args.timeout)
    os.chmod(firmware_path, 0o755)

    version = parse_version_from_release(release, asset_name)
    build = parse_build_from_release(release)
    base_url = args.public_base_url.rstrip("/")
    manifest = {
        "product": args.product,
        "channel": args.channel,
        "version": version,
        "build": build,
        "firmware_url": f"{base_url}/{asset_name}",
        "sha256": sha256_file(firmware_path),
        "size": firmware_path.stat().st_size,
        "release_note": first_release_note_line(release),
    }

    manifest_path = output_dir / "manifest.json"
    write_json_atomic(manifest_path, manifest)
    log(f"manifest updated: {manifest_path}")
    log(f"version={version} build={build} size={manifest['size']}")
    return 0


if __name__ == "__main__":
    try:
        raise SystemExit(main())
    except urllib.error.HTTPError as e:
        fail(f"HTTP error: {e.code} {e.reason}")
    except urllib.error.URLError as e:
        fail(f"Network error: {e.reason}")
    except Exception as e:  # pylint: disable=broad-except
        fail(str(e))
