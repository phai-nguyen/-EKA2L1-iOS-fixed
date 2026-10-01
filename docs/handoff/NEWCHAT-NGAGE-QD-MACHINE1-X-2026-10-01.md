# HANDOFF — EKA2L1 N-Gage QD RH-29 MACHINE1-X — 2026-10-01

## Lệnh bắt đầu chat mới

Tiếp tục dự án N-Gage QD RH-29 MACHINE1 từ HANDOFF `NEWCHAT-NGAGE-QD-MACHINE1-X-2026-10-01.md` trong repo `phai-nguyen/-EKA2L1-iOS-fixed`, nhánh `ngage-machine1`. MACHINE1-X build #69 đã PASS. Giữ nguyên mô hình X, không quay lại model cũ, không tự suy diễn `0x0C150004` là remap register. Bước kế tiếp là device-test X, xác nhận nút **RH-29 Machine Probe** đã hiện lại, chạy Probe một lần và phân tích `RH29_MACHINE1_X.txt`. Nếu X dừng tại `0x24`, không tiếp tục mở rộng low shadow mù quáng; phải truy ngược nguồn tạo R2/copy count trước.

---

## 1. Active scope

Workstream đang active:
**Nokia N-Gage QD RH-29 V04.10 — MACHINE1 full-machine/raw ARM probe trên EKA2L1 iOS.**

Mục tiêu:
- chạy boot ARM thật từ RH-29 ROM trong một probe độc lập;
- dùng fresh Dyncom core;
- bus fail-closed;
- chỉ mô hình hóa behavior có device/source evidence;
- không phá HLE đang hoạt động tốt;
- không fallback sang model cũ khi gặp blocker.

Không trộn với:
- RM-356 CompatBoot/DirectHome;
- RM-356 Menu3→Home;
- EKA2L1 iOS localization/iOS15;
- WP7 XAP Runner;
- N-Gage game localization.

Repo / branch:
- repo: `phai-nguyen/-EKA2L1-iOS-fixed`
- branch: `ngage-machine1`
- upstream pin: `EKA2L1/EKA2L1@c5ae62d9c667f6e7060d76be58a2614122a629a4`

Thiết bị test:
- iPhone 12 Pro Max
- iOS 18.7

Known-good product path cần giữ:
- RH-29 HLE import/boot/apps/game vẫn hoạt động;
- HYBRIDHOME3;
- VPL importer;
- Vietnamese localization;
- Bundle runtime `app.lavender1865.valley8348`;
- `EKA2L1_IOS_DYNARMIC=OFF`.

---

## 2. Hard safety / evidence rules

Bắt buộc giữ:

1. Không quay lại model cũ.
2. Không gán semantics cho address/magic value nếu chưa có corroboration.
3. Đặc biệt:
   - **không suy diễn `0x0C150004` là remap register**;
   - `0x08000000` hiện chỉ là vùng candidate đang được write-tracked;
   - `0x0A000000` có evidence mạnh hơn là candidate RAM-bank probe.
4. Không mở rộng low memory writable theo kiểu “thêm từng word cho chạy tiếp” nếu chưa hiểu copy count.
5. Không map broad low RAM chỉ để vượt blocker.
6. Tách rõ:
   - device evidence;
   - generic Symbian/ARM corroboration;
   - hypothesis.

---

## 3. RH-29 / WD2 research hiện có

Public service/flashing evidence đáng giữ:
- RH-29 dùng **UPP WD2**;
- 1 x 128 Mbit SDRAM;
- 1 x 128 Mbit flash + 1 x 64 Mbit flash;
- SDRAM D310, flash D311/D312;
- flash logs cho second flash:
  - `0x02000000–0x027FFFFF`;
  - AMD 29BDS064J;
  - manufacturer `0x0001`;
  - device `0x277E`;
- WD2 loaders:
  - `w3_2nd.fia`
  - `w3_amd.fia`.

JAF/RH-29 dump evidence:
first 16 bytes:
`C9 00 00 EA 70 01 00 EA 73 01 00 EA 76 01 00 EA`

First LE word:
`0xEA0000C9`

Điều này khớp ROM word đầu của MACHINE1 và address-zero cold reset alias hiện tại.

Targeted search đã thử nhưng **không có credible exact register definition** cho:
- `0x7037F800`
- `0xB2950489`
- `0x20202112`
- `0x08000000`
- `0x0C150004`
- halfword `0x0080`.

