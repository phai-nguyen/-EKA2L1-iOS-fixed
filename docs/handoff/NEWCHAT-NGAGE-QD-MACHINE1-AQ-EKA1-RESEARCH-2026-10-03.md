# NEW CHAT HANDOFF — N-Gage QD MACHINE1-AQ + EKA1 mapping research

Date: 2026-10-03  
Repo: `phai-nguyen/-EKA2L1-iOS-fixed`  
Branch: `ngage-machine1`  
Target: Nokia N-Gage QD RH-29 V04.10  
Current branch HEAD at handoff creation: `23c5b741efd23da731c45b98ab52db23faa0a2d4`

## 0. One-line continuation state

MACHINE1-AP device report is the latest device evidence. It PASSes the exact relocated 8-word stack push, executes 3017 instructions, then stops on the first new local-stack store:

- data_write width32
- address `0x09FFF3AC`
- PC `0x00000F24`
- LR `0x00000F18`
- instruction `0xE58DC024` = `STR r12,[sp,#0x24]`
- value `0x00000000`
- cause `unmapped`.

MACHINE1-AQ has already been implemented to admit **only this exact store** and its initialized readback, with no 0x54-byte frame widening. Build #132 is SUCCESS and awaits device test / `RH29_MACHINE1_AQ.txt`.

During the latest research pass, the RH-29 boot table and EKA1 ROM mapping were correlated against the **correct-era Series 60 6.1 SDK** and independent N-Gage/EKA1 sources. The research was committed in:

- `23c5b741efd23da731c45b98ab52db23faa0a2d4`
- message: `docs(machine1): record EKA1 ROM mapping research`
- file: `docs/research/RH29-BOOTSTRAP-REGION-TABLE-2026-10-02.md`.

## 1. Latest device report: MACHINE1-AP

Input report:

- `RH29_MACHINE1_AP.txt`
- identity: `RH29_MACHINE1_AP`
- `ROM_BASE=0x50000000`
- `ROM_SIZE=0x01170000`
- budget 10000
- executed instructions: **3017**
- stop: `unresolved_access`
- CP15 C1 evidence unchanged:
  - PC `0x2DC8`
  - instruction `EE010F10`
  - value `0x1272`
  - emulated
  - policy remains MMU off.

### 1.1 AP exact relocated push confirmed

Before F18:

- helper `0x3FC..0x40C` sets
  - SP = `0x09FFF3FC`
  - LR = `0x00000F18`
  - R0 = `0x0A000FBC`
  - R1 = 0.

At `0xF18`:

- instruction `E92D47F0` = `STMDB sp!,{r4-r10,lr}`
- exact footprint `0x09FFF3DC..0x09FFF3F8`
- 8 exact 32-bit words:
  1. `0x09FFF3DC = 0x0A000000` (r4)
  2. `0x09FFF3E0 = 0x0A000000` (r5 before F20 reload)
  3. `0x09FFF3E4 = 0x00000000` (r6)
  4. `0x09FFF3E8 = 0x01170000` (r7)
  5. `0x09FFF3EC = 0x00000000` (r8 before F20 reload)
  6. `0x09FFF3F0 = 0x00000000` (r9)
  7. `0x09FFF3F4 = 0x00000080` (r10)
  8. `0x09FFF3F8 = 0x00000F18` (lr).

AP proves the exact push gate is correct; do not generalize it to a stack range.

### 1.2 AP path after the push

Low-page/device trace:

- `0xF18 E92D47F0`
- `0xF1C E24DD054` -> `SUB sp,sp,#0x54`
  - SP becomes `0x09FFF388`
- `0xF20 E8901120` -> loads words from `r0=0x0A000FBC`
- `0xF24 E58DC024` -> first new store
- prefetched/visible next instructions:
  - `0xF28 E590A00C`
  - `0xF2C E5904010`
  - `0xF30 E598C00C`
  - `0xF34 E58DC018`
  - `0xF38 E1A00008`
  - `0xF3C EB0006ED`.

Register snapshot at AP stop:

