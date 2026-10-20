#!/usr/bin/env bash
set -euo pipefail

# Pull latest GitHub release firmware asset, then regenerate manifest.json.
# Dependencies: bash, curl, jq, sha256sum, grep, sed, stat

log() { echo "[OTA_SYNC] $*"; }
err() { echo "[ERR][OTA_SYNC] $*" >&2; }

usage() {
  cat <<'EOF'
Usage:
  pull_release_to_manifest.sh \
    --repo owner/name \
    --product openbread-normal-one \
    --channel stable \
    --output-dir /opt/openbread-ota/data/ota/openbread-normal-one \
    --public-base-url https://ota.openbread.net/ota/openbread-normal-one \
    [--asset-regex '^firmware-.*\.bin$'] \
    [--github-token <token>]
EOF
}

require_cmd() {
  command -v "$1" >/dev/null 2>&1 || {
    err "Missing required command: $1"
    exit 1
  }
}

parse_build() {
  local text="$1"
  local b
  b="$(printf '%s\n' "$text" | grep -Eio 'build[[:space:]]*[:=][[:space:]]*[0-9]+' | head -n1 | grep -Eo '[0-9]+' || true)"
  printf '%s' "$b"
}

parse_version_from_tag() {
  local tag="$1"
  if [[ -n "$tag" ]]; then
    if [[ "$tag" =~ ^v(.+)$ ]]; then
      printf '%s' "${BASH_REMATCH[1]}"
    else
      printf '%s' "$tag"
    fi
  fi
}

parse_version_from_filename() {
  local name="$1"
  local v
  v="$(printf '%s' "$name" | sed -nE 's/^firmware-([0-9A-Za-z._-]+)\.bin$/\1/p')"
  printf '%s' "$v"
}

REPO=""
PRODUCT=""
CHANNEL="stable"
OUTPUT_DIR=""
PUBLIC_BASE_URL=""
ASSET_REGEX='^firmware-.*\.bin$'
GITHUB_TOKEN="${GITHUB_TOKEN:-}"

while [[ $# -gt 0 ]]; do
  case "$1" in
    --repo) REPO="$2"; shift 2 ;;
    --product) PRODUCT="$2"; shift 2 ;;
    --channel) CHANNEL="$2"; shift 2 ;;
    --output-dir) OUTPUT_DIR="$2"; shift 2 ;;
    --public-base-url) PUBLIC_BASE_URL="$2"; shift 2 ;;
    --asset-regex) ASSET_REGEX="$2"; shift 2 ;;
    --github-token) GITHUB_TOKEN="$2"; shift 2 ;;
    -h|--help) usage; exit 0 ;;
    *) err "Unknown arg: $1"; usage; exit 1 ;;
  esac
done

if [[ -z "$REPO" || -z "$PRODUCT" || -z "$OUTPUT_DIR" || -z "$PUBLIC_BASE_URL" ]]; then
  usage
  exit 1
fi

require_cmd curl
require_cmd jq
require_cmd sha256sum
require_cmd grep
require_cmd sed
require_cmd stat

mkdir -p "$OUTPUT_DIR"

api_url="https://api.github.com/repos/${REPO}/releases/latest"
headers=(-H "Accept: application/vnd.github+json" -H "User-Agent: openbread-ota-sync-shell")
if [[ -n "$GITHUB_TOKEN" ]]; then
  headers+=(-H "Authorization: Bearer ${GITHUB_TOKEN}")
fi

log "fetch latest release: ${api_url}"
release_json="$(curl -fsSL "${headers[@]}" "$api_url")"

asset_name="$(printf '%s' "$release_json" | jq -r --arg re "$ASSET_REGEX" '.assets[]? | select(.name | test($re)) | .name' | head -n1)"
asset_url="$(printf '%s' "$release_json" | jq -r --arg re "$ASSET_REGEX" '.assets[]? | select(.name | test($re)) | .browser_download_url' | head -n1)"
tag_name="$(printf '%s' "$release_json" | jq -r '.tag_name // ""')"
release_name="$(printf '%s' "$release_json" | jq -r '.name // ""')"
release_body="$(printf '%s' "$release_json" | jq -r '.body // ""')"

if [[ -z "$asset_name" || "$asset_name" == "null" || -z "$asset_url" || "$asset_url" == "null" ]]; then
  err "No release asset matched regex: $ASSET_REGEX"
  exit 1
fi

firmware_path="${OUTPUT_DIR}/${asset_name}"
log "download asset: ${asset_name}"
curl -fsSL "${headers[@]}" "$asset_url" -o "$firmware_path"
chmod 0755 "$firmware_path"

sha256="$(sha256sum "$firmware_path" | awk '{print $1}')"
size="$(stat -c '%s' "$firmware_path")"

version="$(parse_version_from_tag "$tag_name")"
if [[ -z "$version" ]]; then
  version="$(parse_version_from_filename "$asset_name")"
fi
if [[ -z "$version" ]]; then
  err "Cannot infer version from tag or filename. tag='${tag_name}', asset='${asset_name}'"
  exit 1
fi

build="$(parse_build "${release_body}
${release_name}
${tag_name}")"
if [[ -z "$build" ]]; then
  err "Cannot find build number. Add 'build: <int>' to release body/name/tag."
  exit 1
fi

base_url="${PUBLIC_BASE_URL%/}"
firmware_url="${base_url}/${asset_name}"
manifest_tmp="$(mktemp)"
manifest_path="${OUTPUT_DIR}/manifest.json"

jq -n --arg product "$PRODUCT" --arg channel "$CHANNEL" --arg version "$version" \
  --arg firmware_url "$firmware_url" --arg sha256 "$sha256" \
  --argjson build "$build" --argjson size "$size" --arg body "$release_body" '
  def limited($bytes):
    reduce (explode[]) as $cp ({text:"", full:false};
      ([$cp] | implode) as $char |
      if .full or ((.text + $char) | utf8bytelength) > $bytes then .full = true
      else .text += $char end) | .text;
  ($body | split("\n") | map(gsub("^\\s+|\\s+$"; "")) | map(select(length > 0))) as $lines |
  (($lines[0] // "Release published from GitHub.") | sub("^[-*]\\s+"; "") | gsub("[\"\\\\\\x00-\\x1f]"; "") | limited(95)) as $summary |
  ($lines | map(select(startswith("#") | not)) |
    map({bullet:test("^[-*]\\s+"), text:(sub("^[-*]\\s+"; "") | gsub("`"; ""))}) |
    map(select(.text | test("^(build\\s*[:=]|sha256\\b|RAM\\b|Flash\\b|pio\\s|https?://|固件附件)"; "i") | not))) as $notes |
  ($notes | map(select(.bullet)) | map(.text)) as $bullets |
  ((if ($bullets | length) > 0 then $bullets else ($notes | map(.text)) end) | join("\n") | limited(767)) as $full |
  {product:$product, channel:$channel, version:$version, build:$build,
   firmware_url:$firmware_url, sha256:$sha256, size:$size,
   release_note:(if $summary == "" then "Release published from GitHub." else $summary end),
   release_notes:(if $full == "" then $summary else $full end)}
' > "$manifest_tmp"

mv "$manifest_tmp" "$manifest_path"
chmod 0755 "$manifest_path"
log "manifest updated: ${manifest_path}"
log "version=${version} build=${build} size=${size}"
