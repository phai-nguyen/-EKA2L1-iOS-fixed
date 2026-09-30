# RH-29 MACHINE1 Design

**Date:** 2026-09-30  
**Branch:** `ngage-machine1`  
**Base:** `hybridhome-upstream1@acb2775a652f44104c841a6ed08557801a3aadbe`  
**Target:** Nokia N-Gage QD RH-29, Symbian OS 6.1 / EKA1  
**ROM baseline:** RH-29 V04.10 (09-09-2004) `SYM.ROM`

## 1. Status gate

The RH-29 V04.10 `SYM.ROM` is now **DEVICE PASS** in the current EKA2L1 iOS product path.

Observed device evidence:
- ROM imports successfully.
- RH-29 device boots in the existing EKA2L1 HLE path.
- ROM applications are usable.
- FIFA 05 installs/runs and is playable.

This proves the ROM is a valid baseline for research. It does **not** prove full-machine boot; MACHINE1 is a separate research path.

## 2. Goal

MACHINE1 is the smallest full-machine-oriented probe that can answer:

> Starting from the RH-29 ROM's real restart vector, what addresses does the ARM guest execute, read, and write before the first unsupported hardware dependency stops progress?

The first milestone is evidence collection, not Home/Menu/LCD output.

## 3. Non-goals

MACHINE1 does not implement:
- LCD/framebuffer;
- keypad;
- audio;
- GSM/Bluetooth;
- MMC;
- timer/IRQ/FIQ;
- complete ARM920T MMU/CP15 device behavior;
- RM-356 full hardware;
- replacement of the existing EKA2L1 HLE path.

No guessed RH-29 MMIO addresses or guessed physical RAM windows are allowed.

## 4. Approaches considered

### A. Modify EKA2L1 `memory_system` globally

Rejected for MACHINE1.

The current `memory_system` is designed around EKA2L1's HLE virtual-memory model. Global MMIO interception here risks changing every existing EKA1/EKA2 device and could regress the product branch.

### B. Dedicated RH-29 machine bus using the existing ARM core callbacks

**Selected.**

EKA2L1's `arm::core` already exposes:
- `read_8bit/read_16bit/read_32bit/read_64bit`;
- `write_*`;
- `read_code`;
- `system_call_handler`;
- `exception_handler`;
- PC/LR/SP/register/CPSR access;
- bounded `run(instruction_count)` and `step()`.

MACHINE1 therefore creates a separate RH-29 bus and wires it directly to a CPU core. The existing HLE `memory_system` remains untouched.

On iOS, the backend remains Dyncom because the product baseline has JIT/Dynarmic disabled.

### C. Desktop-only external CPU emulator / Unicorn-style probe

Rejected as the primary path.

It could be useful for offline research, but the device-test environment is iPhone-first and the result would not directly become the iOS machine implementation.

## 5. Architecture

```
Installed RH-29 SYM.ROM
        |
        v
rh29_machine_probe
        |
        +--> parse EKA1 rom_header
        |      - rom_base
        |      - rom_size
        |      - restart_vector
        |      - kern_data_address
        |      - kern_limit
        |      - hardware
        |
        +--> rh29_bus
        |      - ROM region (read/execute)
        |      - verified RAM regions only
        |      - unknown-access tracer
        |
        +--> existing arm::core (Dyncom on iOS)
               - read/write callbacks -> rh29_bus
               - read_code -> rh29_bus
               - exception callback -> probe report
               - bounded execution
```

The machine probe is separate from the normal EKA2L1 kernel/services/HLE boot.

## 6. ROM mapping

The ROM header is parsed at runtime from the installed RH-29 `SYM.ROM`.

Required validation before execution:
- `rom_base == 0x50000000`;
- `rom_size` is non-zero and fits the file;
- `restart_vector` lies inside a mapped executable region or otherwise has an explicitly understood reset mapping;
- header fields cannot overflow the ROM file.

The known DEVICE-PASS image is expected to report a ROM size of `0x01170000` (18,284,544 bytes), but the probe must validate the file rather than trust this constant.

ROM writes are never silently accepted.

## 7. Reset state