- R0=`0x0A000FBC`
- R1=`0`
- R2=`0x0A000FBC`
- R3=`0`
- R4=`0x0A000000`
- R5=`0x09FFF400`
- R6=`0`
- R7=`0x01170000`
- R8=`0x00000080`
- R9=`0`
- R10=`0x00000080`
- R11=`0x0A000000`
- R12=`0`
- SP=`0x09FFF388`
- LR=`0x00000F18`
- PC=`0x00000F28`
- CPSR=`0x600000D3`.

Exact unresolved transaction:

- `UNRESOLVED_KIND=data_write`
- width 32
- `UNRESOLVED_ADDRESS=0x09FFF3AC`
- `UNRESOLVED_PC=0x00000F24`
- `UNRESOLVED_LR=0x00000F18`
- `UNRESOLVED_VALUE=0`.

## 2. MACHINE1-AQ already implemented

AQ was implemented immediately from AP device evidence.

### 2.1 Exact gate

Constants:

- address: `0x09FFF3AC`
- width: 4 bytes
- PC: `0x00000F24`
- LR: `0x00000F18`
- instruction: `0xE58DC024`
- value: `0x00000000`.

Static relationship:

- AQ word is `candidate_bootstrap_relocated_stack_base - 0x30`
- i.e. exact `SP+0x24` after `SUB sp,sp,#0x54`.

Policy string:

`BOOTSTRAP_RELOCATED_LOCAL_WORD_POLICY=AP_device_exact_sp_plus_0x24_addr_0x09fff3ac_pc_0x0f24_lr_0x0f18_value_0_initialized_readback_only_no_frame_widen`

AQ allows only:

1. exact write at that address with exact width/PC/LR/value;
2. later readback only if initialized.

Everything else in the 0x54-byte reservation remains unmapped.

### 2.2 AQ commit chain

Staging/build commits:

- `d45b7703173b003fde50eb19e56bd1c89e21ce24` — stage workflow without intermediate builds
- `adf2bffab6dadc730c9c80e216c57263ff9728aa` — define exact relocated local word
- `31646265596a0138d40cbd71245eae75c7a449bd` — gate exact sp+0x24 store
- `56cc742ba0af948c5569ecb49564230a41d84af8` — fail-closed model tests
- `886da81208a4920abbee05093df9ababbec79803` — carry local-word counters
- `c1d000935bad2cadc526f7005587723841559d87` — report exact local-word evidence
- `c3a3417ef0348015d749648fec1f32495ffd36ce` — report tests
- `2f82bc3b7f4f027d596f533fbf7e0a92ade0a90f` — stage AQ identity in iOS integration
- `019014237520239660c6e2f78c38a73b3303caf8` — verify staged AQ contracts
- `406548120ef857a1c65aa7db423ae226ad023944` — enable AQ workflow
- `f9eee740f26e38291c588b8d73bb74d5496df910` — trigger exact sp+0x24 build.

Expected device report filename:

- **`RH29_MACHINE1_AQ.txt`**

### 2.3 Build #132

GitHub Actions:

- run number: **132**
- run ID: **37019841827**
- name: `Build NGAGE-MACHINE1-AQ`
- head: `f9eee740f26e38291c588b8d73bb74d5496df910`
- status: **completed / success**
- Actions URL:
  `https://github.com/phai-nguyen/-EKA2L1-iOS-fixed/actions/runs/37019841827`

Artifact:

- name: `EKA2L1-NGAGE-MACHINE1-AQ-IPA`
- artifact ID: **11233745298**
- size: 10,116,170 bytes
- SHA-256:
  `ba96ea84fc67423eb900b8e764a081036c81dd9a954f65244eed0f6ce3770ab9`
- expires: 2026-12-31.

## 3. New EKA1/Symbian 6.1 research findings

Latest research deliberately used contemporaneous EKA1/Symbian 6.1 material instead of silently importing later EKA2 semantics.

### 3.1 Correct-era TRomHeader layout

Preserved Series 60 6.1 SDK source in `ngagesdk/sdk`, file:

- `6.1/Shared/EPOC32/Include/e32rom.h`

gives a 0x100-byte EKA1 `TRomHeader`.

Relevant offsets:

| Offset | Field |
|---:|---|
| +0x80 | iVersion |
| +0x84 | iTime |
| +0x8C | iRomBase |
| +0x90 | iRomSize |
| +0x94 | iRomRootDirectoryList |
| +0x98 | iKernDataAddress |
| +0x9C | iKernStackAddress |
| +0xA0 | iPrimaryFile |
| +0xA4 | iSecondaryFile |

