#!/usr/bin/env bash
set -euo pipefail

SCRIPT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
BUILD_DIR="$SCRIPT_DIR/build"

usage() {
    echo "Usage: $0 [--build] [--run] [--clean] [--quiet]"
    echo ""
    echo "  (no flags)  Generate build files with cmake"
    echo "  --build     Build with ninja"
    echo "  --run       Run in a nested Wayland session"
    echo "  --clean     Delete the build directory"
    echo "  --quiet     Suppress all output except errors"
    exit 1
}

do_generate=false
do_build=false
do_run=false
do_clean=false
quiet=false

if [[ $# -eq 0 ]]; then
    do_generate=true
fi

for arg in "$@"; do
    case "$arg" in
        --build)   do_build=true ;;
        --run)     do_run=true ;;
        --clean)   do_clean=true ;;
        --quiet)   quiet=true ;;
        --help|-h) usage ;;
        *) echo "Unknown flag: $arg"; usage ;;
    esac
done

log() {
    $quiet || echo "$@"
}

# Redirect stdout to /dev/null in quiet mode; stderr always visible.
if $quiet; then
    exec 3>&1 1>/dev/null
fi

if $do_clean; then
    log "Deleting $BUILD_DIR ..."
    rm -rf "$BUILD_DIR"
    log "Done."
    exit 0
fi

if $do_generate; then
    log "Generating build files ..."
    cmake -B "$BUILD_DIR" -G Ninja -DCMAKE_BUILD_TYPE=Debug -S "$SCRIPT_DIR"
fi

if $do_build; then
    if [[ ! -d "$BUILD_DIR" ]]; then
        log "Build directory not found. Generating first ..."
        cmake -B "$BUILD_DIR" -G Ninja -DCMAKE_BUILD_TYPE=Debug -S "$SCRIPT_DIR"
    fi
    log "Building ..."
    cmake --build "$BUILD_DIR" -j"$(nproc)"
fi

if $do_run; then
    if [[ ! -f "$BUILD_DIR/bin/kwin_wayland" ]]; then
        echo "Error: $BUILD_DIR/bin/kwin_wayland not found. Build first with --build." >&2
        exit 1
    fi
    # Restore stdout for the session so konsole has a usable terminal.
    $quiet && exec 1>&3
    cd "$BUILD_DIR"
    source prefix.sh
    cd bin
    log "Starting nested KWin session ..."
    exec env QT_PLUGIN_PATH="$(pwd):${QT_PLUGIN_PATH:-}" \
        dbus-run-session ./kwin_wayland --xwayland konsole
fi
