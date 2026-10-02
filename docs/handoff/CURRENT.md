# CURRENT HANDOFF

Active branch: `ngage-machine1`

Latest state:
- MACHINE1-AB device log `RH29_MACHINE1_AB.txt` analyzed on 2026-10-02.
- AB confirms the exact second nested stack push completed: 4 x 32-bit writes / 16 initialized bytes at `0x0A000FAC–0x0A000FBB`, with 4 readbacks.
- After returning from that helper, PC `0x11C8` reads the already-copied word at `0x0A000010` (originally `0x32800021`), then PC `0x11CC`/`0x11D0` OR control bits to produce `0xB2800021`.
- New blocker: PC `0x11D4`, instruction `0xE5803008` = `STR r3,[r0,#8]`, attempts write32 `0xB2800021 -> 0x0A000010`, LR=`0x22C4`.
- `0x0A000010` is inside the existing Z bootstrap-copy footprint `0x0A000000–0x0A000107`; this is an in-place mutation, not evidence for a wider RAM mapping.
- MACHINE1-AC permits only this exact writeback: address `0x0A000010`, width32, PC `0x11D4`, LR `0x22C4`, value `0xB2800021`, and only when those four bytes have already been initialized by the Z copy.
- Y FIQ banking fix, Z exact 0x108-byte copy, AA first stack push, AB second stack push, low shadow 36 bytes, and the probe-share UX remain unchanged.
- **Chia sẻ báo cáo** stays directly below **Chạy probe**.
- `0x0C150004` semantics remain unknown; do not infer remap.
- Research note: `docs/research/RH29-MACHINE1-AC-2026-10-02.md`.


## Deep firmware finding after AB

Static reconstruction of the 66-word source block at ROM alias `0x744` shows it is a two-word header followed by **16 records x 16 bytes**. The executed helper at `0x12FC` proves each record starts with `base,size`; `0x22A0` scans the 16 records and returns the one containing a queried address; `0x11C8–0x11D4` mutates field +8 of the selected record.

The recovered table includes:
- `0x0A000000, size 0x01000000` — exactly 16 MiB, matching RH-29's 128-Mbit SDRAM and the observed bootstrap-copy/stack region. This is now the strongest main-SDRAM candidate.
- `0x08000000, size 0x1000` — only 4 KiB; the old MACHINE1 label treating 0x08000000 as 16-MiB candidate SDRAM is now contradicted by firmware evidence and should be corrected in a later model step.
- `0x0C000000, size 0x00200000` — contains the observed address `0x0C150004`, but that register's exact semantics remain unknown.
- flash-family ranges lining up with independent RH-29 UFS/JAF evidence.

Field +8 values (`0x32800021`, `0x31000023`, `0x33400023`, `0x30000023`, `0x32000022`, `0x30200023`) remain raw firmware values. Their low bits resemble ARMv4/v5 translation descriptor types, but do not yet label them as raw PDEs.

Research note: `docs/research/RH29-BOOTSTRAP-REGION-TABLE-2026-10-02.md`.
