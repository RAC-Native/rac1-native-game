# The VU0 ops without their flags for the generated files that read none (2026-11-01, the Wii U's CPU; src/game/ovl/vu0_nf.h):
# a generated file using vu0m's ops gets, in place of its `using namespace vu0m;`, vu0nf's on the Wii U (or a build defining
# RAC1_VU0_NF, the PC's check) when every CFC2 16 / 17 it has (the status / MAC flags) follows, in straight-line code, an FMAC op
# whose destination is vf0 - vu0nf keeps those exact. CFC2 18 (the clip flags) is vclip's alone, which vu0nf leaves exact.
# Run by generate_all.py over the files it made.
import re

OP = re.compile(r'\b(add|sub|mul|madd|msub|addbc|subbc|mulbc|maddbc|msubbc|maddv|msubv|opmsub|rotate|transform)\((vf\(\)\[(\d+)\])')
ACC = re.compile(r'\b(addabc|subabc|mulabc|maddabc|msubabc|mulav|addav|subav|maddav|msubav|opmula)\(')
LABEL = re.compile(r'\s*L_[0-9a-f]+:')
USING = 'using namespace vu0m;'
SWITCH = ('// (the Wii U: the VU0 ops without their flags, vu0_nf.h - this file reads no status / MAC flag but after an op into vf0)\n'
          '#if defined(__WIIU__) || defined(RAC1_VU0_NF)\n'
          'using namespace vu0nf;\n'
          '#else\n'
          'using namespace vu0m;\n'
          '#endif')


def flags_safe(text):
    """True when every CFC2 16 / 17 of the text follows, with no label in between, an op into vf0"""
    last = None
    for line in text.split('\n'):
        if LABEL.match(line):
            last = 'label'
        m = OP.search(line)
        if m:
            last = 'vf0' if m.group(3) == '0' else 'vf'
        elif ACC.search(line):
            last = 'acc'
        if re.search(r'cfc2\(1[67]\)', line) and last != 'vf0':
            return False
    return True


def apply(path):
    """The file at `path` switched to vu0nf when it may be; True when it was"""
    text = open(path, encoding='utf-8').read()
    if USING not in text or not flags_safe(text):
        return False
    lines = text.split('\n')
    out = []
    for line in lines:
        if line.strip() == USING:
            ind = line[:len(line) - len(line.lstrip())]
            out.extend(ind + l if l.startswith('using') else l for l in SWITCH.split('\n'))
        else:
            out.append(line)
    text = '\n'.join(out)
    inc = '#include <game/ovl/vu0_regs.h>'
    if inc in text:
        text = text.replace(inc, inc + '\n#include <game/ovl/vu0_nf.h>', 1)
    else:
        text = re.sub(r'^(namespace rac1)', '#include <game/ovl/vu0_nf.h>\n\n\\1', text, count=1, flags=re.M)
    open(path, 'w', encoding='utf-8').write(text)
    return True
