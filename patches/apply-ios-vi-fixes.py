#!/usr/bin/env python3
from pathlib import Path
import sys

root = Path(sys.argv[1] if len(sys.argv) > 1 else ".")
path = root / "src/emu/ios/App/ContentView.swift"
text = path.read_text()

# UIDocumentPickerViewController is used for device ROM/RPKG/7z selection.
if "import UIKit\n" not in text:
    text = text.replace("import SwiftUI\n", "import SwiftUI\nimport UIKit\n", 1)

old = '''    private func contentTypes(for target: PickTarget) -> [UTType] {
        switch target {
        case .rom: return romTypes
        case .rpkg: return rpkgTypes
        case .archive: return archiveTypes
        }
    }
'''
new = '''    private func contentTypes(for target: PickTarget) -> [UTType] {
        // Device dumps are validated by filename extension after selection.
        // Keep the system picker broad so re-signing and Files providers do not
        // have to agree on EKA2L1's custom imported UTI registration.
        return [.data]
    }
'''
if old not in text:
    raise SystemExit("contentTypes block not found")
text = text.replace(old, new)

old = '''            .fileImporter(isPresented: $showingImporter,
                          allowedContentTypes: contentTypes(for: pickTarget),
                          allowsMultipleSelection: false) { result in
                pick(result, target: pickTarget)
            }
            .interactiveDismissDisabled(installing)
'''
new = '''            .sheet(isPresented: $showingImporter) {
                DeviceDocumentPicker(contentTypes: contentTypes(for: pickTarget)) { result in
                    pick(result, target: pickTarget)
                    showingImporter = false
                } onCancel: {
                    showingImporter = false
                }
            }
            .interactiveDismissDisabled(installing)
'''
if old not in text:
    raise SystemExit("ImportDeviceView fileImporter block not found")
text = text.replace(old, new)

marker = '''// Everything is read in place at install time. The installer copies the ROM onto
'''
helper = r'''// UIKit document picker used only by the device installer.
//
// SwiftUI's fileImporter can present the Files UI successfully but fail to
// deliver the selected security-scoped URL on some re-signed iOS builds. The
// UIKit delegate callback is explicit and has proved more reliable across iOS
// versions and signing tools.
private struct DeviceDocumentPicker: UIViewControllerRepresentable {
    let contentTypes: [UTType]
    let onResult: (Result<[URL], Error>) -> Void
    let onCancel: () -> Void

    func makeCoordinator() -> Coordinator {
        Coordinator(onResult: onResult, onCancel: onCancel)
    }

    func makeUIViewController(context: Context) -> UIDocumentPickerViewController {
        let picker = UIDocumentPickerViewController(
            forOpeningContentTypes: contentTypes,
            asCopy: false
        )
        picker.delegate = context.coordinator
        picker.allowsMultipleSelection = false
        return picker
    }

    func updateUIViewController(_ uiViewController: UIDocumentPickerViewController,
                                context: Context) {}

    final class Coordinator: NSObject, UIDocumentPickerDelegate {
        let onResult: (Result<[URL], Error>) -> Void
        let onCancel: () -> Void

        init(onResult: @escaping (Result<[URL], Error>) -> Void,
             onCancel: @escaping () -> Void) {
            self.onResult = onResult
            self.onCancel = onCancel
        }

        func documentPicker(_ controller: UIDocumentPickerViewController,
                            didPickDocumentsAt urls: [URL]) {
            onResult(.success(urls))
        }

        func documentPickerWasCancelled(_ controller: UIDocumentPickerViewController) {
            onCancel()
        }
    }
}

'''
if "private struct DeviceDocumentPicker: UIViewControllerRepresentable" not in text:
    if marker not in text:
        raise SystemExit("ImportDeviceView insertion marker not found")
    text = text.replace(marker, helper + marker, 1)

old = '''    private func pick(_ result: Result<[URL], Error>, target: PickTarget) {
        guard case .success(let urls) = result, let url = urls.first else { return }
        let kind = expectedExtension(for: target)
'''
new = '''    private func pick(_ result: Result<[URL], Error>, target: PickTarget) {
        let urls: [URL]
        switch result {
        case .failure(let error):
            errorMessage = error.localizedDescription
            return
        case .success(let pickedURLs):
            urls = pickedURLs
        }
        guard let url = urls.first else {
            errorMessage = "Không nhận được tệp từ trình chọn tệp."
            return
        }
        let kind = expectedExtension(for: target)
'''
if old not in text:
    raise SystemExit("pick block not found")
text = text.replace(old, new)

path.write_text(text)
print("Applied UIKit iOS device file-picker fix")
