#!/usr/bin/env python3
from pathlib import Path
import json
import sys

if len(sys.argv) != 2:
    raise SystemExit("usage: apply_vpl_firmware.py <eka2l1-source>")

root = Path(sys.argv[1]).resolve()
content_path = root / "src/emu/ios/App/ContentView.swift"
bridge_swift_path = root / "src/emu/ios/App/EKA2L1Bridge.swift"
bridge_h_path = root / "src/emu/ios/Bridge/IosEmulator.h"
bridge_mm_path = root / "src/emu/ios/Bridge/IosEmulator.mm"
strings_path = root / "src/emu/ios/Resources/Localizable.xcstrings"

content = content_path.read_text(encoding="utf-8")
bridge_swift = bridge_swift_path.read_text(encoding="utf-8")
bridge_h = bridge_h_path.read_text(encoding="utf-8")
bridge_mm = bridge_mm_path.read_text(encoding="utf-8")

start = content.find("struct ImportDeviceView: View {")
end = content.find("// App icon shared by the grid and row cells.", start)
if start < 0 or end < 0:
    raise SystemExit("ImportDeviceView anchors not found")

import_view = r'''struct ImportDeviceView: View {
    var onFinish: (Bool) -> Void

    @Environment(\.dismiss) private var dismiss

    private struct PickedFile { let name: String; let url: URL }
    private enum PickTarget { case rom, rpkg, archive, vplFolder, vplFiles }

    private enum SourceKind: Hashable {
        case looseFiles
        case archive
        case vplFirmware
    }

    @State private var source: SourceKind = .looseFiles
    @State private var rom: PickedFile?
    @State private var rpkg: PickedFile?
    @State private var archive: PickedFile?
    @State private var vplFolder: PickedFile?
    @State private var vplFiles: [PickedFile] = []
    @State private var isolateDrives = true
    @State private var pickTarget: PickTarget = .rom
    @State private var showingImporter = false
    @State private var installing = false
    @State private var installProgress: Double = 0
    @State private var cancelFlag: InstallCancelFlag?
    @State private var cancelRequested = false
    @State private var errorMessage: String?

    private var installDisabled: Bool {
        switch source {
        case .looseFiles:
            return rom == nil
        case .archive:
            return archive == nil
        case .vplFirmware:
            return vplFolder == nil && vplFiles.isEmpty
        }
    }

    var body: some View {
        NavigationStack {
            Form {
                Section {
                    Picker("import.source", selection: $source) {
                        Text("import.source.looseFiles").tag(SourceKind.looseFiles)
                        Text("import.source.archive").tag(SourceKind.archive)
                        Text("import.source.vpl").tag(SourceKind.vplFirmware)
                    }
                    .pickerStyle(.menu)
                    .disabled(installing)
                }

                Section {
                    if source == .looseFiles {
                        Button { pickTarget = .rom; showingImporter = true } label: {
                            fileRow(title: String(localized: "import.romFile"), value: rom?.name)
                        }
                        Button { pickTarget = .rpkg; showingImporter = true } label: {
                            fileRow(title: String(localized: "import.rpkgFile"), value: rpkg?.name)
                        }
                    } else if source == .archive {
                        Button { pickTarget = .archive; showingImporter = true } label: {
                            fileRow(title: String(localized: "import.archiveFile"), value: archive?.name)
                        }
                    } else {
                        Button {
                            vplFiles = []
                            pickTarget = .vplFolder
                            showingImporter = true
                        } label: {
                            fileRow(title: String(localized: "import.vpl.chooseFolder"), value: vplFolder?.name)
                        }

                        Button {
                            vplFolder = nil
                            pickTarget = .vplFiles
                            showingImporter = true
                        } label: {
                            fileRow(title: String(localized: "import.vpl.chooseFiles"),
                                    value: vplFiles.isEmpty ? nil : vplFiles.first?.name)
                        }

                        if vplFiles.count > 1 {
                            Text(String(localized: "import.vpl.fileCount \(vplFiles.count)"))
                                .font(.footnote)
                                .foregroundColor(.secondary)
                        }
                    }
                } footer: {
                    if source == .looseFiles {
                        Text("import.recommendedDevices")
                    } else if source == .archive {
                        Text("import.archiveHint")
                    } else {
                        Text("import.vpl.hint")
                    }
                }

                Section {
                    Toggle("import.isolateDrives", isOn: $isolateDrives)
                        .disabled(installing)
                } footer: {
                    Text("import.isolateDrives.footer")
                }

                if let errorMessage {
                    Section { Text(errorMessage).foregroundColor(.red) }
                }

                Section {
                    if installing {
                        VStack(alignment: .leading, spacing: 8) {
                            ProgressView(value: installProgress)
                            Text(cancelRequested
                                 ? String(localized: "import.cancelling")
                                 : String(localized: "import.installing \(Int(installProgress * 100))"))
                                .font(.footnote)
                                .foregroundColor(.secondary)
                        }
                        Button(role: .destructive) {
                            cancelRequested = true
                            cancelFlag?.request()
                        } label: {
                            Text("import.stopInstall")
                        }
                        .disabled(cancelRequested)
                    } else {
                        Button(action: install) {
                            Text("common.install")
                        }
                        .disabled(installDisabled)
                    }
                }
            }
            .navigationTitle("import.title")
            .navigationBarTitleDisplayMode(.inline)
            .toolbar {
                ToolbarItem(placement: .cancellationAction) {
                    Button("common.cancel") { dismiss() }
                        .disabled(installing)
                }
            }
            .fileImporter(isPresented: $showingImporter,
                          allowedContentTypes: contentTypes(for: pickTarget),
                          allowsMultipleSelection: pickTarget == .vplFiles) { result in
                pick(result, target: pickTarget)
            }
            .interactiveDismissDisabled(installing)
        }
    }

    private func fileRow(title: String, value: String?) -> some View {
        HStack {
            Text(title).foregroundColor(.primary)
            Spacer()
            Text(value ?? String(localized: "import.noFileSelected"))
                .foregroundColor(.secondary)
                .lineLimit(1)
                .truncationMode(.middle)
        }
    }

    private func contentTypes(for target: PickTarget) -> [UTType] {
        switch target {
        case .rom: return romTypes
        case .rpkg: return rpkgTypes
        case .archive: return archiveTypes
        case .vplFolder: return [.folder]
        case .vplFiles: return [.data]
        }
    }

    private func expectedExtension(for target: PickTarget) -> String {
        switch target {
        case .rom: return "rom"
        case .rpkg: return "rpkg"
        case .archive: return "7z"
        case .vplFolder, .vplFiles: return "vpl"
        }
    }

    private func pick(_ result: Result<[URL], Error>, target: PickTarget) {
        guard case .success(let urls) = result, !urls.isEmpty else { return }

        if target == .vplFolder {
            let url = urls[0]
            errorMessage = nil
            vplFolder = PickedFile(name: url.lastPathComponent, url: url)
            vplFiles = []
            return
        }

        if target == .vplFiles {
            guard urls.contains(where: { $0.pathExtension.caseInsensitiveCompare("vpl") == .orderedSame }) else {
                errorMessage = String(localized: "import.vpl.noManifest")
                return
            }
            errorMessage = nil
            vplFolder = nil
            vplFiles = urls.map { PickedFile(name: $0.lastPathComponent, url: $0) }
            return
        }

        guard let url = urls.first else { return }
        let kind = expectedExtension(for: target)
        guard url.pathExtension.caseInsensitiveCompare(kind) == .orderedSame else {
            errorMessage = String(localized: "import.error.wrongExtension \(kind.uppercased())")
            return
        }

        errorMessage = nil
        let picked = PickedFile(name: url.lastPathComponent, url: url)
        switch target {
        case .rom: rom = picked
        case .rpkg: rpkg = picked
        case .archive: archive = picked
        case .vplFolder, .vplFiles: break
        }
    }

    private func install() {
        let urls: [URL]
        let isolate = isolateDrives
        let run: @Sendable (@escaping @Sendable (Double) -> Void,
                            @escaping @Sendable () -> Bool) -> EKA2L1InstallResult

        switch source {
        case .looseFiles:
            guard let rom else { return }
            let romPath = rom.url.path
            let rpkgPath = rpkg?.url.path
            urls = [rom.url] + (rpkg.map { [$0.url] } ?? [])
            run = { progress, cancel in
                EKA2L1Bridge.installDevice(romPath: romPath, rpkgPath: rpkgPath, isolateDrives: isolate,
                                           progress: progress, cancelCheck: cancel)
            }

        case .archive:
            guard let archive else { return }
            let archivePath = archive.url.path
            urls = [archive.url]
            run = { progress, cancel in
                EKA2L1Bridge.installDevice(archivePath: archivePath, isolateDrives: isolate,
                                           progress: progress, cancelCheck: cancel)
            }

        case .vplFirmware:
            if let vplFolder {
                let folderURL = vplFolder.url
                urls = [folderURL]
                run = { progress, cancel in
                    guard let vplPath = Self.findVPL(in: folderURL) else {
                        return .vplInvalid
                    }
                    NSLog("[VPL-FIRMWARE][FOLDER] %@", vplPath)
                    return EKA2L1Bridge.installDevice(vplPath: vplPath, isolateDrives: isolate,
                                                      progress: progress, cancelCheck: cancel)
                }
            } else {
                let selected = vplFiles.map(\.url)
                guard !selected.isEmpty else { return }
                urls = selected
                run = { progress, cancel in
                    guard let staged = Self.stageFirmwareFiles(selected) else {
                        return .generalFailure
                    }
                    defer { try? FileManager.default.removeItem(at: staged) }
                    guard let vplPath = Self.findVPL(in: staged) else {
                        return .vplInvalid
                    }
                    NSLog("[VPL-FIRMWARE][FILES] count=%ld vpl=%@", selected.count, vplPath)
                    return EKA2L1Bridge.installDevice(vplPath: vplPath, isolateDrives: isolate,
                                                      progress: progress, cancelCheck: cancel)
                }
            }
        }

        installing = true
        installProgress = 0
        errorMessage = nil
        cancelRequested = false
        let flag = InstallCancelFlag()
        cancelFlag = flag

        DispatchQueue.global(qos: .userInitiated).async {
            let scoped = urls.filter { $0.startAccessingSecurityScopedResource() }
            defer { scoped.forEach { $0.stopAccessingSecurityScopedResource() } }

            let result = run({ fraction in
                DispatchQueue.main.async { installProgress = fraction }
            }, { flag.isRequested })

            DispatchQueue.main.async {
                installing = false
                cancelFlag = nil
                cancelRequested = false
                if result == .success {
                    onFinish(true)
                    dismiss()
                } else if result == .cancelled {
                    dismiss()
                } else {
                    errorMessage = installMessage(for: result)
                }
            }
        }
    }

    private static func findVPL(in folder: URL) -> String? {
        let fm = FileManager.default
        guard let items = try? fm.contentsOfDirectory(at: folder,
                                                      includingPropertiesForKeys: nil,
                                                      options: [.skipsHiddenFiles]) else {
            return nil
        }
        return items.first(where: {
            $0.pathExtension.caseInsensitiveCompare("vpl") == .orderedSame
        })?.path
    }

    private static func stageFirmwareFiles(_ files: [URL]) -> URL? {
        let fm = FileManager.default
        let folder = fm.temporaryDirectory
            .appendingPathComponent("eka2l1-vpl-\(UUID().uuidString)", isDirectory: true)
        do {
            try fm.createDirectory(at: folder, withIntermediateDirectories: true)
            for source in files {
                let destination = folder.appendingPathComponent(source.lastPathComponent)
                if fm.fileExists(atPath: destination.path) {
                    try fm.removeItem(at: destination)
                }
                try fm.copyItem(at: source, to: destination)
            }
            return folder
        } catch {
            try? fm.removeItem(at: folder)
            NSLog("[VPL-FIRMWARE][STAGE-FAIL] %@", String(describing: error))
            return nil
        }
    }

    private func installMessage(for result: EKA2L1InstallResult) -> String {
        switch result {
        case .success:
            return String(localized: "common.completed")
        case .alreadyExist:
            return String(localized: "import.error.alreadyExists")
        case .determineProductFailure:
            return String(localized: "import.error.determineProduct")
        case .insufficient:
            return String(localized: "import.error.insufficient")
        case .notExist:
            return String(localized: "import.error.notExist")
        case .rpkgCorrupt:
            return String(localized: "import.error.rpkgCorrupt")
        case .vplInvalid:
            return String(localized: "import.error.vplInvalid")
        case .romCorrupt:
            return String(localized: "import.error.romCorrupt")
        case .rofsCorrupt:
            return String(localized: "import.error.rofsCorrupt")
        case .fpsxCorrupt:
            return String(localized: "import.error.fpsxCorrupt")
        case .romFailToCopy:
            return String(localized: "import.error.romCopy")
        case .needRpkg:
            return String(localized: "import.error.needRpkg")
        case .cancelled:
            return String(localized: "import.cancelled")
        case .archiveCorrupt:
            return String(localized: "import.error.archiveCorrupt")
        case .archiveNoDevice:
            return String(localized: "import.error.archiveNoDevice")
        case .generalFailure:
            return String(localized: "common.error")
        @unknown default:
            return String(localized: "common.error")
        }
    }
}

'''

