# NEWCHAT — N-Gage QD RH-29 MACHINE1-AG — 2026-10-02

## 0. Mục đích HANDOFF

Tài liệu này chốt toàn bộ chuỗi nghiên cứu MACHINE1 đọc được đến thời điểm cuối cuộc trò chuyện hiện tại, để mở chat mới và tiếp tục ngay mà không quay lại các mốc cũ.

**Repo:** `phai-nguyen/-EKA2L1-iOS-fixed`  
**Branch:** `ngage-machine1`  
**Target:** Nokia N-Gage QD RH-29, firmware/ROM V04.10  
**Thiết bị test iOS:** iPhone 12 Pro Max, iOS 18.7  
**Ngôn ngữ trao đổi:** tiếng Việt  
**Nguyên tắc:** evidence-first, fail-closed; không mở rộng RAM/MMIO chỉ để boot tiến; không gán ngữ nghĩa cho `0x0C150004` khi chưa có bằng chứng trực tiếp.

---

## 1. Trạng thái cuối cùng

### 1.1 MACHINE1-AF đã test trên máy

File người dùng gửi có tên vật lý:

`RH29_MACHINE1_AE(1).txt`

Tên vẫn là AE vì harness/workflow chưa đổi identity, nhưng **logic trong build là AF**.

Kết quả AF trên thiết bị:

- instruction budget: `10000`
- executed instructions: **2122**
- stop reason: `unresolved_access`
- bootstrap copy:
  - base `0x0A000000`
  - size `0x108` = 264 bytes
  - 66 write32
  - 264 initialized bytes
- post-copy first-record mutation: accepted count **1**
- record-loop mutation: accepted count **15**
- tổng cộng firmware đã xử lý **16/16 record control**.
- nested-stack read/write counters trước blocker:
  - read count 7
  - write count 7
  - initialized bytes 16.

AF trace xác nhận loop đi qua cả các control low5=2 và low5=3, ví dụ:

- `0x30000023 -> 0xB0000023`
- `0x32000022 -> 0xB2000022`
- `0x30200023 -> 0xB0200023`

Sau khi record loop hoàn tất:

- PC `0x11E0`: `MOV r0,r5`
- PC `0x11E4`: `BL 0x2258`
- LR mới: `0x11E8`
- PC `0x2258`: instruction `0xE92D4030`
- decode: `STMDB sp!, {r4,r5,lr}`
- entry SP: `0x0A000FBC`
- footprint chính xác: `0x0A000FB0..0x0A000FBB` (12 bytes)

Blocker AF:

- `UNRESOLVED_KIND=data_write`
- width: 32 bit
- address: **`0x0A000FB0`**
- PC: **`0x00002258`**
- LR: **`0x000011E8`**
- value: `0x00000000`
- cause: `unmapped`

Điểm quan trọng: đây **không phải vùng stack mới**. Nó chính xác là footprint 12 byte đã được chứng minh và admit trước đó cho callsite `PC=0x2228/LR=0x11E0`. Vì vậy bước tiếp theo chỉ cần thêm callsite mới; không cần mở thêm RAM.

CP15 vẫn không đổi:

- PC `0x2DC8`
- instruction `0xEE010F10`
- value `0x1272`
- policy: observed ARM920T C1 write 0x1272, MMU off.

### 1.2 MACHINE1-AG đã triển khai

AG chỉ thêm đúng callsite mới:

- PC `0x2258`
- LR `0x11E8`
- instruction `0xE92D4030`
- stack top `0x0A000FBC`
- push base `0x0A000FB0`
- size `0x0C` = 12 bytes

AG **reuse** vùng nested stack đã có, không mở rộng một byte nào.

Tests mới:

- exact PC/LR + 12-byte footprint: PASS
- wrong LR: fail-closed
- address dưới `0x0A000FB0`: fail-closed
- cùng storage/counters với nested stack cũ.

Các commit AG:

1. `6955147e73460ef5fd1f870269cc837522892fdc`  
   `MACHINE1-AG define AF-observed fourth stack callsite`

2. `b23d7b1935a78560b596e818d005d8fceda54745`  
   `MACHINE1-AG admit exact 0x2258 stack reuse`

