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

# Copy SDL2 dylib if present
for dylib in "${BUILT_PRODUCTS_DIR}"/*.dylib; do
	name=$(basename "$dylib")
	case "$name" in
		libSDL2*|SDL2*) cp "$dylib" "${MACOS}/" ;;
	esac
done

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