content = content[:start] + import_view + content[end:]
content_path.write_text(content, encoding="utf-8")

swift_anchor = '''    nonisolated static func installDevice(archivePath: String, isolateDrives: Bool,
                                          progress: (@Sendable (Double) -> Void)? = nil,
                                          cancelCheck: (@Sendable () -> Bool)? = nil) -> EKA2L1InstallResult {
        EKA2L1Emulator.shared().installDevice(archivePath: archivePath, isolateDrives: isolateDrives,
                                              progress: progress, cancelCheck: cancelCheck)
    }

'''
swift_insert = swift_anchor + '''    // Nokia firmware set: .vpl manifest plus the matching .fpsx/.rofs files
    // stored beside it. The core install_firmware parser remains authoritative.
    nonisolated static func installDevice(vplPath: String, isolateDrives: Bool,
                                          progress: (@Sendable (Double) -> Void)? = nil,
                                          cancelCheck: (@Sendable () -> Bool)? = nil) -> EKA2L1InstallResult {
        EKA2L1Emulator.shared().installDevice(vplPath: vplPath, isolateDrives: isolateDrives,
                                              progress: progress, cancelCheck: cancelCheck)
    }

'''
if swift_anchor not in bridge_swift:
    raise SystemExit("EKA2L1Bridge archive anchor not found")