Kết luận:
**semantics của các giá trị trên vẫn unknown.**

---

## 4. Baseline trước W

MACHINE1-V đã cho exact early setup:

### 0x08000000 block

```asm
0xB24 E3A00302   MOV  r0,#0x08000000
0xB28 E59F1068   LDR  r1,[pc,#0x68]   ; 0x7037F800
0xB2C E5801000   STR  r1,[r0]

0xB30 E59F0064   LDR  r0,[pc,#0x64]   ; 0x08000004
0xB34 E59F1064   LDR  r1,[pc,#0x64]   ; 0xB2950489
0xB38 E5801000   STR  r1,[r0]

0xB3C E59F0060   LDR  r0,[pc,#0x60]   ; 0x08000008
0xB40 E3A01037   MOV  r1,#0x37
0xB44 E5801000   STR  r1,[r0]

0xB48 E59F0058   LDR  r0,[pc,#0x58]   ; 0x08000010
0xB4C E59F1058   LDR  r1,[pc,#0x58]   ; 0x20202112
0xB50 E5801000   STR  r1,[r0]

0xB54 E59F0054   LDR  r0,[pc,#0x54]   ; 0x08000014
0xB58 E59F104C   LDR  r1,[pc,#0x4C]   ; 0x20202112
0xB5C E5801000   STR  r1,[r0]
```

### exact operation ở 0x0C150004

```asm
0xB60 E59F004C   LDR  r0,[pc,#0x4C]   ; 0x0C150004
0xB64 E3A01080   MOV  r1,#0x80
0xB68 E1C010B0   STRH r1,[r0]
0xB6C E1A0F00E   MOV  pc,lr
```

Direct evidence:
- fixed setup sequence;
- `0x0080 -> 0x0C150004` là thao tác thật;
- chức năng phần cứng chưa biết.

---

## 5. MACHINE1-W — diagnostics-only

W commit:
- `e27add129f1c8cb5e2ff6da9ff0ed27fb1682abf`
- `feat(machine1): add evidence-only MACHINE1-W diagnostics`

W mục tiêu:
- **không đổi memory response**;
- không remap assumption;
- thu thêm evidence.

W thêm:
- all guest bus callbacks trong low 4KiB;
- full R0–R12 quanh PC `0x300–0x37F`;
- raw ROM literal pool `0xB90–0xBC0`;
- instruction index quanh `PC=0xB68`;
- mọi condition-passed CP15 trước barrier.

W build:
- workflow: Build NGAGE-MACHINE1-W
- run #61
- run ID `36860576014`
- PASS
- artifact: `EKA2L1-NGAGE-MACHINE1-W-IPA`
- artifact ID `11161688824`
- size `10,104,642`
- ZIP SHA256 `57876a54f289db7beaafca62fcc8ac7f7d6a15d786ec9fe1a356a84b9046f577`

---

## 6. Device result MACHINE1-W

User gửi:
`RH29_MACHINE1_W.txt`

User observation:
**sau W, nút/chức năng Probe không còn hiện trong app.**

Log W:
- header `RH29_MACHINE1_W`
- ROM base `0x50000000`
- ROM size `0x01170000`
- reset PC `0`
- synthetic ROM alias at zero
- current candidate SDRAM:
  - base `0x08000000`
  - 16 MiB
  - write-tracked
- exact MMIO allow:
  - address `0x0C150004`
  - width 16 bit
  - value `0x0080`
  - accepted once
  - semantics still unknown
- Flash2 `0x02000000`, 8 MiB
- RAM probe base `0x0A000000`
- post-probe workspace `0x0A0001E0`
- low-vector shadow 32 bytes
- executed instructions: `1545`
- stop: `unresolved_access`
- low-page trace total: `178`.

### Exact setup writes confirmed again

1. `0x08000000 <- 0x7037F800`
2. `0x08000004 <- 0xB2950489`
3. `0x08000008 <- 0x00000037`
4. `0x08000010 <- 0x20202112`
5. `0x08000014 <- 0x20202112`

---

## 7. Exact blocker từ W

Routine:

```asm
0x2340 E4913004   LDR  r3,[r1],#4
0x2344 E4803004   STR  r3,[r0],#4
0x2348 E2522001   SUBS r2,r2,#1
0x234C 1AFFFFFB   BNE  0x2340
```

