#!/usr/bin/env python3
from pathlib import Path
import sys

root = Path(sys.argv[1] if len(sys.argv) > 1 else ".")
path = root / "src/emu/ios/App/ContentView.swift"
text = path.read_text()

old = '''    private func contentTypes(for target: PickTarget) -> [UTType] {
        switch target {
        case .rom: return romTypes
        case .rpkg: return rpkgTypes
        case .archive: return archiveTypes
        }
    }
'''
new = '''    private func contentTypes(for target: PickTarget) -> [UTType] {
        // Keep the device installer picker deliberately broad. Re-signed /
        // sideloaded builds can lose reliable registration of our custom
        // .rom/.rpkg/.7z UTTypes when their bundle identifier is rewritten.
        // The selection handler already validates the filename extension, so
        // allowing public.data here is safe and makes Files providers return
        // the URL consistently.
        return [.data]
    }
'''
if old not in text:
    raise SystemExit("contentTypes block not found")
text = text.replace(old, new)

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
            errorMessage = "No file was returned by the document picker."
            return
        }
        let kind = expectedExtension(for: target)
'''
if old not in text:
    raise SystemExit("pick block not found")
text = text.replace(old, new)

path.write_text(text)
print("Applied resilient iOS device file-picker fix")