bridge_swift = bridge_swift.replace(swift_anchor, swift_insert, 1)
bridge_swift_path.write_text(bridge_swift, encoding="utf-8")

header_anchor = '''- (EKA2L1InstallResult)installDeviceWithArchivePath:(NSString *)archivePath
                                      isolateDrives:(BOOL)isolateDrives
                                           progress:(nullable void (^)(double fraction))progress
                                        cancelCheck:(nullable BOOL (^)(void))cancelCheck
    NS_SWIFT_NAME(installDevice(archivePath:isolateDrives:progress:cancelCheck:));

'''
header_insert = header_anchor + '''// Install a Nokia firmware set described by a .vpl manifest. Matching .fpsx,
// ROFS and related firmware files must be present in the same folder.
// Uses the existing eka2l1::install_firmware parser; no firmware logic is
// reimplemented in the iOS frontend.
- (EKA2L1InstallResult)installDeviceWithVplPath:(NSString *)vplPath
                                  isolateDrives:(BOOL)isolateDrives
                                       progress:(nullable void (^)(double fraction))progress
                                    cancelCheck:(nullable BOOL (^)(void))cancelCheck
    NS_SWIFT_NAME(installDevice(vplPath:isolateDrives:progress:cancelCheck:));

'''
if header_anchor not in bridge_h:
    raise SystemExit("IosEmulator.h archive anchor not found")
