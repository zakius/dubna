#!/usr/bin/env python3
# SPDX-License-Identifier: GPL-2.0-only OR GPL-3.0-only
"""Prints the objects OUT compiles from our C++ files, for a product's build to
compile first: an error in our code then fails the build minutes in, not hours.

Usage: ours.py OUT SOURCE...
OUT is an output dir in the tree, after gn gen. A SOURCE is a dir of new files
(dubna/src, a product's src/; its .cc and .mm files) or a patch (the files it
changes). Objects (.o, or .obj for Windows) of OUT's default toolchain only.
"""
import re, sys
from pathlib import Path


def main():
    if len(sys.argv) < 3:
        sys.exit(__doc__)
    files = set()
    for source in map(Path, sys.argv[2:]):
        if source.is_dir():
            files.update(str(f.relative_to(source)) for f in source.rglob('*')
                         if f.suffix in ('.cc', '.mm'))
        else:
            files.update(re.findall(r'(?m)^\+\+\+ b/(\S+\.(?:cc|mm))$', source.read_text()))
    sources = {f'../../{f}' for f in files}
    objects = []
    # gn writes a .ninja file per target; non-UTF-8 bytes come from some flags.
    for ninja in Path(sys.argv[1], 'obj').rglob('*.ninja'):
        for m in re.finditer(r'(?m)^build (\S+\.o(?:bj)?)(?: \S+)*: \w+ (\S+)', ninja.read_text(errors='replace')):
            if m[2] in sources:
                objects.append(m[1])
    if objects:
        print('\n'.join(sorted(objects)))


if __name__ == '__main__':
    main()
