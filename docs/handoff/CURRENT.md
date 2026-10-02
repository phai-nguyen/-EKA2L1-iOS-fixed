# CURRENT HANDOFF

Canonical handoff:
`docs/handoff/NEWCHAT-NGAGE-QD-MACHINE1-AL-2026-10-02.md`

Active repo/branch:
- repo: `phai-nguyen/-EKA2L1-iOS-fixed`
- branch: `ngage-machine1`
- target: Nokia N-Gage QD RH-29 V04.10

Latest device evidence:
- AK logic report filename: `RH29_MACHINE1_AE(7).txt`
- budget 10000; executed 2520; stop `unresolved_access`.
- exact AK anchor read at `0x0A001100`, PC=`0x1368`, LR=`0x1348`, instruction `0xE594C000`, synthetic seed 0, accepted count 1.
- raw handler dump `0x1328..0x13BC` proves a power-of-two address-line/alias probe.
- new blocker:
  - data_read width32
  - address `0x0A005100`
  - PC `0x137C`
  - LR `0x1348`
  - cause unmapped.
- CP15 unchanged: `0x2DC8 / EE010F10 / 0x1272`, emulated, MMU-off policy.

MACHINE1-AL:
- code SHA `548591216f477b54ccbf2fabaa97cdabeba717dc`.
- models only 10 sparse 32-bit probe words at anchor + power-of-two offsets `0x4000..0x800000`.
- exact callsites:
  - original distant read `0x137C`
  - test write `0x1380`
  - anchor verify read `0x1384`
  - distant readback `0x1390`
  - restore write `0x13A4`
  - LR `0x1348`.
- synthetic independent zero seeds; test pattern `0xFFFFFFFF`.
- gaps remain unmapped; no 16-MiB RAM map.
- handler dump extended to `0x1328..0x13FC`.
- report/workflow identity now AL.

Build:
- run #127, ID `37000204979`: **SUCCESS**
- artifact `EKA2L1-NGAGE-MACHINE1-AL-IPA`
- artifact ID `11223094821`
- SHA-256 `3ab653b13cb9f1791120b942530eec96797fab66d8d82eddbeb18828532d9245`
- run: `https://github.com/phai-nguyen/-EKA2L1-iOS-fixed/actions/runs/37000204979`
- artifact: `https://api.github.com/repos/phai-nguyen/-EKA2L1-iOS-fixed/actions/artifacts/11223094821/zip`

Next:
1. Device-test AL build #127 with budget 10000.
2. Share `RH29_MACHINE1_AL.txt`.
3. Check five sparse-probe counters; expected 10 each if full loop completes.
4. Follow only the next exact unresolved transaction.
5. Do not broad-map `0x0A000000..0x0AFFFFFF`.
6. Do not infer `0x0C150004` is a remap register.

Permanent constraints:
- preserve Y FIQ banking fix;
- preserve Z exact 0x108 copy;
- preserve all exact AA..AK gates;
- low vector shadow remains 36 bytes;
- synthetic RAM probe seeds are hypotheses, not device-observed RAM contents;
- keep Share report UI directly below Run probe.