bridge_h = bridge_h.replace(header_anchor, header_insert, 1)
bridge_h_path.write_text(bridge_h, encoding="utf-8")

mm_anchor = '''- (EKA2L1InstallResult)installDeviceWithArchivePath:(NSString *)archivePath
                                      isolateDrives:(BOOL)isolateDrives
                                           progress:(void (^)(double))progress
                                        cancelCheck:(BOOL (^)(void))cancelCheck {
    if (![NSFileManager.defaultManager fileExistsAtPath:archivePath]) {
        return EKA2L1InstallResultNotExist;
    }

    const std::string archive_std = archivePath.UTF8String;

    return [self runDeviceInstall:^(eka2l1::device_manager *dvc, const std::string &rom_resident_path,
                                     const std::string &root_z_path, progress_changed_callback progress_cb,
                                     cancel_requested_callback cancel_cb) {
        return eka2l1::loader::install_archive(dvc, archive_std, rom_resident_path, root_z_path,
            isolateDrives == YES, progress_cb, cancel_cb);
    } progress:progress cancelCheck:cancelCheck];
}

'''
mm_insert = mm_anchor + '''- (EKA2L1InstallResult)installDeviceWithVplPath:(NSString *)vplPath
                                  isolateDrives:(BOOL)isolateDrives
                                       progress:(void (^)(double))progress
                                    cancelCheck:(BOOL (^)(void))cancelCheck {
    if (!_state || ![NSFileManager.defaultManager fileExistsAtPath:vplPath]) {
        return EKA2L1InstallResultNotExist;
    }

    const std::string vpl_std = vplPath.UTF8String;
    const std::string storage = _state->conf.storage;

    return [self runDeviceInstall:^(eka2l1::device_manager *dvc, const std::string &rom_resident_path,
                                     const std::string &root_z_path, progress_changed_callback progress_cb,
                                     cancel_requested_callback cancel_cb) {
        return eka2l1::install_firmware(
            dvc, vpl_std, storage, rom_resident_path, isolateDrives == YES,
            [](const std::vector<std::string> &variants) -> int {
                return variants.empty() ? -1 : 0;
            },
            progress_cb, cancel_cb);
    } progress:progress cancelCheck:cancelCheck];
}

'''
if mm_anchor not in bridge_mm:
    raise SystemExit("IosEmulator.mm archive anchor not found")