Critical device correlation:

- AP trace reads physical low-ROM `0x00000090` at PC `0x1200`
- returned value: **`0x01170000`**
- correct-era meaning: **`TRomHeader::iRomSize`**.

This same `0x01170000` was stored by AO at `0x0A000FCC`.
Therefore when F20/F2C consumes `[r0,#0x10]`, that field can now be called ROM size with strong source backing.

### 3.2 EKA1 logical ROM base 0x50000000

The same Series 60 6.1 SDK contains:

- `Shared/EPOC32/Tools/hpsym.pl`

with default:

`my $rombase = 0x50000000;`

and describes it as logical-address offset from physical ROM address.

Independent corroboration:

- RH-29 report already parses `ROM_BASE=0x50000000`;
- old N-Gage map preserved by Engemu places ROM image at `0x50000000–0x57FFFFFF`;
- EKA2L1 has explicit EKA1 constants with ROM base `0x50000000`;
- other EPOC/ER5 preservation work also uses `TRomHeader::iRomBase` at +0x8C and logical ROM base 0x50000000.

Important interpretation:

- reset/bootstrap currently executes low **physical** ROM while MMU is off;
- 0x50000000 is the intended EKA1 **logical** ROM mapping after translation setup;
- do not pre-map 0x50000000 merely because its eventual role is known.

### 3.3 Historical N-Gage/EKA1 virtual layout

THC-derived map preserved in Engemu:

- `0x40000000–0x40001FFF`: Super page + CPU page
- `0x40010000–0x40010FFF`: shadow RAM temporary
- `0x41000000–0x41003FFF`: Page Directory
- `0x41080000–0x41083FFF`: Page-table info
- `0x42000000–0x423FFFFF`: Page tables
- `0x50000000–0x57FFFFFF`: ROM image
- `0x58000000–0x5EFFFFFF`: memory-mapped I/O
- `0x5F000000–0x5FFFFFFF`: video RAM
- `0x60000000–0x7FFFFFFF`: RAM
- high kernel area from `0x80000000`.

EKA2L1 commit:

- `17c005e982921010f40cdb15b51f433d22cb65ee`
- message: `kernel: Support page table injection for EKA1`

independently records EKA1 page-directory handling and descriptor low-bit behavior.

Use this layout as **architectural corroboration**, not as permission to broad-map those virtual ranges before RH-29 device execution actually reaches MMU setup.

## 4. Recovered RH-29 16-entry physical region table

Z copied 0x108 bytes from low ROM source `0x744` to `0x0A000000`:

- header word0 = 16
- header word1 = 1
- then 16 records × 16 bytes.

Raw records:

| # | base | size | control | +C |
|---:|---:|---:|---:|---:|
| 0 | 0x00000000 | 0x0000A000 | 0x32800021 | 0 |
| 1 | 0x0000A000 | 0x000F6000 | 0x32800021 | 0 |
| 2 | 0x00100000 | 0x00200000 | 0x32800021 | 0 |
| 3 | 0x00300000 | 0x00011000 | 0x32800021 | 0 |
| 4 | 0x00311000 | 0x000EF000 | 0x32800021 | 0 |
| 5 | 0x00400000 | 0x00B00000 | 0x32800021 | 0 |
| 6 | 0x02000000 | 0x00100000 | 0x32800021 | 0 |
| 7 | 0x02100000 | 0x00300000 | 0x32800021 | 0 |
| 8 | 0x00F00000 | 0x000F0000 | 0x32800021 | 0 |
| 9 | 0x0C000000 | 0x00200000 | 0x31000023 | 0 |
| 10 | 0x0D000000 | 0x01000000 | 0x31000023 | 0 |
| 11 | 0x02400000 | 0x00400000 | 0x31000023 | 0 |
| 12 | 0x02400000 | 0x00400000 | 0x33400023 | 0 |
| 13 | 0x08000000 | 0x00001000 | 0x30000023 | 0 |
| 14 | 0x0A000000 | 0x01000000 | 0x32000022 | 0 |
| 15 | 0x0C120000 | 0x00001000 | 0x30200023 | 0 |

Strongly established:

