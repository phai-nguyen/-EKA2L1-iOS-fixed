# CURRENT HANDOFF

Canonical handoff:
`docs/handoff/NEWCHAT-NGAGE-QD-MACHINE1-AQ-EKA1-RESEARCH-2026-10-03.md`

Active repo/branch:
- repo: `phai-nguyen/-EKA2L1-iOS-fixed`
- branch: `ngage-machine1`
- target: Nokia N-Gage QD RH-29 V04.10

Latest device evidence:
- report: `RH29_MACHINE1_AP.txt`
- budget 10000
- executed: 3017
- stop: `unresolved_access`
- AP exact 8-word relocated push at `0xF18 E92D47F0` passed.
- after `0xF1C SUB sp,sp,#0x54`, SP = `0x09FFF388`.
- next exact blocker:
  - data_write width32
  - address `0x09FFF3AC`
  - PC `0x00000F24`
  - LR `0x00000F18`
  - instruction `0xE58DC024`
  - value `0`
  - cause unmapped.
- CP15 unchanged: `0x2DC8 / EE010F10 / 0x1272`, MMU-off policy.

MACHINE1-AQ:
- code head before research doc: `f9eee740f26e38291c588b8d73bb74d5496df910`
- exact-only gate for `0x09FFF3AC = 0` at PC `0xF24`, LR `0xF18`
- initialized-only readback
- no 0x54-byte frame widening.
- report identity: `RH29_MACHINE1_AQ`
- expected report file: `RH29_MACHINE1_AQ.txt`.

Build:
- run #132, ID `37019841827`: **SUCCESS**
- artifact `EKA2L1-NGAGE-MACHINE1-AQ-IPA`
- artifact ID `11233745298`
- SHA-256 `ba96ea84fc67423eb900b8e764a081036c81dd9a954f65244eed0f6ce3770ab9`
- run: `https://github.com/phai-nguyen/-EKA2L1-iOS-fixed/actions/runs/37019841827`

Latest research:
- commit `23c5b741efd23da731c45b98ab52db23faa0a2d4`
- file `docs/research/RH29-BOOTSTRAP-REGION-TABLE-2026-10-02.md`
- correct-era Series 60 6.1 `TRomHeader`:
  - `+0x8C = iRomBase`
  - `+0x90 = iRomSize`
- AP device read at physical `0x90` returns `0x01170000`, now source-backed as ROM size.
- expected F30 read from physical `0x8C` is `0x50000000`, but this remains to be device-confirmed in AQ.
- 0x50000000 is EKA1 logical ROM base; do not pre-map it.
- known historical EKA1 virtual layout (page directory 0x41000000, page tables 0x42000000, ROM 0x50000000, RAM 0x60000000) is corroboration only, not permission to add mappings.

Next:
1. Device-test AQ build #132 with budget 10000.
2. Share `RH29_MACHINE1_AQ.txt`.
3. Follow only the next exact unresolved access.
4. Do not implement predicted F34 store before device evidence.
5. Do not broad-map RAM, stack, page tables, ROM VA, or EKA1 RAM VA.
6. Keep `0x0C150004` semantics unknown.

Permanent:
- preserve Y FIQ R8–R12 banking fix;
- preserve Z exact 0x108 copy;
- preserve exact AA..AQ gates;
- low-vector shadow stays 36 bytes;
- sparse RAM probe remains exact-only;
- distinguish device evidence from deterministic derivation and external research.
