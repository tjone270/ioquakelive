"""
Assemble per-platform release zips for ioquakelive.

Inputs:
  --shared <dir>    Directory to drop into every release zip as-is. Typically
                    contains baseq3/iobin.pk3 and baseq3/pak01.pk3 (the
                    deterministic universal paks built by the package job).
                    pak00.pk3 is NOT included — end users supply it themselves
                    from their existing Quake Live install.
  --engines <dir>   Parent directory containing one subdirectory per platform
                    artifact, e.g. engines/engine-linux-x86_64/quakelive.x86_64
                    (matches actions/download-artifact layout for the
                    "engine-*" artifacts).
  --output <dir>    Where to write quakelive-<platform>.zip files.

Each per-platform zip ends up with the engine binaries flattened at the root
plus a baseq3/ subdirectory containing whatever is in --shared/baseq3.

The zips themselves are NOT byte-deterministic (different platforms ship
different engine binaries by definition), but every zip's baseq3/iobin.pk3
and baseq3/pak01.pk3 are byte-identical because they came from --shared.
"""
import argparse
import os
import sys
import zipfile


# Maps engine artifact directory name -> output zip basename.
PLATFORM_OUT = {
    'engine-linux-x86_64': 'quakelive-linux-x86_64.zip',
    'engine-windows-x64':  'quakelive-windows-x64.zip',
    'engine-macos-arm64':  'quakelive-macos-arm64.zip',
}


def add_tree(zf, root, dest_prefix):
    """Add every file under root into zf, with paths prefixed by dest_prefix."""
    for dirpath, dirnames, filenames in os.walk(root):
        dirnames.sort()
        for name in sorted(filenames):
            abs_path = os.path.join(dirpath, name)
            rel = os.path.relpath(abs_path, root).replace(os.sep, '/')
            arc = f'{dest_prefix}/{rel}' if dest_prefix else rel
            zf.write(abs_path, arc, zipfile.ZIP_DEFLATED)


def build_one(engine_dir, shared_dir, out_path):
    print(f'building {out_path}')
    with zipfile.ZipFile(out_path, 'w', zipfile.ZIP_DEFLATED, compresslevel=9) as zf:
        # Engine binaries flatten to the root of the zip
        add_tree(zf, engine_dir, '')
        # Shared content (baseq3/...) layered on top
        add_tree(zf, shared_dir, '')


def main():
    ap = argparse.ArgumentParser(description=__doc__)
    ap.add_argument('--shared', required=True, help='dir to inline into every zip (contains baseq3/)')
    ap.add_argument('--engines', required=True, help='parent dir containing engine-<platform>/ subdirs')
    ap.add_argument('--output', required=True, help='dir to write release zips into')
    args = ap.parse_args()

    if not os.path.isdir(args.shared):
        sys.exit(f'shared dir not found: {args.shared}')
    if not os.path.isdir(args.engines):
        sys.exit(f'engines dir not found: {args.engines}')
    os.makedirs(args.output, exist_ok=True)

    found_any = False
    for entry in sorted(os.listdir(args.engines)):
        engine_dir = os.path.join(args.engines, entry)
        if not os.path.isdir(engine_dir):
            continue
        out_basename = PLATFORM_OUT.get(entry)
        if not out_basename:
            print(f'  skipping unknown engine artifact dir: {entry}')
            continue
        out_path = os.path.join(args.output, out_basename)
        build_one(engine_dir, args.shared, out_path)
        size = os.path.getsize(out_path)
        print(f'  wrote {out_path} ({size} bytes)')
        found_any = True

    if not found_any:
        sys.exit('no recognised engine-<platform> artifact dirs were found')


if __name__ == '__main__':
    main()
