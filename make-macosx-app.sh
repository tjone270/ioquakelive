#!/bin/bash
# Create a macOS .app bundle for Quake Live

if [ $# -lt 1 ] || [ $# -gt 2 ]; then
	echo "Usage: $0 <release|debug> [arch]"
	exit 1
fi

TARGET_NAME="$1"
ARCH="${2:-$(uname -m)}"
OBJROOT="build"
BUILT_PRODUCTS_DIR="${OBJROOT}/${TARGET_NAME}-darwin-${ARCH}"

if [ ! -d "${BUILT_PRODUCTS_DIR}" ]; then
	echo "Build directory not found: ${BUILT_PRODUCTS_DIR}"
	exit 1
fi

PRODUCT_NAME="quakelive"
WRAPPER_NAME="${PRODUCT_NAME}.app"
CONTENTS="${BUILT_PRODUCTS_DIR}/${WRAPPER_NAME}/Contents"
MACOS="${CONTENTS}/MacOS"
RESOURCES="${CONTENTS}/Resources"
BASEDIR="baseq3"

# Check for client binary
CLIENT="${BUILT_PRODUCTS_DIR}/${PRODUCT_NAME}.${ARCH}"
if [ ! -f "${CLIENT}" ]; then
	echo "Client binary not found: ${CLIENT}"
	exit 1
fi

echo "Creating ${WRAPPER_NAME} for ${ARCH}..."

# Create bundle structure
mkdir -p "${MACOS}"
mkdir -p "${RESOURCES}"

# Copy executables
cp "${CLIENT}" "${MACOS}/${PRODUCT_NAME}"
if [ -f "${BUILT_PRODUCTS_DIR}/quakelive_dedicated.${ARCH}" ]; then
	cp "${BUILT_PRODUCTS_DIR}/quakelive_dedicated.${ARCH}" "${MACOS}/quakelive_dedicated"
fi

# Copy renderer (engine loads cl_renderer + ARCH_STRING + DLL_EXT, e.g. opengl2arm64.dylib)
if [ -f "${BUILT_PRODUCTS_DIR}/opengl2${ARCH}.dylib" ]; then
	cp "${BUILT_PRODUCTS_DIR}/opengl2${ARCH}.dylib" "${MACOS}/opengl2${ARCH}.dylib"
fi

# Bundle SDL2 into Contents/Frameworks/ and rewrite the load path.
# The binary links against SDL2 via its Homebrew/system install name (an
# absolute path); macOS hardened-runtime rejects loading a non-platform dylib
# whose Team ID doesn't match the app's.  Bundling SDL2 and signing it with
# the same Developer ID certificate is the correct fix.
FRAMEWORKS="${CONTENTS}/Frameworks"
mkdir -p "${FRAMEWORKS}"

# Locate the SDL2 dylib that the linker used (try build dir, then common locations).
SDL2_SRC=""
for candidate in \
    "${BUILT_PRODUCTS_DIR}/libSDL2-2.0.0.dylib" \
    "$(brew --prefix sdl2 2>/dev/null)/lib/libSDL2-2.0.0.dylib" \
    /opt/homebrew/lib/libSDL2-2.0.0.dylib \
    /usr/local/lib/libSDL2-2.0.0.dylib; do
    if [ -f "$candidate" ]; then
        SDL2_SRC="$candidate"
        break
    fi
done

if [ -n "$SDL2_SRC" ]; then
    SDL2_DST="${FRAMEWORKS}/libSDL2-2.0.0.dylib"
    cp "$SDL2_SRC" "$SDL2_DST"
    # Set the install name inside the bundled copy so dyld resolves it correctly
    # when any binary in the bundle loads it.
    install_name_tool -id "@rpath/libSDL2-2.0.0.dylib" "$SDL2_DST"

    # Rewrite the SDL2 load path in every Mach-O binary we placed in the bundle.
    NEW_SDL2="@executable_path/../Frameworks/libSDL2-2.0.0.dylib"
    for bin in \
        "${MACOS}/${PRODUCT_NAME}" \
        "${MACOS}/quakelive_dedicated" \
        "${MACOS}/opengl2${ARCH}.dylib"; do
        [ -f "$bin" ] || continue
        OLD_SDL2=$(otool -L "$bin" 2>/dev/null | awk '/libSDL2/{print $1; exit}')
        if [ -n "$OLD_SDL2" ] && [ "$OLD_SDL2" != "$NEW_SDL2" ]; then
            install_name_tool -change "$OLD_SDL2" "$NEW_SDL2" "$bin"
            echo "  Rewrote SDL2 path in $(basename "$bin")"
        fi
    done
    echo "Bundled SDL2 from $SDL2_SRC"
else
    echo "Warning: SDL2 dylib not found; app may fail to launch on machines without Homebrew"
fi

# Generate .icns from quakelive.ico
ICNS_NAME="quakelive.icns"
ICO_SRC="misc/quakelive.ico"
if [ -f "${ICO_SRC}" ] && command -v sips &>/dev/null && command -v iconutil &>/dev/null; then
	ICONSET=$(mktemp -d)/quakelive.iconset
	mkdir -p "${ICONSET}"

	# Extract the largest image from the ico and resize to all required sizes
	sips -s format png "${ICO_SRC}" --out "${ICONSET}/icon_512x512@2x.png" &>/dev/null
	for SIZE in 16 32 128 256 512; do
		sips -z ${SIZE} ${SIZE} "${ICONSET}/icon_512x512@2x.png" --out "${ICONSET}/icon_${SIZE}x${SIZE}.png" &>/dev/null
	done
	for SIZE in 16 32 128 256; do
		DOUBLE=$((SIZE * 2))
		sips -z ${DOUBLE} ${DOUBLE} "${ICONSET}/icon_512x512@2x.png" --out "${ICONSET}/icon_${SIZE}x${SIZE}@2x.png" &>/dev/null
	done

	iconutil -c icns "${ICONSET}" -o "${RESOURCES}/${ICNS_NAME}" 2>/dev/null
	rm -rf "$(dirname "${ICONSET}")"
	echo "Generated ${ICNS_NAME} from ${ICO_SRC}"
elif [ -f "misc/quakelive.icns" ]; then
	cp "misc/quakelive.icns" "${RESOURCES}/${ICNS_NAME}"
else
	echo "Warning: No icon source found, .app will have no icon"
	ICNS_NAME=""
fi

# PkgInfo
echo -n "APPLQLIV" > "${CONTENTS}/PkgInfo"

# Get version
VERSION=$(grep '^VERSION=' Makefile 2>/dev/null | head -1 | sed 's/VERSION=//')
[ -z "$VERSION" ] && VERSION=$(git describe --always 2>/dev/null || echo "dev")

# Info.plist
cat > "${CONTENTS}/Info.plist" << PLIST
<?xml version="1.0" encoding="UTF-8"?>
<!DOCTYPE plist PUBLIC "-//Apple//DTD PLIST 1.0//EN" "http://www.apple.com/DTDs/PropertyList-1.0.dtd">
<plist version="1.0">
<dict>
    <key>CFBundleDevelopmentRegion</key>
    <string>en</string>
    <key>CFBundleExecutable</key>
    <string>${PRODUCT_NAME}</string>
    <key>CFBundleIconFile</key>
    <string>${ICNS_NAME}</string>
    <key>CFBundleIdentifier</key>
    <string>com.tjone270.${PRODUCT_NAME}</string>
    <key>CFBundleInfoDictionaryVersion</key>
    <string>6.0</string>
    <key>CFBundleName</key>
    <string>Quake Live</string>
    <key>CFBundlePackageType</key>
    <string>APPL</string>
    <key>CFBundleShortVersionString</key>
    <string>${VERSION}</string>
    <key>CFBundleVersion</key>
    <string>${VERSION}</string>
    <key>CGDisableCoalescedUpdates</key>
    <true/>
    <key>LSMinimumSystemVersion</key>
    <string>11.0</string>
    <key>NSHighResolutionCapable</key>
    <true/>
    <key>NSRequiresAquaSystemAppearance</key>
    <false/>
    <key>NSHumanReadableCopyright</key>
    <string>Copyright © 2015 id Software LLC, a ZeniMax Media company. All rights reserved.</string>
    <key>NSPrincipalClass</key>
    <string>NSApplication</string>
</dict>
</plist>
PLIST

echo "Created ${BUILT_PRODUCTS_DIR}/${WRAPPER_NAME}"
