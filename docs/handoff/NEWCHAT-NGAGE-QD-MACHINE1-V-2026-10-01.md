# HANDOFF — EKA2L1 N-Gage QD RH-29 MACHINE1-V — 2026-10-01

## Lệnh bắt đầu chat mới

Tiếp tục dự án N-Gage QD RH-29 MACHINE1 từ HANDOFF `NEWCHAT-NGAGE-QD-MACHINE1-V-2026-10-01.md`. Repo `phai-nguyen/-EKA2L1-iOS-fixed`, nhánh `ngage-machine1`. MACHINE1-V đã build PASS và đã có device log. Không quay lại model cũ, không tự suy diễn `0x0C150004` là remap register. Trước MACHINE1-W, phân tích V và nghiên cứu thật hẹp DCT4 / UPP-WD2 cho `0x08000000` và `0x0C150004`.

---

## 1. Mục tiêu

Khảo sát boot kiểu full-machine/raw ARM của Nokia N-Gage QD RH-29 V04.10 trong EKA2L1 bằng một probe MACHINE1 tách riêng khỏi đường HLE đang hoạt động tốt.

MACHINE1:
- dùng fresh `arm::core` / Dyncom;
- đọc ROM RH-29 đã cài;
- parse EKA1 ROM header;
- chạy bounded ARM instructions;
- bus fail-closed;
- chỉ thêm hardware behavior khi có bằng chứng;
- không sửa/chiếm dụng global HLE system, kernel, scheduler, services, active HLE CPU.

Baseline HLE RH-29 hiện tốt:
- firmware import PASS;
- boot HLE PASS;
- apps PASS;
- FIFA 05 playable.

Không được làm hỏng:
- HYBRIDHOME3;
- VPL importer;
- Vietnamese localization;
- RH-29 HLE;
- game functionality;
- Bundle ID runtime `app.lavender1865.valley8348`;
- `EKA2L1_IOS_DYNARMIC=OFF`;
- min iOS 16.0 cho branch MACHINE1;
- artifact ZIP chỉ IPA.

Thiết bị test: iPhone 12 Pro Max, iOS 18.7.

---

## 2. Repo / branch / upstream

- Repo: `phai-nguyen/-EKA2L1-iOS-fixed`
- Branch: `ngage-machine1`
- HEAD trước HANDOFF:
  - `45fe1a3286cdb80daba9766658ddaf12f8d25217`
  - `feat(machine1): capture early setup registers in MACHINE1-V`
- Upstream pin:
  - `EKA2L1/EKA2L1@c5ae62d9c667f6e7060d76be58a2614122a629a4`
- Reference repo:
  - `phai-nguyen/EKA2L1-S60-Hybrid-Home`

---

## 3. RH-29 ROM facts

- Nokia N-Gage QD RH-29
- Firmware V 04.10 — 09-09-2004
- size `0x01170000` = 18,284,544 bytes
- MD5 `ba9ae3d584c42b24d44bf0f25ba5896f`
- SHA256 `4824ee55086cade3415c7478ec54b9654658bfdd737e779995dfe0ef716ab70b`
- ZIP SHA256 `3dc38659252ad35d11500a7ec8078e4f6eaf42d2f5a29ec3e5e1e522ee612230`
- EKA1 ROM base `0x50000000`
- root dir `0x50311000`
- 97 dirs, 1060 files
- RH-29 UID `0x101FB2B1`
- NEM-4 UID `0x101F8C19`
- không trộn Retrobios NEM-4 ROM với RH-29.

EKA1 header:
- 512 bytes;
- first 124 bytes = jump area;
- offset `0x7C restart_vector` là instruction/vector word, không phải pointer;
- `rom_base` 0x8C;
- `rom_size` 0x90;
- root list 0x94;
- kern_data 0x98;
- kern_limit 0x9C.

---

## 4. MACHINE1 tiến trình A → M

A:
- sai khi coi `restart_vector` là pointer;
- 0 instruction, invalid_rom.

B — `08a35ec7528719bc9e417a28499523b25a387e75a`
- reset PC=0;
- synthetic low ROM alias;
- CP15 barrier;
- 6 instructions;
- `MCR p15,0,r0,c1,c0,0` tại `0x2DC8`, value `0x1272`, MMU bit off.

