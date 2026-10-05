# NEWCHAT — N-Gage QD RH-29 MACHINE1-AL — 2026-10-02

## 0. Mục đích

Handoff canonical này chốt toàn bộ workstream RH-29 MACHINE1 đến trạng thái cuối cùng còn đọc được trong cuộc trò chuyện:

- **AK đã được test trên thiết bị và PASS anchor đầu tiên**.
- AK raw-handler dump chứng minh firmware đang thực hiện **power-of-two RAM address-line probe**.
- Repo đã triển khai **MACHINE1-AL** từ đúng bằng chứng AK.
- **AL build #127 đã PASS**, nhưng **chưa có device TXT của AL** tại thời điểm chốt.

Repo: `phai-nguyen/-EKA2L1-iOS-fixed`  
Branch: `ngage-machine1`  
Target: Nokia N-Gage QD RH-29 V04.10  
Device test: iPhone 12 Pro Max, iOS 18.7  
User language: Vietnamese.

---

## 1. Trạng thái thiết bị mới nhất — AK.txt

File user upload thực tế:

`RH29_MACHINE1_AE(7).txt`

Report identity vẫn là AE vì build AK #126 dùng harness identity cũ. Đây là **AK logic**, nhận diện bằng build/code SHA và các trường AK.

### 1.1 AK exact anchor PASS

AK cho phép đúng first probe read:

- address: `0x0A001100`
- width: 32
- PC: `0x1368`
- LR: `0x1348`
- instruction: `0xE594C000`
- synthetic seed: `0x00000000`
- read count: **1**

Quan trọng: seed 0 là **synthetic probe hypothesis**, không phải RAM content quan sát được trên hardware.

### 1.2 Execution progress

- budget: `10000`
- executed: **2520**
- stop: `unresolved_access`

AK đi qua `0x1368`, sau đó:

- `0x136C: E1E0000C`
- `0x1370: E1550007`
- `0x1374: 2A00000E`
- `0x1378: E1A02125`
- blocker tại `0x137C`.

Registers ngay trước blocker:

- R0 = `0xFFFFFFFF`
- R1 = `0x00000000`
- R2 = `0x00001000`
- R3 = `0x00000001`
- R4 = `0x0A001100`
- R5 = `0x00004000`
- R6 = `0x00000000`
- R7 = `0x01000000`
- R8 = `0x0A0000E8`
- SP = `0x0A000F88`
- LR = `0x00001348`.

New unresolved:

- kind: `data_read`
- width: 32
- address: **`0x0A005100`**
- PC: **`0x0000137C`**
- LR: **`0x00001348`**
- cause: `unmapped`.

Address derivation is explicit from registers/instruction:
- R4 anchor = `0x0A001100`
- R5 = `0x4000`
- `0x1378` derives R2=`0x1000`
- `0x137C` indexed load targets anchor + `0x4000`
- result address = `0x0A005100`.

### 1.3 CP15 unchanged

Still exactly one condition-passed P15 access:

- PC `0x2DC8`
- instruction `0xEE010F10`
- value `0x1272`
- emulated
- policy remains MMU-off.

No new evidence assigns semantics to `0x0C150004`.

---

## 2. AK raw handler dump — decisive evidence

AK report dumps exact ROM words `0x1328..0x13BC`.

Key sequence:

- `1328 E92D41F0` — stack push
- `132C E1A08000`
- `1330 E3A05901` — R5 starts at `0x4000`
- `1334 E3A06000` — R6 starts 0
- `1338 E8980090` — loads record fields, observed base `0x0A000000`, size `0x01000000`
- `1344 EB0005D7` — helper returns `0x1000`
- `1348 E2800C01` — +0x100 => `0x1100`
- `134C E3C00003` — align
- `1350 E0844000` — produces anchor `0x0A001100`
- `1368 E594C000` — save original anchor word
- `136C E1E0000C` — invert original value; with synthetic seed 0 gives `0xFFFFFFFF`
- `1370 E1550007` — compare stride vs record size
- `1374 2A00000E`
- `1378 E1A02125`
- `137C E7941102` — read distant probe word
- `1380 E7840102` — write test pattern to distant word
- `1384 E5943000` — re-read anchor
- `1388 E153000C` — verify anchor unchanged
- `138C 1A000002`
- `1390 E7943102` — read distant word back
- `1394 E1530000` — compare with test pattern
- `1398 0A000000`
- `139C E1866005` — accumulate detected bit/stride state
- `13A0 E1A03125`
- `13A4 E7841103` — restore original distant word
- `13A8 E1A05085` — double R5
- `13AC E1550007` — compare against region size
- `13B0 3AFFFFF0` — loop back while below limit
- `13B4 E2673000`
- `13B8 E1866003`
- `13BC E3760901`