- +0 = physical base
- +4 = size
- +8 = mutable control/attribute state
- +C = currently zero in all records.

Do not label +8 as a raw ARM PDE yet.

## 5. Stronger, still conservative classification of low5 classes

The copied control words use low5 values 1, 2, 3.

### low5 = 2

Record 14:

- base `0x0A000000`
- size `0x01000000`
- control `0x32000022`.

The executed handler at `0x1328` performs a power-of-two sparse address-line test over this region.

This is strong device evidence for a **RAM-bank detection path/class**.

### low5 = 1

Most low5=1 records cover low physical flash families.

Independent RH-29 UFS evidence:

- Fl0 `0x00000000–0x00FFFFFF`: AMD 29BDS128J
- Fl1 `0x02000000–0x027FFFFF`: AMD 29BDS064J.

Thus low5=1 strongly correlates with a **flash/ROM-like boot handling class**.

Do not call it a one-to-one flash type because the upper second-flash area overlaps low5=3 records too.

### low5 = 3

Includes:

- `0x0C000000`
- `0x0D000000`
- `0x02400000`
- `0x08000000`
- `0x0C120000`.

Current safest name:

- **special/fixed/hardware mapping path/class**.

Do not reduce this to “MMIO” yet.

The full high-bit packing remains unresolved.

## 6. Most likely F18 semantic direction — hypothesis only

Because F18 already:

- carries ROM size `0x01170000`;
- is about to read `TRomHeader::iRomBase` at physical low-ROM +0x8C;
- executes while MMU is still off;
- then branches to `0x2AF8`;

the strongest current hypothesis is that this path is assembling ROM/MMU bootstrap metadata immediately before a translation/mapping setup helper.

This is **not yet enough** to name function `0x2AF8`.

The device must execute through F28/F30/F34 and into 0x2AF8 before assigning an exact role.

## 7. Predicted next AP→AQ sequence — do not model ahead of evidence

If AQ accepts F24, already visible instructions imply:

1. `F28: LDR r10,[r0,#0x0C]`
   - exact source around `0x0A000FC8`
   - expected 0.

2. `F2C: LDR r4,[r0,#0x10]`
   - source `0x0A000FCC`
   - device value already `0x01170000`
   - now source-backed as `TRomHeader::iRomSize`.

3. `F30: LDR r12,[r8,#0x0C]`
   - after F20, r8 came from local frame and AP snapshot shows r8=`0x80`
   - address = `0x8C`
   - correct-era expected value:
     `TRomHeader::iRomBase = 0x50000000`.

4. `F34: STR r12,[sp,#0x18]`
   - with SP `0x09FFF388`
   - target = **`0x09FFF3A0`**
   - predicted value `0x50000000`.

5. `F38: MOV r0,r8`
6. `F3C: BL 0x2AF8`.

These are predictions from visible code + correct-era TRomHeader semantics. They are **not** permissions to add AQ+1/F34 mappings before the AQ device report.

## 8. Checkpoint timeline to preserve

### X
- low-vector shadow only **36 bytes**
- blocker at 0x24 triggered register-bank investigation
- no widening
- do not infer `0x0C150004` as remap.

### Y
- fixed Dyncom SVC↔FIQ R8–R12 banking
- R11 `0x744` survives FIQ
- R2 `0x42`
- correct bootstrap copy path.

### Z
- exact copy source `0x744`
- destination `0x0A000000`
- size `0x108` / 264 bytes
- 66 write32
- initialized-only readback.

### AA
- first exact stack push
- top `0x0A000FF0`
- base `0x0A000FD0`
- 32 bytes
- PC `0x11A8`
- LR `0x3C8`
- `E92D47F0`.

### AB
- nested 16-byte push
- `0x0A000FAC..0x0A000FBB`
- PC `0x22A0`
- LR `0x11C8`
- `E92D4070`.

### AC/AD
- exact 12-byte reuse
- `0x0A000FB0..0x0A000FBB`
- PC `0x2228`
- LR `0x11E0`
- `E92D4030`.

### AE/AF
- firmware table loop recovered
- 16 records × 0x10
- low5 classes observed: 1, 2, 3
- exact transform only.

### AG
- exact stack reuse
- PC `0x2258`
- LR `0x11E8`
- `E92D4030`.

