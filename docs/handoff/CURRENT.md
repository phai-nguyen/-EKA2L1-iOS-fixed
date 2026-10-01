# CURRENT HANDOFF

Current:
`docs/handoff/NEWCHAT-NGAGE-QD-MACHINE1-X-2026-10-01.md`

Active branch: `ngage-machine1`

Latest state:
- MACHINE1-X build #69 / run `36875051410`: SUCCESS.
- Baseline code SHA before HANDOFF: `3f15f6156971df13f0e2ae59dc6717aef234beea`.
- Artifact: `EKA2L1-NGAGE-MACHINE1-X-IPA`, ID `11169086924`, size `10,105,136` bytes.
- X low-vector shadow is 36 bytes / 9 words; exact observed write at `0x20` is admitted.
- `0x24+` remains fail-closed.
- RH-29 Machine Probe menu is always visible again; backend still rejects non-RH29 execution.
- W diagnostics remain enabled in X.
- Do not infer `0x0C150004` is a remap register; semantics remain unknown.
- Device log `RH29_MACHINE1_X.txt` received: 1,549 instructions; the `0x20` write passes and the 32-bit zero write at `0x24` fails closed at PC `0x2344`.
- R2 provenance confirmed: low-address data read `[0x00]` at PC `0x12EC` returns ROM branch word `0xEA0000C9`; shift/OR/rounding produces `0x28000326` copy words. See `docs/research/RH29-MACHINE1-X-2026-10-01.md`.
- The emulator response is understood; the RH-29 hardware data backing at address zero at this stage is not. Do not fabricate a value, infer a remap trigger, or expand the low shadow to `0x24`.
- New ROM/source finding: SVC R11=`0x744` before the RAM probe; the guest changes to FIQ and sets FIQ R11=0. Pinned Dyncom `ChangePrivilegeMode()` does not preserve shared non-FIQ R8–R12 across FIQ, so SVC R11 becomes 0 and causes the spurious `[0x00]` read. See the follow-up section of `docs/research/RH29-MACHINE1-X-2026-10-01.md`.
- Next isolated experiment: fix/test FIQ R8–R12 banking, keep X's memory response and 36-byte shadow unchanged, then capture actual R1/R2 and next fail-closed barrier. With correct banking, R1=`0x744` and R2=`0x42` are predictions to verify, not device results. Do not infer `0x0C150004` semantics.