3. `90a06fcf2993100d5ee50f0f67359447e03efd66`  
   `test(machine1-ag): gate exact fourth stack reuse`

4. `53326fe05bbdcd2b8a8af7628279750964d792eb`  
   docs handoff update; branch HEAD tại thời điểm chốt HANDOFF.

### 1.3 Build AG

GitHub Actions:

- workflow run: **#110**
- run ID: **36964784995**
- kết quả: **SUCCESS**
- code head được build: `90a06fcf2993100d5ee50f0f67359447e03efd66`
- workflow vẫn hiển thị tên: `Build NGAGE-MACHINE1-AE`
- artifact:
  - **`EKA2L1-NGAGE-MACHINE1-AE-IPA`**
  - artifact ID: `11209700361`
- run URL:
  - `https://github.com/phai-nguyen/-EKA2L1-iOS-fixed/actions/runs/36964784995`
- artifact API zip:
  - `https://api.github.com/repos/phai-nguyen/-EKA2L1-iOS-fixed/actions/artifacts/11209700361/zip`

**Lưu ý:** user yêu cầu từ các lần build sau phải đưa link build trực tiếp trong câu trả lời.

### 1.4 Việc cần làm ngay ở chat mới

1. Cài artifact từ build #110.
2. Chạy RH-29 Machine Probe với budget **10000**.
3. Chia sẻ file TXT mới.
4. File report có thể vẫn mang tên `RH29_MACHINE1_AE.txt` do harness identity cũ; hãy nhận diện nó là **AG logic** dựa vào build #110 / code SHA.
5. Kiểm tra:
   - `BOOTSTRAP_NESTED_STACK_WRITE_COUNT` có tăng thêm 3 write32 cho callsite `0x2258` hay không.
   - blocker mới là gì.
6. Chỉ xử lý blocker tiếp theo theo đúng evidence; không mở stack page/RAM rộng.

---

## 2. Chuỗi tiến triển MACHINE1 quan trọng

### MACHINE1-X

Baseline trước khi sửa FIQ banking.

- low-vector shadow chỉ **36 byte**
- dừng tại `0x24`
- không được mở rộng low shadow mù quáng.
- điều tra cho thấy nguồn lỗi thực sự là register banking khi chuyển SVC <-> FIQ.
- tuyệt đối không suy diễn `0x0C150004` là remap register.

Build X đã PASS trước đó.

### MACHINE1-Y

Sửa Dyncom để giữ đúng **R8-R12** khi chuyển SVC <-> FIQ.

Kết quả:

- R11=`0x744` sống qua FIQ.
- R2 trở thành đúng `0x42`.
- firmware chạy copy loop tại `0x2340/0x2344/0x2348/0x234C`.
- blocker chuyển sang vùng `0x0A000000`, chứng minh X không nên mở low shadow.

Y là fix kiến trúc phải giữ nguyên về sau.

### MACHINE1-Z

Từ trace Y suy ra copy table chính xác:

- source: `0x744`
- destination: `0x0A000000`
- size: `0x108`
- 66 write32
- initialized-only readback.

Z không map generic RAM; chỉ model exact copied bytes.

Sau copy:

- firmware dùng `0x0A000000` làm base.
- PC `0x3AC`, instruction `E280DEFF`, tạo SP `0x0A000FF0`.
- blocker kế tiếp là stack push.

### MACHINE1-AA

Admit first observed stack push:

- SP top `0x0A000FF0`
- base `0x0A000FD0`
- size 32 bytes
- PC `0x11A8`
- LR `0x3C8`
- instruction `E92D47F0`
- exactly 8 write32.

Device AA chứng minh:

- write count 8
- initialized bytes 32
- push hoàn tất.

Sau đó:

- PC `0x11AC`: `SUB sp,sp,#0x14`
- SP -> `0x0A000FBC`
- call `0x11C4 -> 0x22A0`
- LR `0x11C8`
- `0x22A0: E92D4070` = `STMDB sp!, {r4-r6,lr}`
- blocker tại `0x0A000FAC`.

### MACHINE1-AB

Admit exact second/nested push:

- top `0x0A000FBC`
- base `0x0A000FAC`
- size 16 bytes
- PC `0x22A0`
- LR `0x11C8`
- instruction `E92D4070`.

