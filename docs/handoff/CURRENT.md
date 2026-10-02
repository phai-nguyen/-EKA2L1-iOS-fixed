# CURRENT HANDOFF

Active branch: `ngage-machine1`

Latest implementation:
- MACHINE1-AE device report stopped at write32 `0xB1000023 -> 0x0A0000A0`, PC=`0x2220`, LR=`0x2244`.
- AE proved helper `0x21F4..0x2220` processes a copied record whose control word is `0x31000023` (low5=3); therefore AE's model-only `low5==1` gate was too narrow.
- MACHINE1-AF logic extends only that gate to the three low5 values actually present in the copied firmware table: 1, 2 and 3.
- AF still admits a record-loop write only when the destination is an aligned +8 control slot inside the existing 16-record/0x108-byte copied table, bytes were initialized by the Z copy, width is 32-bit, PC=`0x2220`, LR=`0x2244`, old bit31 is clear, and the written value exactly equals the firmware transform `(old & ~0x60) | 0x80000020`.
- Controller tests cover `0x31000023 -> 0xB1000023` and `0x32000022 -> 0xB2000022`; a wrong transform remains fail-closed.
- No RAM/MMIO range is widened. Y FIQ banking fix, Z exact 0x108-byte copy, AA/AB/AC/AD stack/writeback gates, low shadow 36 bytes, flash gates and diagnostics remain unchanged.
- `0x0C150004` semantics remain unknown; do not infer remap.
- Build workflow/harness label remains AE because connector safety blocked CI-file edits; the branch code itself is AF logic.
- Device test should use the artifact from the final AF HEAD and export the report filename embedded by the unchanged harness.

Firmware-table evidence retained:
- source `0x744` = 8-byte header + 16 records x 16 bytes = 0x108 bytes;
- `0x0A000000, size 0x01000000, control 0x32000022` is the strongest main-SDRAM candidate;
- `0x08000000, size 0x1000, control 0x30000023` contradicts the old 16-MiB SDRAM label;
- `0x0C000000, size 0x00200000, control 0x31000023` contains observed `0x0C150004`, without establishing the register semantics.

AF commits:
- `80f41daf05cb2a02dcc2ebec3808416a8b425d63` — model accepts exact low5 1/2/3 transforms.
- `50fad52fc603d4936f2c6c313cbcabc485225dfe` — controller tests for low5 2/3 plus fail-closed wrong transform.
- `24df0a8f13c21f05cdad3896c4d2174266a47411` — model documentation / removal of obsolete low5==1 constant.
