# EKA2L1 iOS Vietnamese localization

This branch is intentionally isolated from the NATIVEBOOT2 project.

- Repository: `phai-nguyen/-EKA2L1-iOS-fixed`
- Branch: `vi-localization-official`
- Upstream source: `EKA2L1/EKA2L1`
- Upstream commit: `3afd85d249f4eef16071f6fdbbbf44e49e4072f7`
- iOS bundle: `com.eka2l1.emulator`
- Localization: Vietnamese (`vi`)
- UI catalog: 241 entries total (235 regular strings + 6 plural entries)
- Info.plist permission descriptions: 4 entries

## Files

- `patches/Localizable.xcstrings`
- `patches/InfoPlist.xcstrings`
- `.github/workflows/build-ios-vietnamese.yml`

The workflow clones the official EKA2L1 source, applies only the Vietnamese string catalogs, builds the unsigned iPhone app, verifies `vi.lproj`, and packages an unsigned IPA.

No NATIVEBOOT2 files, commits, patches, or handoff state are used by this branch.