C — `5a4aebfa763efba04790df626910050caec6dc90`
- emulate exact CP15 case;
- 19 instructions;
- blocker write32 `0x7037F800 -> 0x08000000` tại PC `0xB2C`.
- Lúc đó tạm gọi `0x08000000` candidate SDRAM. V hiện làm giả thuyết này đáng nghi.

D — `06bc2ae667908a6a9d847b39d2df012da912a290`
- mở candidate region 0x08000000 size 16 MiB, write-initialized-only;
- 5 writes / 20 bytes;
- blocker write16 `0x0080 -> 0x0C150004`, PC `0xB68`.

E — `7fcbb6b94ad19bdb01fa68b45146a72f17d1ec97`
- allow exact write `0x0C150004`;
- blocker write16 `0x00FF -> 0x02000000`.

F–M:
- dựng đúng AMD flash command flow cho second flash 0x02000000–0x027FFFFF;
- manufacturer ID 0x0001;
- device ID 0x277E;
- F0 exit;
- AA/55/90 unlock/autoselect;
- M chạy đến 258 instructions rồi blocker read32 `0x0A000000` tại PC `0x2664`, CPSR FIQ.

---

## 5. N → R: RAM probe

N:
- trace A32 cuối;
- xác định firmware tự chuyển sang FIQ;
- `E89800F0 = LDMIA r8,{r4-r7}`;
- R8=`0x0A000000`.

O:
- 64 A32 trace;
- `0x0A000000` đến từ bootstrap config table.
- Public Symbian source `kernel/eka/kernel/arm/bootutils.s` khớp cấu trúc mạnh:
  - `FindRamBankWidth`
  - `FindRamBankConfig`
  - `FindRamBankAddrMap`

Kết luận cho phép:
- `0x0A000000` = candidate physical RAM-bank address under test.

Không được kết luận:
- internal SRAM;
- CS3;
- second SDRAM;
- chip cụ thể.

P/Q/R:
- mở đúng sparse probe footprint theo evidence;
- tới R:
  - base `0x0A000000`
  - window 16 bytes
  - stride `0x3C`
  - 8 windows
- device xác nhận cả 8 windows hoàn tất.

---

## 6. S → T: low vector

S:
- post-probe workspace `0x0A0001E0`, size 0x20;
- budget mặc định 10,000;
- device chạy 1513 instructions;
- firmware đọc address 0, nhận first ROM word `0xEA0000C9`;
- thấy loop copy `LDR r3,[r1],#4` / `STR r3,[r0],#4`;
- stop khi ghi word đầu vào address 0.

T — `d5e753a7add1257ba9ba9ff0707acef96b05ed7f`
- exact 32-byte low-vector shadow `0x00–0x1F`, ROM-seeded, mutable write32.
- build #52 `36838349522`.
- device:
  - shadow writes=8;
  - firmware tiếp tục copy tới `0x20`;
  - dừng write32 `0 -> 0x20`.
- Vì vậy “chỉ 8 vector words là đủ” bị bác bỏ.

Quan trọng hơn:
- PC `0x12EC`: `LDR r2,[r1]`, R1=0;
- bus trả first ROM word `0xEA0000C9`;
- firmware biến thành `0xA0000C98`;
- sau đó thành loop count `0x28000326`.
=> synthetic ROM alias tại address 0 có vẻ sai cho **data access ở giai đoạn này**.
Không được chữa bằng blind shadow expansion.

---

## 7. MACHINE1-U

Mục tiêu:
- không đổi mapping;
- log chính xác 5 write vào vùng 0x08000000.

Build history:
- #53 `36843916342` fail controller test.
- #54 `36846288579` fail do test SyntaxError.
- #55 `36846798556` SUCCESS.
- U artifact:
  - `EKA2L1-NGAGE-MACHINE1-U-IPA`
  - id `11153634389`
  - SHA256 `5d4dbfc3124d25677f03418bdfff197a9281d5d4bca665db15a77d17c0a1eef5`

U device exact writes:
1. `0x08000000 <- 0x7037F800`, PC `0xB2C`, LR `0x344`
2. `0x08000004 <- 0xB2950489`, PC `0xB38`
3. `0x08000008 <- 0x00000037`, PC `0xB44`
4. `0x08000010 <- 0x20202112`, PC `0xB50`
5. `0x08000014 <- 0x20202112`, PC `0xB5C`