MACHINE1 starts from the ROM header's `restart_vector`, not from an HLE process entry point.

The initial CPU mode/CPSR and any reset aliasing must follow documented ARM9/ARM920T reset semantics. They must be verified before being hard-coded.

The initial report records:
- PC;
- LR;
- SP;
- CPSR;
- R0-R12;
- ROM header restart vector;
- instruction budget.

## 8. RAM policy

Physical RH-29 RAM placement is currently unverified.

Therefore MACHINE1 must **not** allocate an arbitrary RAM window just to make execution continue.

The bus supports RAM regions, but a region is enabled only when its base/size has evidence from one of:
1. RH-29 firmware/binary behavior;
2. Nokia service/schematic documentation;
3. credible DCT4/WD2 hardware research;
4. a confirmed upstream/emulator implementation.

Until then, an access outside ROM is classified and logged as unresolved instead of being silently converted into RAM.

This makes MACHINE1-A useful even before RAM is resolved: the first accesses provide concrete addresses for the next research step.

## 9. Unknown access / MMIO tracer

For every access outside mapped ROM/RAM, record:
- read/write;
- width: 8/16/32/64;
- address;
- PC;
- LR;
- write value when applicable;
- occurrence count.

Two policies are allowed:

### Strict policy — authoritative
Stop on the first unresolved access after logging it.

This is the default for evidence used to identify the real hardware map.

### Exploratory zero-stub policy — non-authoritative
Unknown reads may return zero and unknown writes may be discarded so execution can reveal later dependencies.

Every such event must be marked `SYNTHETIC_ZERO` / `SINK_WRITE`. Results from this mode are clues only, never proof of hardware behavior.

## 10. Bounded execution

No infinite run.

Initial selectable budgets:
- 1,000 instructions;
- 10,000 instructions;
- 100,000 instructions;
- 1,000,000 instructions.

The probe stops on:
- budget exhaustion;
- CPU exception;
- unresolved access in strict mode;
- explicit user cancellation;
- invalid PC/code fetch.

The final report contains executed instruction count and complete register state.

## 11. iOS integration

The probe must run on the user's iPhone.

UI exposure is gated to current firmware code `RH-29` only.

Proposed research entry:
- a small `RH-29 Machine Probe` action in a research/debug section;
- not shown for RM-356 or other devices;
- normal app launching remains the default behavior.

The bridge obtains the already-installed ROM from the same storage convention used by EKA2L1:
`data/roms/<lowercase firmware code>/SYM.ROM`.

The probe runs off the main queue and writes a text report into the app Documents/log area so it can be exported with the existing test workflow.

## 12. Isolation guarantees

- `hybridhome-upstream1` is not modified.
- Existing HLE device boot remains the default.
- MACHINE1 is opt-in and RH-29-only.
- No product path is redirected to MACHINE1.
- No guessed device register implementation is added.
- Failure in the probe must return to the normal iOS UI instead of corrupting device state.

## 13. First implementation milestone: MACHINE1-A

MACHINE1-A implements only:

1. locate installed RH-29 `SYM.ROM`;
2. parse and validate EKA1 ROM header;
3. construct a dedicated RH-29 bus;
4. map ROM at `0x50000000`;
5. instantiate the existing ARM core using the non-JIT iOS backend;
6. wire memory/code/exception callbacks;
7. initialize reset PC from `restart_vector`;
8. run a bounded instruction count;
9. stop at first unresolved bus access in strict mode;
10. dump PC/LR/SP/CPSR/R0-R12 and the unknown access.

**Success criterion for MACHINE1-A is not a Nokia logo.**

Success is a deterministic report showing the first real boot-path execution and the first address that requires RAM/MMIO/other hardware modeling.

## 14. Follow-on order

After MACHINE1-A device evidence:

1. classify the first unresolved addresses;
2. establish an evidence-backed physical RAM map;
3. add RAM and rerun;
4. identify first MMIO block(s);
5. only then model timer/interrupt hardware;
6. LCD comes after CPU/timer/IRQ progress is established.

This preserves the Cloudpilot principle: implement only the hardware that the real ROM proves it needs.
