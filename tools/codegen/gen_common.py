#!/usr/bin/env python3
# Shared by the code generators' drivers: the function blocks of a generated file, the instruction comments' removal, writes.
import os
import re
import sys

sys.path.insert(0, os.path.dirname(os.path.abspath(__file__)))
import r5900dis  # noqa: E402

DEF = re.compile(r'^(\w+) (\w+)\(([^)]*)\) \{$')


def blocks(text):
    """[(start, end, name)] of the function definitions of a generated file: from its first `// orig:` line to its closing `}`."""
    L = text.split('\n')
    out = []
    i = 0
    while i < len(L):
        m = DEF.match(L[i])
        if m and m.group(2) != 'main':
            s = i
            while s > 0 and L[s - 1].startswith('//') and 'orig:' in L[s - 1]:
                s -= 1
            e = i
            while L[e] != '}':
                e += 1
            out.append((s, e + 1, m.group(2)))
            i = e + 1
        else:
            i += 1
    return out


_CMT = re.compile(r'^(?:\(delay\)\s*)?([a-z][a-z0-9_]*)(?:\.[A-Za-z0-9]+)*(?:\s|$)')


def is_insn_comment(c):
    """Whether a comment's text (after `//`) is an original instruction: `lw a1,0xf0(sp)`, `(delay) addiu ...`, `jal 0x...`,
    `call 0x...`, `.word 0x...`, a VU0 macro op `vmulax.xyzw ...`."""
    c = c.strip()
    if c.startswith('.word '):
        return True
    m = _CMT.match(c)
    if not m:
        return False
    b = m.group(1)
    if b in ('call',):
        return True
    return b in r5900dis.MNEMONICS or (b[-1:] in 'xyzw' and b[:-1] in r5900dis.MNEMONICS and b.startswith('v'))


def _split_comment(line):
    """(code, comment text or None): the first `//` outside a string literal."""
    q = False
    i = 0
    while i < len(line) - 1:
        ch = line[i]
        if ch == '\\' and q:
            i += 2
            continue
        if ch == '"':
            q = not q
        elif not q and line.startswith('//', i):
            return line[:i], line[i + 2:]
        i += 1
    return line, None


def strip_insn_comments(text):
    """The text without the comments that carry an original instruction. A trailing one is cut (its spaces too); a line that is
    only such a comment is dropped. `// orig:` lines and every other comment stay, so the code (and the lint's anchors) are
    unchanged: the object code is the same."""
    out = []
    for line in text.split('\n'):
        code, c = _split_comment(line)
        while c is not None and is_insn_comment(c.split('//')[0]) and not c.lstrip().startswith('orig:'):
            if code.strip() == '':
                code = None
                break
            line = code.rstrip()
            code, c = line, None
        if code is None:
            continue
        out.append(line)
    return '\n'.join(out)


def write_if_changed(path, text):
    """Writes the file only when its content differs (a build does not recompile an unchanged generated file)."""
    if os.path.exists(path):
        with open(path) as f:
            if f.read() == text:
                return False
    with open(path, 'w') as f:
        f.write(text)
    return True
