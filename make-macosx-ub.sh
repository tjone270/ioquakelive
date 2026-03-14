#!/bin/bash

source "$(dirname "$0")/make-macosx-common.sh"

CC=gcc-4.0

macosx_enter_repo_root "$0"

# we want to use the oldest available SDK for max compatibility. However 10.4 and older
# can not build 64bit binaries, making 10.5 the minimum version.   This has been tested 
# with xcode 3.1 (xcode31_2199_developerdvd.dmg).  It contains the 10.5 SDK and a decent
# enough gcc to actually compile ioquake3
# For PPC macs, G4's or better are required to run ioquake3.

unset X86_64_SDK
unset X86_64_CFLAGS
unset X86_64_MACOSX_VERSION_MIN
unset X86_SDK
unset X86_CFLAGS
unset X86_MACOSX_VERSION_MIN
unset PPC_SDK
unset PPC_CFLAGS
unset PPC_MACOSX_VERSION_MIN

if [ -d /Developer/SDKs/MacOSX10.5.sdk ]; then
	X86_64_SDK=/Developer/SDKs/MacOSX10.5.sdk
	X86_64_CFLAGS="-isysroot /Developer/SDKs/MacOSX10.5.sdk"
	X86_64_MACOSX_VERSION_MIN="10.5"

	X86_SDK=/Developer/SDKs/MacOSX10.5.sdk
	X86_CFLAGS="-isysroot /Developer/SDKs/MacOSX10.5.sdk"
	X86_MACOSX_VERSION_MIN="10.5"

	PPC_SDK=/Developer/SDKs/MacOSX10.5.sdk
	PPC_CFLAGS="-isysroot /Developer/SDKs/MacOSX10.5.sdk"
	PPC_MACOSX_VERSION_MIN="10.5"
fi

# SDL 2.0.5+ (x86, x86_64) only supports MacOSX 10.6 and later
if [ -d /Developer/SDKs/MacOSX10.6.sdk ]; then
	X86_64_SDK=/Developer/SDKs/MacOSX10.6.sdk
	X86_64_CFLAGS="-isysroot /Developer/SDKs/MacOSX10.6.sdk"
	X86_64_MACOSX_VERSION_MIN="10.6"

	X86_SDK=/Developer/SDKs/MacOSX10.6.sdk
	X86_CFLAGS="-isysroot /Developer/SDKs/MacOSX10.6.sdk"
	X86_MACOSX_VERSION_MIN="10.6"
else
	# Don't try to compile with 10.5 version min
	X86_64_SDK=
	X86_SDK=
fi
# end SDL 2.0.5

if [ -z $X86_64_SDK ] || [ -z $X86_SDK ] || [ -z $PPC_SDK ]; then
	echo "\
ERROR: This script is for building a Universal Binary.  You cannot build
       for a different architecture unless you have the proper Mac OS X SDKs
       installed.  If you just want to to compile for your own system run
       'make-macosx.sh' instead of this script.

       In order to build a binary with maximum compatibility you must
       build on Mac OS X 10.6 and have the MacOSX10.5 and MacOSX10.6
       SDKs installed from the Xcode install disk Packages folder."

	exit 1
fi

echo "Building X86_64 Client/Dedicated Server against \"$X86_64_SDK\""
echo "Building X86 Client/Dedicated Server against \"$X86_SDK\""
echo "Building PPC Client/Dedicated Server against \"$PPC_SDK\""
echo

macosx_run_make "x86_64" "$X86_64_CFLAGS" "$X86_64_MACOSX_VERSION_MIN" "$CC"

echo;echo

macosx_run_make "x86" "$X86_CFLAGS" "$X86_MACOSX_VERSION_MIN" "$CC"

echo;echo

macosx_run_make "ppc" "$PPC_CFLAGS" "$PPC_MACOSX_VERSION_MIN" "$CC"

echo

# use the following shell script to build a universal application bundle
export MACOSX_DEPLOYMENT_TARGET="10.5"
export MACOSX_DEPLOYMENT_TARGET_PPC="$PPC_MACOSX_VERSION_MIN"
export MACOSX_DEPLOYMENT_TARGET_X86="$X86_MACOSX_VERSION_MIN"
export MACOSX_DEPLOYMENT_TARGET_X86_64="$X86_64_MACOSX_VERSION_MIN"
macosx_invoke_app_bundle release ""
