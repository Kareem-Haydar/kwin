#!/usr/bin/env bash
set -euo pipefail

SCRIPT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
BUILD_DIR="$SCRIPT_DIR/build"

usage() {
    echo "Usage: $0 [options]"
    echo ""
    echo "  (no flags)       Generate build files with cmake"
    echo ""
    echo "Actions:"
    echo "  -b, --build      Build with ninja"
    echo "  -r, --run        Run in a nested Wayland session"
    echo "  -c, --clean      Delete the build directory"
    echo "  -q, --quiet      Suppress all output except errors"
    echo ""
    echo "Feature flags:"
    echo "  -p, --plasma     Enable Plasma-specific Wayland protocol support"
    echo ""
    echo "Examples:"
    echo "  $0               # configure (no Plasma protocols)"
    echo "  $0 -bp           # build with Plasma protocols"
    echo "  $0 -b -r         # build then run"
    echo "  $0 --build --plasma --run"
    exit 1
}

do_generate=false
do_build=false
do_run=false
do_clean=false
quiet=false
plasma=false

if [[ $# -eq 0 ]]; then
    do_generate=true
fi

for arg in "$@"; do
    case "$arg" in
        --build)   do_build=true ;;
        --run)     do_run=true ;;
        --clean)   do_clean=true ;;
        --quiet)   quiet=true ;;
        --plasma)  plasma=true ;;
        --help)    usage ;;
        -*)
            # Parse combined short flags like -brpq
            flags="${arg#-}"
            if [[ "$flags" == -* ]]; then
                echo "Unknown flag: $arg"; usage
            fi
            for (( i=0; i<${#flags}; i++ )); do
                case "${flags:$i:1}" in
                    b) do_build=true ;;
                    r) do_run=true ;;
                    c) do_clean=true ;;
                    q) quiet=true ;;
                    p) plasma=true ;;
                    h) usage ;;
                    *) echo "Unknown flag: -${flags:$i:1}"; usage ;;
                esac
            done
            ;;
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

CMAKE_FLAGS="-DCMAKE_BUILD_TYPE=Debug"
$plasma && CMAKE_FLAGS="$CMAKE_FLAGS -DKWIN_BUILD_PLASMA_PROTOCOLS=ON"

if $do_generate || $do_build; then
    if [[ ! -d "$BUILD_DIR" ]] || $do_generate; then
        log "Generating build files ..."
        cmake -B "$BUILD_DIR" -G Ninja $CMAKE_FLAGS -S "$SCRIPT_DIR"
    fi
fi

if $do_build; then
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
