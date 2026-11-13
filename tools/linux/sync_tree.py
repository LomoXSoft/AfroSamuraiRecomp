#!/usr/bin/env python3
"""Make <dest> hold the same source files as <src> (Linux counterpart of
Sync-SourceTree in tools/install.ps1).

usage: sync_tree.py <src> <dest> [keep ...]

Equal files are not written, so their timestamps stay older than the build
outputs and Ninja compiles only what changed. A written file gets the current
time. Paths in keep (build folder, codegen output, game-derived files) are
neither written nor removed. Anything named .git, symlinks and disc images
(.iso, .xex, .pe) are skipped.
"""
import os
import shutil
import sys
import time

SKIP_EXT = ('.iso', '.xex', '.pe')


def tree(base, keep):
    out = []
    for root, dirs, files in os.walk(base):
        rel_root = os.path.relpath(root, base)
        rel_root = '' if rel_root == '.' else rel_root
        dirs[:] = [d for d in dirs
                   if d != '.git'
                   and not os.path.islink(os.path.join(root, d))
                   and not kept(os.path.join(rel_root, d), keep)]
        for f in files:
            rel = os.path.join(rel_root, f)
            if f == '.git' or kept(rel, keep) or os.path.islink(os.path.join(root, f)):
                continue
            if f.lower().endswith(SKIP_EXT):
                continue
            out.append(rel)
    return out


def kept(rel, keep):
    return any(rel == k or rel.startswith(k + os.sep) for k in keep)


def same(a, b):
    try:
        if os.path.getsize(a) != os.path.getsize(b):
            return False
    except OSError:
        return False
    with open(a, 'rb') as fa, open(b, 'rb') as fb:
        while True:
            x, y = fa.read(1 << 20), fb.read(1 << 20)
            if x != y:
                return False
            if not x:
                return True


def main():
    if len(sys.argv) < 3:
        sys.exit(__doc__)
    src, dest = os.path.abspath(sys.argv[1]), os.path.abspath(sys.argv[2])
    keep = [k.replace('/', os.sep).rstrip(os.sep) for k in sys.argv[3:]]
    if dest == src:
        print('source sync: source and destination are the same folder')
        return
    if dest.startswith(src + os.sep):
        sys.exit('The install folder is inside the source folder. Choose another install folder.')
    os.makedirs(dest, exist_ok=True)
    wanted = set(tree(src, keep))
    changed = unchanged = removed = 0
    now = time.time()
    for rel in sorted(wanted):
        a, b = os.path.join(src, rel), os.path.join(dest, rel)
        if same(a, b):
            unchanged += 1
            continue
        os.makedirs(os.path.dirname(b), exist_ok=True)
        shutil.copyfile(a, b)
        shutil.copymode(a, b)
        os.utime(b, (now, now))
        changed += 1
    top_git = os.path.join(dest, '.git')
    if os.path.isdir(top_git) and not os.path.islink(top_git):
        shutil.rmtree(top_git)
        removed += 1
    for rel in tree(dest, keep):
        if rel not in wanted:
            os.remove(os.path.join(dest, rel))
            removed += 1
    print('source sync: %d changed, %d unchanged, %d removed' % (changed, unchanged, removed))


if __name__ == '__main__':
    main()