### AH
- `0x1BB4 E92D40F0`
- 20-byte push
- only one new word `0x0A000FA8..AB`; upper 16 reused.

### AI
- `0x1438 E92D4010`
- exact 8-byte push
- `0x0A000FA0..A7`.

### AJ
- low5=2 dispatch enters `0x1328`
- `E92D41F0`
- exact 24-byte stack `0x0A000F88..9F`
- handler begins main-RAM probe setup.

### AK
- exact first anchor `0x0A001100`
- synthetic zero seed, not real hardware RAM content
- raw handler proves power-of-two address-line probe
- next blocker `0x0A005100 @ 0x137C`.

### AL
- exact ten sparse RAM-probe points only:
  offsets `0x4000, 0x8000, 0x10000, 0x20000, 0x40000, 0x80000, 0x100000, 0x200000, 0x400000, 0x800000`
  relative to anchor `0x0A001100`
- device PASSes sparse loop
- no 16-MiB range map
- next blocker local word `0x0A000FBC`.

### AM
- exact local-frame word:
  - addr `0x0A000FBC`
  - PC `0x12AC`
  - LR `0x1270`
  - value `0x09FFF400`
- device PASS
- next blocker relocation destination `0x09FFF400`.

### AN
- exact source-verified `0x108` relocation
  - source `0x0A000000`
  - destination `0x09FFF400`
  - PC `0x2344`
  - LR `0x12B8`
- 66 exact words / 264 bytes
- device PASS
- next blocker `0x0A000FC0`.

### AO
- exact four tail words:
  - FC0 @ 12B8 = `0x80`
  - FC4 @ 12BC = 0
  - FC8 @ 12C0 = 0
  - FCC @ 12C4 = `0x01170000`
- device PASS all 4
- then helper relocates SP and reaches F18.

### AP
- exact 8-word relocated push `0x09FFF3DC..F8`
- PC/LR `0xF18`
- `E92D47F0`
- device PASS
- new blocker `0x09FFF3AC @ F24`.

### AQ
- already coded, built, PASS CI
- admits only `0x09FFF3AC = 0`
- PC `0xF24`
- LR `0xF18`
- instruction `E58DC024`
- device test **pending**.

## 9. Build history relevant to latest progression

- #127 AL — SUCCESS — run `37000204979`
  - artifact `11223094821`
  - SHA-256 `3ab653b13cb9f1791120b942530eec96797fab66d8d82eddbeb18828532d9245`

- #128 AM — SUCCESS — run `37004905190`
  - artifact `11225038665`
  - SHA-256 `830242080f0f99727ad93254605bd9513b41f98c841b4609aac52066cd36fc82`

- #129 AN — SUCCESS — run `37009679125`
  - artifact `11227800289`
  - SHA-256 `638ae0c57754448437b8b5beba8abb4c9b3e9c15e86b82d8d8aeb3e8d1711f4a`

- #130 AO — SUCCESS — run `37014166865`
  - artifact `11229133025`
  - SHA-256 `c4f88f8b368907efc5f8da81d379c19ac92fb2a37faa49a4855cd0de2bb3bc9f`

- #131 AP — SUCCESS — run `37016972531`
  - artifact `11231491732`
  - SHA-256 `69d4d9a26fd6d4a6a33b21cc9a15cd9708ea28f3012d969f999e61bf62774b7d`

- #132 AQ — SUCCESS — run `37019841827`
  - artifact `11233745298`
  - SHA-256 `ba96ea84fc67423eb900b8e764a081036c81dd9a954f65244eed0f6ce3770ab9`.

## 10. Permanent safety/evidence rules

These are non-negotiable for future MACHINE1 steps:

1. Never revert older verified checkpoints just to advance.
2. Preserve Y SVC↔FIQ R8–R12 fix.
3. Preserve Z exact 0x108 table copy.
4. Preserve all exact stack/control gates AA through AQ.
5. Low-vector shadow remains exactly 36 bytes.
6. Never broad-map RAM/stack just to make the PC move.
7. AL stays sparse points only; no generic `0x0A000000..0x0AFFFFFF` RAM map.
8. Synthetic sparse-probe zero seeds are model hypotheses, not device-observed RAM contents.
9. `0x0C150004` semantics remain unknown; do not call it a remap register.
10. CP15 C1 observation remains `0x1272`; MMU remains off at the current stop.
11. Every new admission should be exact address + width + PC + LR + value, or a source-derived value only when that derivation is proven.
12. Maintain negative fail-closed tests around every new admission.
13. Distinguish:
    - directly device-observed
    - deterministic from already observed registers/instruction semantics
    - external-source corroboration
    - unverified hypothesis/prediction.