This is sufficient direct firmware evidence that the routine is performing a **power-of-two address-line / alias probe**, not merely an arbitrary RAM read.

Corroborating later Symbian bootstrap source also contains RAM-bank width/config/address-map probes with save-test-verify-restore structure, but RH-29 device trace/raw ROM is the primary evidence.

---

## 3. MACHINE1-AL implementation

AL was implemented after AK device evidence.

Commit:

`548591216f477b54ccbf2fabaa97cdabeba717dc`

Message:

`feat(machine1-al): model exact sparse RAM address-line probe`

### 3.1 Sparse points only — no 16 MiB map

AL does **not** map `0x0A000000..0x0AFFFFFF`.

It models only ten 32-bit probe words derived from:

- anchor: `0x0A001100`
- first offset: `0x4000`
- each next offset doubles
- limit: `0x01000000`
- point count: **10**

Sparse offsets:

1. `0x00004000`
2. `0x00008000`
3. `0x00010000`
4. `0x00020000`
5. `0x00040000`
6. `0x00080000`
7. `0x00100000`
8. `0x00200000`
9. `0x00400000`
10. `0x00800000`

Thus first point is `0x0A005100`; gaps remain unmapped.

Each point is an independent synthetic-zero probe word. This is deliberately narrower than a RAM mapping.

### 3.2 Exact AL callsites

AL gates only the accesses proven by the handler:

- original distant read PC: `0x137C`
- test write PC: `0x1380`
- anchor verify read PC: `0x1384`
- distant readback PC: `0x1390`
- restore write PC: `0x13A4`
- LR must be `0x1348`
- width must be 32 bit.

Synthetic test value:

`~0x00000000 = 0xFFFFFFFF`

Expected completed loop counters if all ten sparse points run:

- original read: 10
- test write: 10
- anchor verify read: 10
- readback: 10
- restore write: 10.

AL tests explicitly ensure:
- sparse gaps fail closed;
- wrong PC fails;
- wrong write value fails;
- no range map is introduced.

### 3.3 Diagnostics

AL extends raw handler dump to:

`0x1328..0x13FC`

Report now uses AL identity:

- `RH29_MACHINE1_AL`
- output file `RH29_MACHINE1_AL.txt`
- workflow name `Build NGAGE-MACHINE1-AL`.

---

## 4. Build AL

GitHub Actions:

- run **#127**
- run ID: `37000204979`
- result: **SUCCESS**
- code SHA: `548591216f477b54ccbf2fabaa97cdabeba717dc`
- workflow: `Build NGAGE-MACHINE1-AL`
- run URL:
  `https://github.com/phai-nguyen/-EKA2L1-iOS-fixed/actions/runs/37000204979`

Artifact:

- name: `EKA2L1-NGAGE-MACHINE1-AL-IPA`
- artifact ID: `11223094821`
- size: `10111364` bytes
- SHA-256:
  `3ab653b13cb9f1791120b942530eec96797fab66d8d82eddbeb18828532d9245`
- download:
  `https://api.github.com/repos/phai-nguyen/-EKA2L1-iOS-fixed/actions/artifacts/11223094821/zip`

**No AL device TXT has been received yet.**

---

## 5. Immediate next action in new chat

1. Install **build #127 / AL artifact**.
2. Run RH-29 Machine Probe with budget `10000`.
3. Share `RH29_MACHINE1_AL.txt`.
4. Verify:
   - anchor read count remains 1;
   - sparse probe counters progress;
   - ideally all five sparse counters reach 10;
   - identify first new unresolved after the power-of-two loop.
5. Do **not** map the full 16 MiB region merely because firmware record says size `0x01000000`.
6. If AL reaches a new access, derive the exact next behavior from raw handler/trace before adding semantics.

---

## 6. Technical timeline to preserve

### X
- exact low-vector shadow only 36 bytes.
- blocker at 0x24 led to investigating register banking instead of widening shadow.
- never infer `0x0C150004` as remap.

### Y
- fixed Dyncom SVC<->FIQ banking for R8-R12.
- R11=`0x744` survives FIQ.
- correct R2=`0x42`.
- enabled correct bootstrap copy path.

### Z
- exact copy source `0x744`.
- destination `0x0A000000`.
- size `0x108` = 264 bytes.
- 66 write32.
- initialized-only readback.

### AA
- first exact stack push:
  - top `0x0A000FF0`
  - base `0x0A000FD0`
  - size 32
  - PC `0x11A8`, LR `0x3C8`
  - `E92D47F0`.

### AB
- exact nested push:
  - top `0x0A000FBC`
  - base `0x0A000FAC`
  - size 16
  - PC `0x22A0`, LR `0x11C8`
  - `E92D4070`.

