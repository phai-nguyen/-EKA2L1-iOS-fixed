# CURRENT HANDOFF

Active branch: `ngage-machine1`

Latest state:
- MACHINE1-Z device log `RH29_MACHINE1_Z.txt` analyzed on 2026-10-02.
- Z proves the Y-derived bootstrap copy is exact: `BOOTSTRAP_COPY_WRITE_COUNT=66`, `BOOTSTRAP_COPY_INITIALIZED_BYTES=264`.
- Low-vector shadow remains 36 bytes and `LOW_VECTOR_SHADOW_WRITE_COUNT=0`; do not expand/reinterpret low memory.
- After copy completion, firmware reads base `0x0A000000` and PC `0x3AC` instruction `0xE280DEFF` computes SP=`0x0A000FF0`.
- New blocker: PC `0x11A8`, instruction `0xE92D47F0` = `STMDB sp!, {r4-r10,lr}`; first write32 fails at `0x0A000FD0`, LR=`0x3C8`.
- This implies an exact first push footprint of 32 bytes: `0x0A000FD0–0x0A000FEF`.
- MACHINE1-AA adds only that footprint, gated by PC/LR/width and initialized-only readback. It does not map the whole 4-KiB page.
- FIQ banking fix from Y and exact 0x108-byte bootstrap copy from Z remain unchanged.
- `0x0C150004` semantics remain unknown; do not infer remap.
- Research note: `docs/research/RH29-MACHINE1-AA-2026-10-02.md`.
