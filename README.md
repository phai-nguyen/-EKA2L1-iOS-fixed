# EKA2L1 iOS Phone Mode V2

V2 fixes the first Phone Mode build so the compiled iOS core really receives `get_all_apps()`.

Key fixes:
- restores `RootViewController.mm` to call `bridge::get_all_apps()`;
- copies launcher/bridge files to the real CMake source paths (`include/ios` and `src`);
- verifies those APIs before compiling;
- makes touch/key input non-blocking while the emulator is shutting down/rebooting to avoid the iOS watchdog deadlock;
- re-enables touch after normal game exit;
- keeps ROM/RPKG and VPL picker fixes.

Replace the existing five files in `patches/` and replace `.github/workflows/build-ios-fixed.yml`, then run the workflow.