### AC / AD
- exact 12-byte stack reuse:
  - `0x0A000FB0..0x0A000FBB`
  - PC `0x2228`, LR `0x11E0`
  - `E92D4030`.
- no range widening.

### AE
- exposed 16-record control loop.
- table: header 8 bytes + 16 records x 16 bytes = 0x108.
- first model allowed control low5=1 only.
- device proved low5=3 uses same transform helper.

### AF
- exact record loop allowed only observed low5 set 1/2/3.
- all 16 record controls processed.
- no semantic labels forced onto low5 values.

### AG
- exact stack reuse at:
  - PC `0x2258`
  - LR `0x11E8`
  - `E92D4030`
  - same 12-byte footprint.

### AH
- `0x1BB4 E92D40F0`, 20-byte push.
- only one new word `0x0A000FA8..0x0A000FAB`; upper 16 bytes reuse old backing.

### AI
- `0x1438 E92D4010`, 8-byte push.
- exact backing `0x0A000FA0..0x0A000FA7`.

### AJ
- low5=2 dispatch enters handler `0x1328`.
- `E92D41F0`, exact 24-byte stack `0x0A000F88..0x0A000F9F`.
- handler then accesses record base/size and computes `0x0A001100`.

### AK
- exact first read-only anchor at `0x0A001100`.
- synthetic seed 0, explicitly not device-observed content.
- raw handler dump proves sparse power-of-two alias/address-line probe.
- next device blocker = `0x0A005100` at PC `0x137C`.

### AL
- exact ten-point sparse RAM-probe model.
- no broad RAM range.
- build #127 PASS.
- awaiting device TXT.

---

## 7. Firmware region table evidence

Recovered table remains a core artifact.

Most important records:

- `0x0A000000`, size `0x01000000`, control `0x32000022`
  - strongest main-SDRAM candidate.
- `0x08000000`, size `0x00001000`, control `0x30000023`
  - contradicts old code label treating 0x08000000 as 16-MiB main SDRAM.
- `0x0C000000`, size `0x00200000`, control `0x31000023`
  - contains observed `0x0C150004`.
- `0x0C120000`, size `0x1000`, control `0x30200023`.

Do not equate table membership with exact register semantics.

---

## 8. Permanent evidence/safety constraints

1. Do not revert old checkpoints.
2. Preserve Y FIQ R8-R12 fix.
3. Preserve Z exact 0x108 copy.
4. Preserve AA/AB/AC/AD/AF/AG/AH/AI/AJ exact stack/control gates.
5. Preserve AK anchor semantics as **synthetic probe seed**, not hardware content.
6. Preserve AL sparse points only; no generic 16-MiB map.
7. Low-vector shadow remains 36 bytes.
8. Do not broaden stack/RAM just to advance PC.
9. `0x0C150004` semantics remain unknown.
10. CP15 C1 observation remains `0x1272`, MMU off.
11. Every new admission should be exact address/width/PC/LR/value where evidence permits.
12. Negative fail-closed tests required.
13. Distinguish device-observed facts from model hypotheses.
14. Every future PASS build response should include direct GitHub Actions link and artifact link/details.
15. Share-report UI remains directly below Run probe.

---

## 9. UI / user preferences

- Reply in Vietnamese.
- User does not know English.
- User permits autonomous repo edits/builds.
- Do not ask unnecessary clarification when evidence is sufficient.
- iPhone 12 Pro Max, iOS 18.7.
- No Mac.
- Windows 7 x64, 2 GB RAM.
- User wants direct build links.
- User likes handoff/checkpoint before chats become too long.

---

## 10. Command for a new conversation

Use:

`Tiếp tục MACHINE1-AL từ docs/handoff/NEWCHAT-NGAGE-QD-MACHINE1-AL-2026-10-02.md trong repo phai-nguyen/-EKA2L1-iOS-fixed, nhánh ngage-machine1. AK device report đã PASS exact anchor 0x0A001100 và dừng tại data_read 0x0A005100, PC=0x137C/LR=0x1348. Raw handler 0x1328..0x13BC chứng minh power-of-two RAM address-line probe. AL commit 548591216f477b54ccbf2fabaa97cdabeba717dc model đúng 10 sparse words 0x4000..0x800000 offset, không map 16 MiB. Build #127 PASS; chờ/đọc RH29_MACHINE1_AL.txt từ thiết bị. Không broad-map RAM/stack và không suy diễn 0x0C150004 là remap register.`

---

## 11. End-state at handoff creation

- AL code SHA: `548591216f477b54ccbf2fabaa97cdabeba717dc`
- AL build #127: PASS
- artifact: `EKA2L1-NGAGE-MACHINE1-AL-IPA`
- next required evidence: **device TXT from AL**.
