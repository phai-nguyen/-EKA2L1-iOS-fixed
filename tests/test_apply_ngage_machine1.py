import subprocess
import sys
import tempfile
import unittest
from pathlib import Path

ROOT = Path(__file__).resolve().parents[1]
SCRIPT = ROOT / "scripts" / "apply_ngage_machine1.py"


class ApplyNGageMachine1Tests(unittest.TestCase):
    def make_upstream(self, *, good_cmake=True):
        td = tempfile.TemporaryDirectory()
        root = Path(td.name)
        app = root / "src/emu/ios/App"
        bridge = root / "src/emu/ios/Bridge"
        app.mkdir(parents=True)
        bridge.mkdir(parents=True)

        (app / "ContentView.swift").write_text("import SwiftUI\nstruct ContentView {}\n", encoding="utf-8")
        (bridge / "IosEmulator.h").write_text("@interface EKA2L1Emulator : NSObject\n@end\n", encoding="utf-8")
        (bridge / "IosEmulator.mm").write_text('#import "IosEmulator.h"\n', encoding="utf-8")

        if good_cmake:
            cmake = '''set(EKA2L1_IOS_SOURCES
    "${EKA2L1_IOS_APP_DIR}/ContentView.swift"
    "${EKA2L1_IOS_BRIDGE_DIR}/IosEmulator.h"
    "${EKA2L1_IOS_BRIDGE_DIR}/IosEmulator.mm"
    "${EKA2L1_IOS_BRIDGE_DIR}/EKA2L1-Bridging-Header.h"
)
'''
        else:
            cmake = "set(EKA2L1_IOS_SOURCES)\n"
        (root / "src/emu/ios/CMakeLists.txt").write_text(cmake, encoding="utf-8")
        return td, root

    def run_script(self, root):
        return subprocess.run(
            [sys.executable, str(SCRIPT), str(root)],
            cwd=ROOT,
            text=True,
            stdout=subprocess.PIPE,
            stderr=subprocess.STDOUT,
        )

    def test_refuses_missing_cmake_anchor(self):
        td, root = self.make_upstream(good_cmake=False)
        self.addCleanup(td.cleanup)
        result = self.run_script(root)
        self.assertNotEqual(result.returncode, 0)
        self.assertIn("anchor", result.stdout.lower())

    def test_copies_machine_sources_and_swift_view(self):
        td, root = self.make_upstream()
        self.addCleanup(td.cleanup)
        result = self.run_script(root)
        self.assertEqual(result.returncode, 0, result.stdout)

        bridge = root / "src/emu/ios/Bridge"
        app = root / "src/emu/ios/App"
        for name in [
            "RH29MachineModel.h",
            "RH29MachineModel.cpp",
            "RH29MachineRunner.h",
            "RH29MachineRunner.cpp",
        ]:
            self.assertTrue((bridge / name).is_file(), name)
        self.assertTrue((app / "RH29MachineProbeView.swift").is_file())

        self.assertIn('#include "RH29MachineModel.h"', (bridge / "RH29MachineModel.cpp").read_text())
        self.assertIn('#include "RH29MachineModel.h"', (bridge / "RH29MachineRunner.h").read_text())
        self.assertIn('#include "RH29MachineRunner.h"', (bridge / "RH29MachineRunner.cpp").read_text())

    def test_adds_cmake_sources_exactly_once_and_is_idempotent(self):
        td, root = self.make_upstream()
        self.addCleanup(td.cleanup)
        first = self.run_script(root)
        self.assertEqual(first.returncode, 0, first.stdout)
        second = self.run_script(root)
        self.assertEqual(second.returncode, 0, second.stdout)

        cmake = (root / "src/emu/ios/CMakeLists.txt").read_text()
        for source in [
            "RH29MachineProbeView.swift",
            "RH29MachineModel.h",
            "RH29MachineModel.cpp",
            "RH29MachineRunner.h",
            "RH29MachineRunner.cpp",
        ]:
            self.assertEqual(cmake.count(source), 1, source)

    def test_preserves_existing_ios_sources(self):
        td, root = self.make_upstream()
        self.addCleanup(td.cleanup)
        before_content = (root / "src/emu/ios/App/ContentView.swift").read_text()
        before_mm = (root / "src/emu/ios/Bridge/IosEmulator.mm").read_text()
        result = self.run_script(root)
        self.assertEqual(result.returncode, 0, result.stdout)
        self.assertEqual((root / "src/emu/ios/App/ContentView.swift").read_text(), before_content)
        self.assertEqual((root / "src/emu/ios/Bridge/IosEmulator.mm").read_text(), before_mm)


if __name__ == "__main__":
    unittest.main()
