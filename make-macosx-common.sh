#!/bin/bash

macosx_enter_repo_root() {
    cd "$(dirname "$1")"
    if [ ! -f Makefile ]; then
        echo "This script must be run from the ioquake3 build directory"
        exit 1
    fi
}

macosx_validate_arch() {
    case "$1" in
        x86|x86_64|ppc|arm64)
            return 0
            ;;
        *)
            return 1
            ;;
    esac
}

macosx_detect_host_arch() {
    local arch

    arch="$(uname -m)"
    if [ "$arch" = "i386" ]; then
        arch="x86"
    fi

    printf '%s\n' "$arch"
}

macosx_detect_ncpu() {
    local sysctl_path

    sysctl_path="$(command -v sysctl 2> /dev/null)"
    if [ -n "$sysctl_path" ]; then
        sysctl -n hw.ncpu
    else
        nproc
    fi
}

macosx_run_make() {
    local arch="$1"
    local cflags="$2"
    local macosx_version_min="$3"
    local cc="$4"
    local ncpu

    ncpu="$(macosx_detect_ncpu)"

    if [ -n "$cc" ]; then
        (PLATFORM=darwin ARCH="$arch" CC="$cc" CFLAGS="$cflags" MACOSX_VERSION_MIN="$macosx_version_min" make -j"$ncpu") || exit 1
    else
        (PLATFORM=darwin ARCH="$arch" CFLAGS="$cflags" MACOSX_VERSION_MIN="$macosx_version_min" make -j"$ncpu") || exit 1
    fi
}

macosx_invoke_app_bundle() {
    local target="$1"
    local arch="$2"

    if [ -n "$arch" ]; then
        "./make-macosx-app.sh" "$target" "$arch"
    else
        "./make-macosx-app.sh" "$target"
    fi
}
