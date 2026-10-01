# CURRENT HANDOFF

Current:
`docs/handoff/NEWCHAT-NGAGE-QD-MACHINE1-X-2026-10-01.md` (historical X handoff); active implementation has advanced through Y to MACHINE1-Z.

Active branch: `ngage-machine1`

Latest state:
- MACHINE1-Y build #72 / run `36884157684`: SUCCESS.
- Device log `RH29_MACHINE1_Y.txt` analyzed.
- Y confirms the Dyncom FIQ banking fix: SVC R11 remains `0x744`; the false low-address `[0x00]` path from X disappears.
- At PC `0x12EC`, R1=`0x744`, the guest reads `0x10`, derives `0x108`, then R2=`0x42` words before the copy loop.
- Actual copy source is `0x744`; destination is `0x0A000000`.
- Y blocker: write32 `0x32800021 -> 0x0A000010` at PC `0x2344`, LR `0x3C8`; this is the fifth word of the now-correct copy path.
- Low-vector shadow stays 36 bytes and Y records zero low-vector writes. Do not expand or reinterpret low memory.
- `0x0C150004` semantics remain unknown; do not infer remap.
- MACHINE1-Z is the next evidence-gated model: only the exact `0x108`-byte copy footprint at `0x0A000000`, gated by PC/LR/width, with initialized-only readback. Earlier sparse RAM-probe behavior remains unchanged.
- Z workflow also reuses the Y Xcode cache as a restore fallback to avoid another near-full 30-minute compile where possible.
- Research note: `docs/research/RH29-MACHINE1-Z-2026-10-01.md`.
