#!/usr/bin/env bash
set -euo pipefail

if [[ $# -ne 2 ]]; then
    echo "Usage: $0 <Flashback-<platform>-<commit>.tar.gz> <install-root>" >&2
    exit 2
fi
ARCHIVE="$(cd "$(dirname "$1")" && pwd)/$(basename "$1")"
INSTALL_ROOT="$(mkdir -p "$2" && cd "$2" && pwd)"
if [[ ! -f "$ARCHIVE" ]]; then
    echo "Candidate archive not found: $ARCHIVE" >&2
    exit 1
fi

SIDECAR="$ARCHIVE.sha256"
if [[ -f "$SIDECAR" ]]; then
    if command -v sha256sum >/dev/null 2>&1; then
        (cd "$(dirname "$ARCHIVE")" && sha256sum -c "$(basename "$SIDECAR")")
    else
        (cd "$(dirname "$ARCHIVE")" && shasum -a 256 -c "$(basename "$SIDECAR")")
    fi
fi

python3 - "$ARCHIVE" <<'PY'
import sys
import tarfile
from pathlib import PurePosixPath

with tarfile.open(sys.argv[1], "r:gz") as archive:
    for member in archive.getmembers():
        path = PurePosixPath(member.name)
        if path.is_absolute() or ".." in path.parts or not path.parts or path.parts[0] != "Flashback":
            raise SystemExit(f"Unsafe path in candidate archive: {member.name}")
        if member.issym() or member.islnk():
            raise SystemExit(f"Links are not allowed in candidate archives: {member.name}")
PY

mkdir -p "$INSTALL_ROOT/versions"
STAGING="$INSTALL_ROOT/.staging.$$"
cleanup() { rm -rf "$STAGING"; }
trap cleanup EXIT
mkdir "$STAGING"
tar --no-same-owner -xzf "$ARCHIVE" -C "$STAGING"
APP_DIR="$STAGING/Flashback"
if [[ ! -x "$APP_DIR/bin/flashback" || ! -s "$APP_DIR/assets/compiled/asset_registry.tsv" ]]; then
    echo "Candidate is incomplete: expected executable and compiled asset registry." >&2
    exit 1
fi
COMMIT="$(sed -n 's/^commit=//p' "$APP_DIR/build-info.txt" | head -n 1)"
if [[ ! "$COMMIT" =~ ^[0-9a-f]{40}$ ]]; then
    echo "Candidate has an invalid commit in build-info.txt." >&2
    exit 1
fi

DESTINATION="$INSTALL_ROOT/versions/$COMMIT"
if [[ -e "$DESTINATION" ]]; then
    if [[ ! -x "$DESTINATION/bin/flashback" || ! -s "$DESTINATION/assets/compiled/asset_registry.tsv" ]]; then
        echo "Existing version directory is incomplete: $DESTINATION" >&2
        exit 1
    fi
else
    mv "$APP_DIR" "$DESTINATION"
fi

CURRENT="$INSTALL_ROOT/current"
if [[ -e "$CURRENT" && ! -L "$CURRENT" ]]; then
    echo "Refusing to replace non-symlink current path: $CURRENT" >&2
    exit 1
fi
if [[ -L "$CURRENT" ]]; then
    PREVIOUS="$(readlink "$CURRENT")"
    if [[ "$PREVIOUS" != "versions/$COMMIT" ]]; then
        ln -s "$PREVIOUS" "$INSTALL_ROOT/.backup.$$"
        mv -Tf "$INSTALL_ROOT/.backup.$$" "$INSTALL_ROOT/backup"
    fi
fi
ln -s "versions/$COMMIT" "$INSTALL_ROOT/.current.$$"
mv -Tf "$INSTALL_ROOT/.current.$$" "$CURRENT"
printf 'Promoted Flashback %s; current -> %s\n' "$COMMIT" "$(readlink "$CURRENT")"