Observed copy:
- `0x00 -> 0x00` = `EA0000C9`
- `0x04 -> 0x04` = `EA000170`
- `0x08 -> 0x08` = `EA000173`
- `0x0C -> 0x0C` = `EA000176`
- `0x10 -> 0x10` = `EA000179`
- `0x14 -> 0x14` = `EA00017C`
- `0x18 -> 0x18` = `EA0000FE`
- `0x1C -> 0x1C` = `EA00013B`
- read `0x20 = 0`
- write `0x20 = 0` thất bại vì 32-byte shadow kết thúc tại `0x1F`.

Last low-page events:
- read32 address `0x20`, value 0, PC `0x2340`, LR `0x3C8`, success=1
- write32 address `0x20`, value 0, PC `0x2344`, LR `0x3C8`, success=0

### R2 vẫn cực lớn

W trace:
- quanh vòng đầu: R2 khoảng `0x28000325`
- khi tới 0x20: R2 khoảng `0x2800031E`

=> không được broad-map low memory chỉ để cho loop chạy.

---

## 8. CP15 evidence trong W

Chỉ thấy một condition-passed CP15 trước stop:

- PC `0x2DC8`
- instruction `0xEE010F10`
- RD=0
- value `0x00001272`
- emulated=1

Không có evidence mới cho remap.

B68:
- observed
- instruction index before: 33
- after: 34

Không có bằng chứng nào cho phép gọi `0x0C150004` là low-memory remap control.

---

## 9. Quyết định MACHINE1-X

X được tạo theo nguyên tắc evidence-gated:

1. sửa regression UI của Probe;
2. chỉ mở đúng **một word mới đã quan sát trực tiếp** ở `0x20`;
3. `0x24+` vẫn fail-closed;
4. không map broad low memory;
5. không thay semantics của `0x08000000` hoặc `0x0C150004`.

### X low-vector model

File:
`machine1/include/rh29_machine_model.h`

Thay:
- 8 words / 32 bytes
thành:
- **9 words / 36 bytes**

Exact:
`low_vector_shadow_size = 9u * sizeof(std::uint32_t)`

Comments ghi rõ:
- W đã chứng minh exact write32 tại `0x20`;
- X chỉ advance đúng word này;
- `0x24+` fail-closed;
- không biến large R2 thành invented broad mapping.

Commit:
- `d5c5684239077bea481fe7fc0a6314c10cc1165d`
- `feat(machine1-x): admit only observed low-vector word at 0x20`

---

## 10. X UI Probe fix

Root cause của regression UI W:
trước đó nút Probe chỉ được inject nếu:

`store.currentDevice?.firmwareCode.caseInsensitiveCompare("RH-29") == .orderedSame`

Nếu currentDevice state không đúng thời điểm, menu Probe biến mất.

X đổi:
- **RH-29 Machine Probe luôn hiện trong More menu**.
- Backend vẫn chặn non-RH29 trong `IosEmulator.mm`.

Tức là:
- UI visibility unconditional;
- execution safety vẫn RH-29-only.

Commit:
- `27b3fe6039263391b9120a09d45cdfa66ff679c7`
- `feat(machine1-x): restore probe UI and stage evidence-only X`

Backend RH-29 guard bắt buộc giữ.

---

## 11. X labels / report

X đổi:
- `MACHINE1-X`
- `RH29_MACHINE1_X`
- export file:
  `RH29_MACHINE1_X.txt`

Report:
- `LOW_VECTOR_SHADOW_POLICY=arm_low_vectors_36byte_rom_seeded_mutable_write32`
- `LOW_VECTOR_SHADOW_STATUS=X_admits_only_W_observed_write_at_0x20_0x24_plus_fail_closed`

Giữ W diagnostic policy:
- `MACHINE1_W_DIAG_POLICY=no_memory_response_change_no_remap_assumption`

Điều này có chủ ý: X reuse diagnostics W; không có nghĩa X quay lại W model.

---

## 12. X documentation

Research doc:
`docs/research/RH29-MACHINE1-X-2026-10-01.md`

Ghi:
- W exact loop `0x2340–0x234C`;
- exact observed write `0x20`;
- X shadow 36 bytes;
- `0x24+` fail-closed;
- không gán semantics cho `0x0C150004`;
- không claim remap;
- Probe UI luôn hiện, backend vẫn RH-29 gate.