bridge_mm = bridge_mm.replace(mm_anchor, mm_insert, 1)
bridge_mm_path.write_text(bridge_mm, encoding="utf-8")

catalog = json.loads(strings_path.read_text(encoding="utf-8"))
strings = catalog.setdefault("strings", {})
new_strings = {
    "import.source.vpl": "Firmware VPL",
    "import.vpl.chooseFolder": "Choose Firmware Folder",
    "import.vpl.chooseFiles": "Select Firmware Files",
    "import.vpl.hint": "Choose the folder that contains the .vpl manifest and all matching firmware files, or select those files together.",
    "import.vpl.noManifest": "No .vpl manifest was selected.",
    "import.vpl.fileCount %lld": "Selected %lld firmware files."
}
for key, value in new_strings.items():
    strings.setdefault(key, {
        "localizations": {
            "en": {
                "stringUnit": {
                    "state": "translated",
                    "value": value
                }
            }
        }
    })
strings_path.write_text(json.dumps(catalog, ensure_ascii=False, indent=2) + "\n", encoding="utf-8")

print("[VPL-FIRMWARE] patch applied")
print("  SwiftUI: folder + multi-file VPL import")
print("  Bridge:  EKA2L1Bridge.installDevice(vplPath:...)")
print("  Core:    eka2l1::install_firmware")
