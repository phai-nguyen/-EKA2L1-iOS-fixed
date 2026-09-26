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


## Confirmed ESign / Files picker requirement

Device testing on 2026-09-26 confirmed that the iOS Files picker depends on the
re-signed app identity matching the provisioning application identifier.

A/B result on the same iPhone and the same ESign certificate:

- App Store-derived 26.9.2 + Vietnamese, Bundle ID rewritten to
  `app.lavender1865.valley8348`: ROM file selection works.
- The same App Store-derived 26.9.2 + Vietnamese with the original
  `com.eka2l1.emulator` Bundle ID: the Files UI opens, but selecting the ROM
  does not return a usable URL to EKA2L1.

The ESign profile observed in the signed test IPA used
`application-identifier = 3V48279JP4.app.lavender1865.valley8348`.
Therefore the bundle identifier must be `app.lavender1865.valley8348` for
that profile. The Team ID prefix is not part of CFBundleIdentifier.

This is now the preferred sideload packaging for this user's localization
branch. The upstream picker implementation does not need to be replaced when
the provisioning identity is matched correctly.


## Fast build architecture

The Vietnamese branch now has three build modes.

### 1. FAST-REPACK — localization/resources only

Workflow: `.github/workflows/repack-ios-vietnamese.yml`

Triggered when either string catalog changes:

- `patches/Localizable.xcstrings`
- `patches/InfoPlist.xcstrings`

It restores a previously built unsigned `EKA2L1.app`, runs Apple's
`xcstringstool` only for Vietnamese, replaces `vi.lproj`, sets the confirmed
ESign-match bundle identifier, and repackages the IPA. It does not run CMake,
FFmpeg, or compile EKA2L1 C++/Swift code.

### 2. INCREMENTAL-CODE — Swift/C++ changes

Workflow: `.github/workflows/build-ios-vietnamese.yml`

The full-code workflow now restores:

- the previous `build/ios-device` Xcode/CMake build tree;
- ccache compiler objects;
- the separately cached FFmpeg iOS slice.

Xcode therefore recompiles only changed translation units and the targets that
depend on them, then relinks as required.

### 3. FULL-BASE — cold build

A clean build is only required when the upstream revision/toolchain/build
configuration changes, the reusable cache has expired, or a deliberate clean
build is requested.

The base-app cache is keyed by the upstream commit and a code fingerprint, so a
localization-only repack cannot accidentally reuse a binary from a different
code revision.
