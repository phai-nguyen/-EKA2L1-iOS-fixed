# EKA2L1 iOS Fixed – iPhone-only cloud build

This tiny repository builds the patched EKA2L1 iOS app on GitHub's macOS runner.
No Mac is required locally.

## Fixes included

- **Device Dump (Recommended)**
  - picker explicitly accepts `.rom`
  - validates that the chosen file is a ROM (`SYM.ROM` is supported)
  - copies the selected file into the app sandbox before native installation
  - then asks for `.rpkg`

- **VPL Firmware**
  - keeps the normal **Choose Firmware Folder** option
  - adds **Select Files in This Folder**
  - allows selecting multiple extracted firmware files at once
  - copies those files into an internal firmware folder and passes that folder to the VPL installer

## Build on iPhone

1. Upload the contents of this package to the root of your GitHub repository.
2. Confirm these paths exist in GitHub:
   - `.github/workflows/build-ios-fixed.yml`
   - `patches/RootViewController.mm`
3. Open **Actions**.
4. Choose **Build EKA2L1 iOS Fixed IPA**.
5. Tap **Run workflow** → **Run workflow**.
6. Wait for a green check mark.
7. Open the completed run and download artifact **EKA2L1-iOS-fixed-IPA**.
8. Unzip the artifact. It contains `EKA2L1-iOS-fixed-unsigned.ipa`.
9. Sign that IPA using your normal iPhone signing/sideloading app.

The GitHub build intentionally produces an unsigned IPA.
