#!/usr/bin/env bash
set -euo pipefail

SCRIPT_DIR="$(cd "$(dirname "$0")" && pwd)"
PROJECT_ROOT="${AHAMKARA_PROJECT_ROOT:-$(cd "$SCRIPT_DIR/.." && pwd)}"
BUILD_DIR="${1:-$PROJECT_ROOT/build/release}"
OUTPUT_DIR="${2:-$PROJECT_ROOT/build/artifacts}"
PLATFORM="${3:-$(uname -s | tr '[:upper:]' '[:lower:]')-$(uname -m)}"

case "$BUILD_DIR" in
    /*) ;;
    *) BUILD_DIR="$PROJECT_ROOT/$BUILD_DIR" ;;
esac
case "$OUTPUT_DIR" in
    /*) ;;
    *) OUTPUT_DIR="$PROJECT_ROOT/$OUTPUT_DIR" ;;
esac

FLASHBACK_BIN="$BUILD_DIR/samples/flashback/flashback"
ASSET_IMPORTER="$BUILD_DIR/tools/ahamkara_asset_importer"
if [[ ! -x "$FLASHBACK_BIN" || ! -x "$ASSET_IMPORTER" ]]; then
    echo "Missing Flashback or asset importer in $BUILD_DIR; build the release targets first." >&2
    exit 1
fi

cd "$PROJECT_ROOT"
"$ASSET_IMPORTER" --manifest assets/manifest.assets
ASSET_REGISTRY="$PROJECT_ROOT/assets/compiled/asset_registry.tsv"
if [[ ! -s "$ASSET_REGISTRY" ]]; then
    echo "Asset import completed without an asset registry." >&2
    exit 1
fi

COMMIT="$(git -c safe.directory="$PROJECT_ROOT" -C "$PROJECT_ROOT" rev-parse HEAD)"
VERSION="$(sed -n 's/^project(Ahamkara VERSION \([^ ]*\).*/\1/p' "$PROJECT_ROOT/CMakeLists.txt" | head -n 1)"
if [[ -z "$VERSION" ]]; then
    VERSION="0.0.0"
fi
mkdir -p "$OUTPUT_DIR"
STAGE_DIR="$(mktemp -d "$OUTPUT_DIR/.flashback-package.XXXXXX")"
trap 'rm -rf "$STAGE_DIR"' EXIT
APP_DIR="$STAGE_DIR/Flashback"
mkdir -p "$APP_DIR/bin"
install -m 0755 "$FLASHBACK_BIN" "$APP_DIR/bin/flashback"
cp -a "$PROJECT_ROOT/assets" "$APP_DIR/assets"
if [[ -d "$PROJECT_ROOT/client/config" ]]; then
    mkdir -p "$APP_DIR/client"
    cp -a "$PROJECT_ROOT/client/config" "$APP_DIR/client/config"
fi
cat > "$APP_DIR/run-flashback.sh" <<'LAUNCHER'
#!/usr/bin/env sh
set -eu
APP_DIR="$(CDPATH= cd -- "$(dirname -- "$0")" && pwd)"
cd "$APP_DIR"
exec ./bin/flashback "$@"
LAUNCHER
chmod 0755 "$APP_DIR/run-flashback.sh"
cat > "$APP_DIR/build-info.txt" <<INFO
product=Flashback
version=$VERSION
commit=$COMMIT
platform=$PLATFORM
built_utc=$(date -u '+%Y-%m-%dT%H:%M:%SZ')
INFO

ARCHIVE="$OUTPUT_DIR/Flashback-$PLATFORM-$COMMIT.tar.gz"
tar -czf "$ARCHIVE" -C "$STAGE_DIR" Flashback
if command -v sha256sum >/dev/null 2>&1; then
    (cd "$OUTPUT_DIR" && sha256sum "$(basename "$ARCHIVE")" > "$(basename "$ARCHIVE").sha256")
else
    (cd "$OUTPUT_DIR" && shasum -a 256 "$(basename "$ARCHIVE")" > "$(basename "$ARCHIVE").sha256")
fi
printf '%s\n' "$ARCHIVE"