U vẫn dừng tại `0x20`.

---

## 8. MACHINE1-V — trạng thái cuối cùng

Commit:
- `45fe1a3286cdb80daba9766658ddaf12f8d25217`

V giữ nguyên model U, chỉ thêm full trace:
- PC `0xB00–0xB7F`
- instruction
- CPSR
- R0–R12
- SP/LR

Build:
- Build NGAGE-MACHINE1-V #56
- run `36850283187`
- SUCCESS
- artifact `EKA2L1-NGAGE-MACHINE1-V-IPA`
- artifact id `11155277403`
- size `10103952`
- SHA256 `abd682c0fdfad7795fe0d5e49c788fd66d43f0768e5863c239bb87202efb99df`

Device TXT:
- `RH29_MACHINE1_V.txt`
- 1484 lines
- budget 10,000
- executed 1545
- stop unresolved write32 `0x00000020`
- PC `0x2344`
- LR `0x3C8`
- value 0
- cause `rom_write`.

---

## 9. V exact early setup sequence

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

LR xuyên suốt = `0x344`.

### 0x0C150004 operation

```asm
0xB60 E59F004C   LDR  r0,[pc,#0x4C]   ; 0x0C150004
0xB64 E3A01080   MOV  r1,#0x80
0xB68 E1C010B0   STRH r1,[r0]
0xB6C E1A0F00E   MOV  pc,lr
```

Direct evidence:
- cả 5 write ở 0x08000000 và write halfword 0x80 đều là hard-coded setup sequence;
- chưa biết chức năng phần cứng cụ thể.

### Ý nghĩa

Tên `candidate_sdram_base` cho `0x08000000` hiện không còn đáng tin về semantics.
Pattern fixed offsets + magic values trong một setup routine **giống register/config block hơn ordinary RAM**, nhưng chưa đủ source/datasheet để đổi thành “memory controller”.

`0x0A000000` mới là candidate RAM probe có structural corroboration tốt hơn từ Symbian `FindRamBank*`.

---

## 10. Low address 0 vẫn là nút thắt

V vẫn cho thấy:

```asm
0x12EC E5912000   LDR r2,[r1]   ; r1=0
0x12F0 E1A02202   MOV r2,r2,LSL #4
0x12F4 E3822008   ORR r2,r2,#8
```

Bus hiện trả `0xEA0000C9`, rồi firmware tạo:
- `0xA0000C90`
- `0xA0000C98`

Sau đó tại:
- `0x2334 E2822003`
- `0x2338 E1B02122`

ra loop count `0x28000326`.

Rồi copy:

```asm
0x2340 E4913004   LDR r3,[r1],#4
0x2344 E4803004   STR r3,[r0],#4
0x2348 E2522001   SUBS r2,r2,#1
0x234C 1AFFFFFB   BNE 0x2340
```

=> current synthetic ROM alias-at-zero có vẻ đúng cho cold reset fetch nhưng rất có thể sai cho later data view.

Chưa biết transition xảy ra ở đâu.
Không được:
- mở 0x20/0x24/0x28 từng word;
- map toàn low memory writable;
- alias low 0 sang 0x08000000 hoặc 0x0A000000;
- coi `0x0C150004=0x80` là remap trigger khi chưa có source.

---

## 11. Nghiên cứu sâu đã làm

User yêu cầu 3 nhóm:
1. RH-29/DCT4 Bootloader / Low Vector / Emulator Hook.
2. Unicorn/QEMU shadow RAM/remap.
3. ARM920T vector table.

Hướng nghiên cứu đúng nhưng kết quả rộng ban đầu có contamination.

Giữ lại:
- QEMU/Unicorn remap/alias là reference tốt nếu trace chứng minh transition.
- ARM920T low vectors 0x00–0x1C là 8 instruction slots, không phải 8 function pointers.
- vector có thể dùng `B handler` hoặc `LDR pc,[pc,#imm]`.
- data/literal handler table có thể nằm sau 0x20.

Không tin / chưa dùng:
- RH-29 = OMAP claim.
- `0x0C150004` = remap register.
- `0x0A000000` = internal SRAM.
- mọi model C coi 8 vectors là function pointer table.

Public RH-29 service evidence đáng giữ:
- RH-29 dùng UPP WD2;
- 128 Mbit SDRAM + flash;
- chưa có exact CPU memory map/register map từ nguồn công khai đã tìm.