Không map gap `0x0A000FBC..0x0A000FCF` chỉ vì SP đi qua đó; SUB SP không chứng minh memory access.

### MACHINE1-AC

Sau nested push, firmware đi tới post-copy mutation / call chain tiếp theo.

Một stack push 12 byte xuất hiện:

- top `0x0A000FBC`
- base `0x0A000FB0`
- size 12
- PC `0x2228`
- LR `0x11E0`
- instruction `E92D4030`.

Footprint hoàn toàn nằm trong 16-byte nested window đã có; không mở range.

### MACHINE1-AD

Gộp/cho phép second + third stack use trên cùng 16-byte nested storage, exact-gated.

Record table bắt đầu được xử lý sâu hơn.

### MACHINE1-AE

Thiết bị cho thấy helper record loop `0x21F4..0x2220`.

Các điểm chính:

- table có 16 record x 16 byte sau header 8 byte.
- control field ở offset +8.
- record loop stride `0x10`.
- write transform tại:
  - PC `0x2220`
  - LR `0x2244`
  - instruction `E5803008`.
- firmware logic:
  - lấy low5
  - clear mask `0x60`
  - OR path tạo bit high/control.
- AE ban đầu chỉ admit low5=1.

Device AE dừng khi gặp control low5=3:

- old `0x31000023`
- new `0xB1000023`
- write target thuộc control slot của record.
- đây là bằng chứng rằng low5==1 là gate do model tự giới hạn, không phải firmware.

### MACHINE1-AF

AF mở đúng set low5 **1,2,3** vì đây là các giá trị thấy trong copied table; vẫn giữ:

- exact table footprint
- exact aligned control +8 slot
- initialized bytes only
- write32
- PC `0x2220`
- LR `0x2244`
- old bit31 clear
- exact transformed value.

Không mở RAM/MMIO.

AF device test đã hoàn tất toàn bộ 16 record và đi tới blocker stack mới tại `0x2258`, dẫn tới AG.

---

## 3. Firmware table / memory evidence phải giữ

### Copied table

Source `0x744`:

- header 8 byte
- 16 records x 16 byte
- total `0x108`.

Các control/value đã quan sát gồm:

- nhiều `0x32800021`
- `0x31000023`
- `0x33400023`
- `0x30000023`
- `0x32000022`
- `0x30200023`.

### Main-SDRAM candidate

Table evidence mạnh nhất hiện tại:

- base `0x0A000000`
- size `0x01000000` = 16 MiB
- control `0x32000022`.

Đây là **strongest candidate** cho main SDRAM, nhưng model vẫn không nên broad-map 16 MiB chỉ từ inference này.

### 0x08000000

Table có entry:

- base `0x08000000`
- size `0x1000`
- control `0x30000023`.

Điều này mâu thuẫn với nhãn cũ coi `0x08000000` là 16-MiB SDRAM chính. Tuy nhiên code vẫn có sparse candidate writes/traces ở vùng này; không tự ý tái cấu trúc rộng khi chưa cần.

### 0x0C000000 region

Table có entry:

- base `0x0C000000`
- size `0x00200000`
- control `0x31000023`.

Observed `0x0C150004` nằm trong vùng này, nhưng điều đó **không chứng minh** địa chỉ này là remap register.

### Early observed MMIO

Exact observed transaction:

- address `0x0C150004`
- width 16
- value `0x0080`.

Giữ nguyên policy:
- exact observed write only
- hardware identity unknown.

### Flash evidence

RH-29 second flash window:

- base `0x02000000`
- size `0x00800000`.

Observed AMD-style commands:

- write `0x00FF` at `0x02000000`
- ID entry `0x0090` at `0x0200AAAA`
- manufacturer ID `0x0001`
- device ID first word `0x277E`
- F0 exits autoselect
- unlock1: `0x00AA` at `0x02000AAA`
- unlock2: `0x0055` at `0x02000554`
- command `0x0090` after unlock.

Không generic-map flash reads ngoài các response đã evidenced.

---

## 4. Invariants / safety rules cho mọi bản tiếp theo

