# CURRENT HANDOFF

Current:
`docs/handoff/NEWCHAT-NGAGE-QD-MACHINE1-V-2026-10-01.md`

Active branch: `ngage-machine1`

Latest state:
- MACHINE1-V build #56 / run `36850283187`: SUCCESS.
- Device log `RH29_MACHINE1_V.txt` fully analyzed.
- Exact early setup fingerprint recovered for `0x08000000` writes and `STRH 0x0080 -> 0x0C150004`.
- Identity of `0x08000000` and `0x0C150004` remains unproven; do not label either as remap/MMIO controller without corroboration.
- Synthetic ROM alias at low address 0 is likely wrong for later data access: ROM opcode `0xEA0000C9` is consumed as data and produces bogus copy count `0x28000326`.
- Current blocker remains write32 `0x00000020` at PC `0x2344`, LR `0x3C8`, cause `rom_write`.
- Do not extend low shadow blindly.
- MACHINE1-W is not implemented.
- Next step: targeted RH-29 / DCT4 / UPP-WD2 research using exact V fingerprint; if still unresolved, build W-DIAG only.