Exact GitHub searches:
- `0x7037F800`
- `0xB2950489`
- `0x0C150004`
không cho technical RH-29/WD2 match đáng tin; chủ yếu noise.

---

## 12. Deep Research tiếp theo — phải thật hẹp

Dùng exact fingerprint V, tìm:
- RH-29 service/level-3;
- DCT4;
- UPP WD2;
- UPP register map;
- Nokia bootstrap/flasher dumps;
- Griffin/JAF/UFS/Phoenix reverse engineering;
- exact constants:
  - `0x7037F800`
  - `0xB2950489`
  - `0x20202112`
  - `0x0C150004`
  - write16 `0x0080`.

Câu hỏi cần trả lời:
1. `0x08000000` là RAM hay MMIO/config/register block?
2. `0x0C150004` là register gì?
3. bit/value `0x80` làm gì?
4. có low-address remap sau reset không?
5. nếu có, trigger / backing / range là gì?

Không có source thì giữ unknown.

---

## 13. MACHINE1-W — CHƯA IMPLEMENT

Không code W ngay khi chưa xong research.

Hai hướng hợp lệ:

### W-DIAG
Nếu vẫn chưa biết register:
- trace literal pool quanh B90–BC0;
- trace callsite quanh LR 0x344;
- trace mọi data/code read/write low page 0x00000000–0x00000FFF;
- thêm sequence index trước/sau B68;
- không đổi memory response.

### W-REMAP
Chỉ khi có corroboration:
- model 2 pha:
  1. reset/code-fetch phase: ROM alias at zero;
  2. later phase: documented backing at zero.
- trigger/range/backing phải theo source.

---

## 14. Public Symbian source hữu ích

Repo:
`SymbianSource/oss.FCL.sf.os.kernelhwsrv`

Files:
- `kernel/eka/kernel/arm/bootutils.s`
  - `FindRamBankWidth`
  - `FindRamBankConfig`
  - `FindRamBankAddrMap`
  - `WordMove`
- `kernel/eka/kernel/arm/bootmain.s`
  - `BTF_RamBanks`
  - RAM bank iteration/test
- `kernel/eka/include/kernel/arm/bootcpu.inc`
  - `RAM_VERBATIM = 1`

Chỉ dùng như generic/later Symbian structural corroboration, không coi là exact RH-29 BSP.

---

## 15. Current exact next action

1. Không đổi hardware model.
2. Nghiên cứu hẹp exact V fingerprint.
3. Nếu không xác định được 0x0C150004 / 0x08000000:
   - làm W-DIAG.
4. Chỉ khi source corroborates mới làm W-REMAP.

Decision:
- nếu `0x0C150004` là remap control → implement exact state transition;
- nếu là timing/chip-select/clock/khác → không dùng làm remap trigger;
- nếu `0x08000000` là register block → đổi naming/semantics, không coi là SDRAM;
- nếu unknown → giữ fail-closed.

---

## 16. Build IDs cuối

- U #53: `36843916342` — fail
- U #54: `36846288579` — fail
- U #55: `36846798556` — success
- V #56: `36850283187` — success

V:
https://github.com/phai-nguyen/-EKA2L1-iOS-fixed/actions/runs/36850283187

---

## 17. Các project khác — không trộn

RM-356 CompatBoot/DirectHome, RM-356 Menu3→Home, EKA2L1 iOS localization/iOS15, WP7 Runner, N-Gage 2.0 S60v3/v5 là workstream riêng.

Active hiện tại là:
**N-Gage QD RH-29 MACHINE1 full-machine probe.**

---

## 18. Trạng thái cuối cuộc trò chuyện

User vừa gửi `RH29_MACHINE1_V.txt`.
Log đã đọc tới cuối.

Chốt:
- V hoàn thành diagnostic.
- 5 writes 0x08000000 là hard-coded setup sequence.
- `0x0080 -> 0x0C150004` là hard-coded exact operation.
- identity của hai vùng vẫn unknown.
- low address 0 synthetic ROM alias đang rất có khả năng sai ở later data-access phase.
- blocker `0x20` không được xử lý bằng blind shadow expansion.
- MACHINE1-W chưa tạo.
- targeted DCT4/UPP-WD2 research là bước tiếp theo.