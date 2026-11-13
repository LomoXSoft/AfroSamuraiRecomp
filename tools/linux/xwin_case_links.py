#!/usr/bin/env python3
"""Add symlinks to an xwin splat for headers and libraries the sources name in another case.

Windows file names are case-insensitive, Linux ones are not. xwin already adds
links for many SDK spellings, but source code such as `#include <ObjBase.h>`
or `#pragma comment(lib, "Dbghelp.lib")` can still miss.
usage: xwin_case_links.py <xwin dir> <source dir>...
"""
import os
import re
import sys

INC = re.compile(rb'^\s*#\s*include\s*[<"]([^>"]+)[>"]', re.M)
LIB = re.compile(rb'#\s*pragma\s+comment\s*\(\s*lib\s*,\s*"([^"]+)"', re.I)
CMAKE_LIB = re.compile(rb'\b([A-Za-z0-9_]+)\.lib\b|\b(?:target_link_libraries|link_libraries)\s*\(([^)]*)\)', re.I)
SRC_EXT = ('.c', '.cc', '.cpp', '.cxx', '.h', '.hh', '.hpp', '.hxx', '.inl', '.inc', '.ipp')


def index(dirs):
    """Map lower-case relative path -> set of real relative paths, per directory."""
    out = []
    for d in dirs:
        m = {}
        for root, _, files in os.walk(d):
            for f in files:
                rel = os.path.relpath(os.path.join(root, f), d).replace(os.sep, '/')
                m.setdefault(rel.lower(), set()).add(rel)
        out.append((d, m))
    return out


def wanted(srcs):
    inc, lib = set(), set()
    for s in srcs:
        for root, _, files in os.walk(s):
            if '/.git' in root:
                continue
            for f in files:
                p = os.path.join(root, f)
                low = f.lower()
                try:
                    if low.endswith(SRC_EXT):
                        data = open(p, 'rb').read()
                        inc.update(x.decode('latin-1') for x in INC.findall(data))
                        lib.update(x.decode('latin-1') for x in LIB.findall(data))
                    elif low == 'cmakelists.txt' or low.endswith('.cmake'):
                        data = open(p, 'rb').read()
                        for a, b in CMAKE_LIB.findall(data):
                            if a:
                                lib.add(a.decode('latin-1') + '.lib')
                            for w in re.findall(rb'[A-Za-z0-9_]+', b):
                                lib.add(w.decode('latin-1') + '.lib')
                except OSError:
                    pass
    return inc, lib


def link(indexed, names):
    made = 0
    for name in sorted(names):
        name = name.replace('\\', '/')
        exact = any(os.path.exists(os.path.join(d, name)) for d, _ in indexed)
        if exact:
            continue
        for d, m in indexed:
            real = m.get(name.lower())
            if not real:
                continue
            target = sorted(real)[0]
            dst = os.path.join(d, name)
            os.makedirs(os.path.dirname(dst), exist_ok=True)
            if not os.path.lexists(dst):
                os.symlink(os.path.relpath(os.path.join(d, target), os.path.dirname(dst)), dst)
                made += 1
            break
    return made


def main():
    xwin, srcs = sys.argv[1], sys.argv[2:]
    incdirs = [os.path.join(xwin, 'crt', 'include')] + [
        os.path.join(xwin, 'sdk', 'include', x) for x in ('ucrt', 'um', 'shared', 'winrt')]
    libdirs = [os.path.join(xwin, 'crt', 'lib', 'x86_64')] + [
        os.path.join(xwin, 'sdk', 'lib', x, 'x86_64') for x in ('um', 'ucrt')]
    inc, lib = wanted(srcs)
    a = link(index([d for d in incdirs if os.path.isdir(d)]), inc)
    b = link(index([d for d in libdirs if os.path.isdir(d)]), lib)
    print('xwin case links: %d headers, %d libraries' % (a, b))


if __name__ == '__main__':
    main()