14. Do not turn the recovered 16-record physical table into broad permissive bus mappings.
15. Do not pre-map eventual EKA1 virtual layout ranges 0x41000000 / 0x42000000 / 0x50000000 / 0x60000000.
16. Keep report/share UI below Run probe.
17. User wants Vietnamese explanations and direct build/artifact links.
18. User has already granted permission to edit/push/build autonomously.

## 11. Next exact action

The next chat should begin from **AQ**, not AP.

1. If the user has `RH29_MACHINE1_AQ.txt`, read it first.
2. Verify:
   - AQ exact local-word write count is 1;
   - initialized count/readback behavior;
   - exact next unresolved transaction.
3. Do not implement F34 in advance.
4. If device execution reaches F30:
   - verify physical read `0x8C`
   - expected `0x50000000` because correct-era EKA1 `TRomHeader::iRomBase` is at +0x8C.
5. If the next blocker is indeed F34 at `0x09FFF3A0`, only then create the next exact milestone, likely AR, with exact PC/LR/value.
6. If it reaches `0x2AF8`, dump/trace enough of that function to identify its behavior before naming it or adding page-table/MMU mappings.

## 12. Research sources already inspected

Do not repeat this search unless new evidence requires it.

### Correct-era SDK

Repo: `ngagesdk/sdk`

Relevant:

- `6.1/Shared/EPOC32/Include/e32rom.h`
- `6.1/Shared/EPOC32/Tools/hpsym.pl`.

### Historical N-Gage emulator/research

Repo: `mrRosset/Engemu`

Relevant:

- `Engemu/Memory/GageMemory.h`
  - preserves THC N-Gage virtual map
- `Engemu/Memory/BootMemory.h`
- `Engemu/TRomImage.h`
- `Engemu/Loader/TRomImageLoader.cpp`.

### Upstream EKA2L1

Repo: `EKA2L1/EKA2L1`

Relevant:

- EKA1 constants in `src/emu/mem/include/mem/page.h`
- commit `17c005e982921010f40cdb15b51f433d22cb65ee`
  - EKA1 page table injection support.

### General EPOC preservation cross-check

Repo: `joehaines/psionEmulators`

Useful corroboration:

- EPOC `TRomHeader::iRomBase` at +0x8C
- `iRomSize` at +0x90
- logical ROM base 0x50000000 across EPOC5-era systems.

This cross-check is architectural, not RH-29-specific hardware evidence.

## 13. Separate workstreams — do not mix

This handoff is only for N-Gage QD MACHINE1.

Do not accidentally merge it with:

- Nokia 5800 CompatBoot/DirectHome;
- Nokia 5800 Menu3→Home;
- upstream iOS localization/VPL work except as shared build plumbing;
- WP7 XAP runner.

## 14. Exact command for a new chat

Use:

`Tiếp tục MACHINE1-AQ từ docs/handoff/NEWCHAT-NGAGE-QD-MACHINE1-AQ-EKA1-RESEARCH-2026-10-03.md trong repo phai-nguyen/-EKA2L1-iOS-fixed, nhánh ngage-machine1. AP device report đã PASS exact relocated stack và dừng tại data_write 0x09FFF3AC, PC=0xF24/LR=0xF18, value 0. AQ commit f9eee740f26e38291c588b8d73bb74d5496df910 đã build #132 PASS; chờ/đọc RH29_MACHINE1_AQ.txt. Nghiên cứu mới đã xác nhận correct-era EKA1 TRomHeader: +0x8C=iRomBase, +0x90=iRomSize; device read 0x90=0x01170000 là ROM size và dự kiến F30 đọc 0x8C=0x50000000. Không broad-map RAM/stack/page tables, không suy diễn 0x0C150004 là remap register, không implement F34 trước device evidence.`

End of handoff.