1. **Không quay lại mốc cũ.**
2. Giữ Y FIQ banking fix.
3. Giữ Z exact bootstrap copy `0x108`.
4. Giữ AA first stack exact 32 byte.
5. Giữ AB exact nested stack 16 byte.
6. Giữ AC/AD exact 12-byte reuse.
7. Giữ AF exact record-loop transform logic.
8. Không map một trang 4 KiB chỉ vì stack chạm một địa chỉ.
9. Không broad-map 16 MiB `0x0A000000` chỉ vì table gợi ý SDRAM.
10. Không mở low vector shadow trên 36 byte nếu chưa có transaction mới.
11. Không gán `0x0C150004` là remap register.
12. Không bịa semantics cho control low5 1/2/3; chỉ mô tả chúng là observed control low5 values.
13. Mọi admission mới phải có:
    - exact address/range
    - exact width
    - exact PC/LR nếu có thể
    - exact value/transform nếu firmware chứng minh
    - negative test fail-closed.
14. Mỗi build mới phải đưa **link GitHub Actions trực tiếp** cho user.
15. Khi TXT mới về: đọc blocker trước, lần ngược registers/flow trước khi mở memory.

---

## 5. iOS probe UX

User đã yêu cầu nút **Chia sẻ báo cáo** nằm ngay dưới **Chạy probe** để không phải kéo xuống cuối TXT dài.

Đã sửa:
- ShareLink ở ngay dưới nút chạy probe.
- chỉ hiện khi report URL tồn tại.
- bỏ duplicate ShareLink ở cuối report.
- test đảm bảo ShareLink nằm trước phần long report.

Commit UI trước đó:
- `018ac06faad66d15a414d006aed1876ad64994cb`
- test placement commit `b3aa5b673ee1ae33e24b301dd90e230ed1e44887`.

---

## 6. Build / workflow caveat

Workflow hiện vẫn mang identity **MACHINE1-AE** vì các chỉnh sửa CI identity từng bị connector safety block.

Do đó:

- run name có thể vẫn là `Build NGAGE-MACHINE1-AE`
- artifact có thể vẫn là `EKA2L1-NGAGE-MACHINE1-AE-IPA`
- report file có thể vẫn là `RH29_MACHINE1_AE.txt`

Không được nhầm identity file với logic thực tế.

Để nhận diện logic:
- dùng GitHub run number
- dùng head SHA
- đối chiếu commit code.

Build #110 = **AG logic** dù label AE.

---

## 7. User/test preferences cần giữ

- Trả lời bằng tiếng Việt.
- User không biết tiếng Anh.
- User thích tiếp tục tự động, không hỏi lại khi đã đủ dữ liệu.
- User đã cho phép tự sửa repo/build.
- Khi build xong: luôn đưa link trực tiếp.
- Khi test app thông thường user hay dùng 3 cách: cài sạch / cài đè xoá log / cài đè giữ log; riêng MACHINE1 probe hiện ưu tiên đúng artifact + report TXT.
- Không cần yêu cầu user dùng Mac; user không có Mac.
- PC user: Windows 7 x64, 2 GB RAM.
- iPhone test: iPhone 12 Pro Max iOS 18.7.

---

## 8. Câu lệnh mở chat mới

Dùng nguyên văn:

`Tiếp tục MACHINE1-AG từ docs/handoff/NEWCHAT-NGAGE-QD-MACHINE1-AG-2026-10-02.md trong repo phai-nguyen/-EKA2L1-iOS-fixed, nhánh ngage-machine1. Build #110 PASS ở code SHA 90a06fcf2993100d5ee50f0f67359447e03efd66; artifact vẫn mang nhãn AE nhưng logic là AG. Chờ/đọc TXT thiết bị mới; blocker AF cũ là write32 0x0A000FB0 tại PC=0x2258/LR=0x11E8, và AG chỉ admit đúng callsite đó trên footprint 12 byte đã có. Không broad-map RAM/stack, không mở low shadow, không suy diễn 0x0C150004 là remap register.`

---

## 9. Mốc kết thúc HANDOFF

Tại thời điểm chốt:

- branch HEAD: `53326fe05bbdcd2b8a8af7628279750964d792eb`
- latest code build: #110 PASS
- build code SHA: `90a06fcf2993100d5ee50f0f67359447e03efd66`
- artifact: `EKA2L1-NGAGE-MACHINE1-AE-IPA`
- next action: **device-test AG and read the next TXT blocker**.
