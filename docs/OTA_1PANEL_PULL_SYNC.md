# OTA Server Pull Sync (1Panel)

This is the mode you requested:

- You manually publish a GitHub Release.
- 1Panel server automatically pulls latest Release firmware.
- Server computes `sha256` and `size` automatically.
- Server rewrites `manifest.json` automatically.

## Script

Use this script from this repo:

- `tools/ota/pull_release_to_manifest.py`

## Release Convention (Important)

1. Release asset must include firmware bin named:
- `firmware-<version>.bin`

Example:
- `firmware-0.1.2.bin`

2. Release body (or title/tag) must include build number:
- `build: 3`

This build is used for OTA version decision.

## 1Panel Scheduled Task Example

Add a scheduled task (for example every 5 minutes):

```bash
python3 /opt/openbread-ota/scripts/pull_release_to_manifest.py \
  --repo YOUR_GITHUB_OWNER/YOUR_REPO \
  --product openbread-normal-one \
  --channel stable \
  --output-dir /opt/openbread-ota/data/ota/openbread-normal-one \
  --public-base-url https://ota.openbread.net/ota/openbread-normal-one \
  --asset-regex '^firmware-.*\.bin$'
```

If your repo is private or to avoid GitHub API rate limits:

```bash
export GITHUB_TOKEN=ghp_xxx
python3 /opt/openbread-ota/scripts/pull_release_to_manifest.py ...
```

## Suggested Server Layout

```txt
/opt/openbread-ota/
  data/
    ota/
      openbread-normal-one/
        manifest.json
        firmware-0.1.1.bin
        firmware-0.1.2.bin
  scripts/
    pull_release_to_manifest.py
```

## Setup Steps

1. Copy script to server:
- `/opt/openbread-ota/scripts/pull_release_to_manifest.py`

2. Make script executable (optional):

```bash
chmod +x /opt/openbread-ota/scripts/pull_release_to_manifest.py
```

3. Create 1Panel scheduled task with the command above.

4. Publish a new GitHub Release manually.

5. Check task log for:
- `[OTA_SYNC] manifest updated ...`

6. Verify URL:
- `https://ota.openbread.net/ota/openbread-normal-one/manifest.json`

## Failure Diagnostics

- `Cannot find build number...`
  - Add `build: <int>` in release body/title/tag.
- `No asset matched regex...`
  - Ensure release asset name matches `firmware-*.bin`.
- `HTTP error 403/404`
  - Check repo visibility, token, owner/repo name.

## Note

This flow is server-pull based and does not require GitHub Actions deployment.