Commit:
- `9ecff47b947fd0af1a2e7002122e8c7879623507`
- `docs(machine1-x): record evidence gate`

---

## 13. MACHINE1-X CI failure history và fix

### #65 — FAIL
Run:
`36864032125`

Fail ở:
`Run MACHINE1 controller tests`

Hai stale test:
- vẫn đòi low shadow = 8 words;
- vẫn đòi UI chứa RH-29 condition.

Đã sửa contract.

### #66 — FAIL
Run:
`36872254620`

Vẫn chạy commit chưa mang đủ corrected test expectations.
Hai assertion cũ còn trong run:
- 8-word low shadow;
- RH-29 conditional UI.

Sau đó branch có commit:
- `21a1a52584487569a03846a1e06649e80bea8a57`
- `fix(machine1-x): correct stale controller expectations`

### #67 — FAIL
Run:
`36872645490`

Còn stale assertion:
`arm_low_vectors_32byte_rom_seeded_mutable_write32`

Trong khi X đúng là:
`arm_low_vectors_36byte_rom_seeded_mutable_write32`

Fix commit:
- `55f9ef6d004dac173e5781ceb97252fb5de5ac70`
- `fix(machine1-x): update 36-byte low-vector test contract`

### #68 — FAIL
Run:
`36873547591`

Còn stale status string từ T:
`LOW_VECTOR_SHADOW_STATUS=diagnostic_hypothesis_disproved_as_complete_mapping_by_T_write_at_0x20`

Trong X phải là:
`LOW_VECTOR_SHADOW_STATUS=X_admits_only_W_observed_write_at_0x20_0x24_plus_fail_closed`

Fix commit:
- `3f15f6156971df13f0e2ae59dc6717aef234beea`
- `fix(machine1-x): align low-vector status test with X`

Các fail #65–#68 là **test/CI contract drift**, không phải hardware-model failure.

---

## 14. MACHINE1-X build cuối — PASS

Workflow:
**Build NGAGE-MACHINE1-X**

Run:
- **#69**
- run ID: `36875051410`
- head SHA:
  `3f15f6156971df13f0e2ae59dc6717aef234beea`
- conclusion:
  **SUCCESS**

Artifact:
- `EKA2L1-NGAGE-MACHINE1-X-IPA`
- artifact ID: `11169086924`
- size: `10,105,136` bytes
- expired: false

Đây là build X hợp lệ hiện tại.

---

## 15. Code HEAD cần coi là baseline

Baseline code X trước HANDOFF:
`3f15f6156971df13f0e2ae59dc6717aef234beea`

Hardware behavior tại baseline này:
- low shadow: 36 bytes;
- exact `0x20` admitted;
- `0x24+` fail-closed;
- W diagnostics vẫn hoạt động;
- Probe menu luôn visible;
- backend RH-29 gate vẫn giữ;
- không có remap claim mới.

Không rollback về:
- 32-byte T/W shadow;
- conditional Probe UI;
- broad low mapping;
- bất kỳ giả thuyết `0x0C150004 = remap`.

---

## 16. Device test X — exact next step

Cài artifact:
`EKA2L1-NGAGE-MACHINE1-X-IPA`

Test:

1. Mở app.
2. Xác nhận menu **RH-29 Machine Probe** đã hiện lại.
3. Chọn RH-29 firmware/device.
4. Chạy Probe một lần, budget mặc định 10,000 là đủ trước tiên.
5. Export:
   `RH29_MACHINE1_X.txt`
6. Gửi TXT.

Ưu tiên log, không cần video nếu UI không có thay đổi đáng kể.

---

## 17. Expected X observation

Khả năng dự kiến theo W:
- write `0x20` sẽ pass;
- loop có thể tiếp tục tới `0x24`;
- nếu không có control-flow/state change, `0x24` sẽ fail.

Nhưng **không được giả định trước**.
Phải đọc actual X log.

Nếu stop tại `0x24`:
- không đơn giản tăng 36 -> 40 bytes rồi lặp lại;
- bắt đầu provenance analysis cho R2/copy count.

---

## 18. Nếu X dừng tại 0x24 — bước kỹ thuật tiếp theo

Cần targeted trace quanh call/setup của copy routine:

Ưu tiên:
- `0x2300–0x2350`;
- caller tạo R0/R1/R2 trước khi vào `0x2340`;
- exact instruction cuối ghi R2;
- exact value progression của R2;
- relation với earlier low-address data read.

Mục tiêu:
**xác định vì sao R2 trở thành khoảng `0x28000326`.**

Không dùng workaround mapping để che nguồn lỗi.

Có thể cần MACHINE1-Y diagnostics:
- R2 provenance trace;
- callsite stack/caller trace;
- stop intentionally trước runaway copy;
- không đổi memory response.

---

## 19. Low-address interpretation hiện tại

Important prior evidence:

T/V/W cho thấy later data access ở low address 0 có thể không nên nhìn thấy reset ROM alias giống code fetch.

Đã từng thấy:
- data read address 0 trả `0xEA0000C9`;
- giá trị này đi qua shift/ORR;
- tạo value rất lớn;
- cuối cùng tạo copy count khoảng `0x28000326`.

Điều này vẫn là **hypothesis về alias/view problem**, chưa đủ để tự implement remap.

Cần source/device evidence cho:
- transition time;
- backing;
- range;
- trigger.

Nếu chưa có, giữ fail-closed.

---

## 20. Những điều tuyệt đối không được suy diễn

Không nói:
- `0x0C150004` chắc chắn là remap register;
- `0x0080` chắc chắn bật remap;
- `0x08000000` chắc chắn là SDRAM;
- `0x08000000` chắc chắn là memory-controller register block;
- `0x0A000000` chắc chắn là internal SRAM;
- low vectors chỉ có 8 words nên 0x20 là unrelated.

Chỉ được nói:
- exact accesses đã quan sát;
- structural match;
- candidate/hypothesis nếu chưa corroborated.

---

## 21. Useful source references / prior research

RH-29 service / hardware:
- ManualMachine RH-29 service manual
- Manualzilla baseband troubleshooting
- Nokia N-Gage schematic/service materials

Flashing/log evidence:
- GSMForum / GSMHosting RH-29 flashing logs
- JAF/UFS logs

Generic Symbian source:
`SymbianSource/oss.FCL.sf.os.kernelhwsrv`

Files:
- `kernel/eka/kernel/arm/bootutils.s`
  - `FindRamBankWidth`
  - `FindRamBankConfig`
  - `FindRamBankAddrMap`
  - `WordMove`
- `kernel/eka/kernel/arm/bootmain.s`
- `kernel/eka/include/kernel/arm/bootcpu.inc`

Dùng generic source như structural corroboration, không coi là exact RH-29 BSP.

---

## 22. Build monitor

Trong chat đã tạo một automation theo dõi MACHINE1-X mỗi giờ:
- nếu build đang queued/in_progress: không báo;
- nếu fail: đọc log, sửa branch `ngage-machine1`, giữ model X;
- nếu success: báo PASS + artifact.

Tuy nhiên khi bắt đầu chat mới, luôn kiểm tra GitHub live trước khi dựa vào trạng thái cũ.

---

## 23. Current final state

Tại thời điểm HANDOFF:

- MACHINE1-W device log đã phân tích xong.
- W xác nhận exact failed write ở `0x20`.
- X đã mở đúng word `0x20`, shadow = 36 bytes.
- `0x24+` vẫn fail-closed.
- Probe UI regression đã được sửa bằng always-visible menu entry.
- backend vẫn RH-29-only.
- mọi stale CI contract từ W/T đã được sửa.
- **MACHINE1-X #69 PASS**.
- artifact IPA đã có.
- **chưa có device log X** trong cuộc trò chuyện này.
- bước kế tiếp duy nhất: cài X, chạy Probe, gửi `RH29_MACHINE1_X.txt`.
- nếu blocker chuyển sang `0x24`, ưu tiên truy R2 provenance, không blind-expand shadow.

---

## 24. One-line continuation command

`Tiếp tục MACHINE1-X từ docs/handoff/NEWCHAT-NGAGE-QD-MACHINE1-X-2026-10-01.md. Build #69 PASS; hãy giữ nguyên X, chờ/đọc RH29_MACHINE1_X.txt và nếu stop ở 0x24 thì truy nguồn R2 trước, không mở rộng low shadow mù quáng và không suy diễn 0x0C150004 là remap register.`
