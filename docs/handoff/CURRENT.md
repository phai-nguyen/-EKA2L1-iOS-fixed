# CURRENT HANDOFF

Active branch: `ngage-machine1`

Latest state:
- MACHINE1-AA device log `RH29_MACHINE1_AA.txt` analyzed on 2026-10-02.
- AA confirms the first bootstrap stack push completed exactly: 8 x 32-bit writes / 32 initialized bytes at `0x0A000FD0–0x0A000FEF`.
- After that push, PC `0x11AC` executes `SUB sp,sp,#0x14`, leaving SP=`0x0A000FBC`.
- PC `0x11C4` calls `0x22A0`, giving LR=`0x11C8`. Instruction `0xE92D4070` at `0x22A0` is `STMDB sp!, {r4-r6,lr}`.
- AA stops on the first write32 at `0x0A000FAC`; therefore the exact second push footprint is 16 bytes: `0x0A000FAC–0x0A000FBB`.
- MACHINE1-AB adds only that 16-byte footprint, gated by PC `0x22A0`, LR `0x11C8`, aligned width32, with initialized-only readback.
- The gap `0x0A000FBC–0x0A000FCF` remains fail-closed; do not map the whole stack page.
- Probe UX baseline now places **Chia sẻ báo cáo** directly below **Chạy probe**, with the old bottom ShareLink removed.
- Y FIQ banking fix, Z exact 0x108-byte copy, AA first 32-byte push, and low shadow 36 bytes remain unchanged.
- `0x0C150004` semantics remain unknown; do not infer remap.
- Research note: `docs/research/RH29-MACHINE1-AB-2026-10-02.md`.
