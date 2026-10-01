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
- No MACHINE1-X device log has been received yet.
- Next step: install X, run Probe, export `RH29_MACHINE1_X.txt`.
- If X stops at `0x24`, trace R2/copy-count provenance before any further low-shadow expansion.
