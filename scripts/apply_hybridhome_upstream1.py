#!/usr/bin/env python3
from pathlib import Path
import sys

if len(sys.argv) != 2:
    raise SystemExit("usage: apply_hybridhome_upstream1.py <eka2l1-source>")

root = Path(sys.argv[1]).resolve()
content_path = root / "src/emu/ios/App/ContentView.swift"
cmake_path = root / "src/emu/ios/CMakeLists.txt"
home_path = root / "src/emu/ios/App/S60HybridHomeView.swift"

content = content_path.read_text(encoding="utf-8")
cmake = cmake_path.read_text(encoding="utf-8")

# Device-tested ESign identity. The Files document picker returned usable ROM/RPKG
# URLs on this user's iPhone only when CFBundleIdentifier matched the
# provisioning application identifier used by ESign.
bundle_old = 'set(EKA2L1_IOS_BUNDLE_ID "com.eka2l1.emulator" CACHE STRING "iOS bundle identifier")'
bundle_new = 'set(EKA2L1_IOS_BUNDLE_ID "app.lavender1865.valley8348" CACHE STRING "iOS bundle identifier")'
if bundle_old not in cmake:
    raise SystemExit("iOS bundle-id anchor not found")
cmake = cmake.replace(bundle_old, bundle_new, 1)

body_old = '''                } else if store.devices.isEmpty {
                    emptyState
                } else {
                    appList
                }
'''
body_new = '''                } else if store.devices.isEmpty {
                    emptyState
                } else if isRM356 {
                    hybridHome
                } else {
                    appList
                }
'''
if body_old not in content:
    raise SystemExit("ContentView body anchor not found")
content = content.replace(body_old, body_new, 1)

anchor = '''    private var homeImporterTypes: [UTType] {
'''
insert = '''    // HYBRIDHOME-UPSTREAM1 is deliberately scoped to Nokia 5800 RM-356.
    // Every other device keeps the upstream EKA2L1 home surface unchanged.
    private var isRM356: Bool {
        store.currentDevice?.firmwareCode.caseInsensitiveCompare("RM-356") == .orderedSame
    }

    private var hybridHome: some View {
        S60HybridHomeView(
            deviceName: store.currentDevice?.displayName ?? "Nokia 5800",
            apps: store.apps,
            onRefresh: { await store.refreshApps() }
        )
    }

'''
if anchor not in content:
    raise SystemExit("ContentView property anchor not found")
content = content.replace(anchor, insert + anchor, 1)
content_path.write_text(content, encoding="utf-8")

cmake_anchor = '''    "${EKA2L1_IOS_APP_DIR}/ContentView.swift"
'''
cmake_insert = '''    "${EKA2L1_IOS_APP_DIR}/ContentView.swift"
    "${EKA2L1_IOS_APP_DIR}/S60HybridHomeView.swift"
'''
if cmake_anchor not in cmake:
    raise SystemExit("iOS CMake Swift source anchor not found")
cmake = cmake.replace(cmake_anchor, cmake_insert, 1)
cmake_path.write_text(cmake, encoding="utf-8")

home = r'''import SwiftUI

// HYBRIDHOME-UPSTREAM1
//
// Host-rendered S60-style home surface for Nokia 5800 RM-356.
// The emulator backend, application registry, icons and launched processes all
// remain the real upstream EKA2L1 implementations. The original Nokia Home UID
// is excluded from this proof because the point is to avoid depending on its
// startup chain while preserving the rest of the Symbian environment.
struct S60HybridHomeView: View {
    let deviceName: String
    let apps: [EKA2L1AppItem]
    let onRefresh: () async -> Void

    private let realNokiaHomeUID: UInt32 = 0x102750F0

    private var visibleApps: [EKA2L1AppItem] {
        apps
            .filter { $0.uid != realNokiaHomeUID }
            .sorted {
                $0.name.localizedCaseInsensitiveCompare($1.name) == .orderedAscending
            }
    }

    private let columns = Array(
        repeating: GridItem(.flexible(minimum: 68, maximum: 96), spacing: 8),
        count: 4
    )

    var body: some View {
        ZStack {
            LinearGradient(
                colors: [
                    Color(red: 0.08, green: 0.18, blue: 0.27),
                    Color(red: 0.20, green: 0.34, blue: 0.42),
                    Color(red: 0.55, green: 0.64, blue: 0.65)
                ],
                startPoint: .top,
                endPoint: .bottom
            )
            .ignoresSafeArea()

            VStack(spacing: 0) {
                statusBar

                ScrollView {
                    VStack(alignment: .leading, spacing: 14) {
                        header

                        LazyVGrid(columns: columns, spacing: 8) {
                            ForEach(visibleApps) { app in
                                NavigationLink(destination: EmulatorView(uid: app.uid)) {
                                    AppGridCell(uid: app.uid, name: app.name)
                                        .foregroundStyle(.white)
                                }
                                .buttonStyle(.plain)
                                .simultaneousGesture(
                                    TapGesture().onEnded {
                                        NSLog("[HYBRIDHOME-UPSTREAM1][LAUNCH] uid=0x%08X name=%@",
                                              app.uid, app.name)
                                    }
                                )
                            }
                        }
                    }
                    .padding(.horizontal, 10)
                    .padding(.bottom, 20)
                }
                .refreshable {
                    NSLog("[HYBRIDHOME-UPSTREAM1][REFRESH]")
                    await onRefresh()
                }

                softkeyBar
            }
        }
        .onAppear {
            NSLog("[HYBRIDHOME-UPSTREAM1][SHOW] device=%@ apps=%ld",
                  deviceName, visibleApps.count)
        }
    }

    private var statusBar: some View {
        HStack(spacing: 8) {
            Text("NOKIA")
                .font(.caption.bold())

            Spacer()

            TimelineView(.periodic(from: .now, by: 30)) { context in
                Text(context.date.formatted(date: .omitted, time: .shortened))
                    .font(.caption.monospacedDigit())
            }

            Image(systemName: "antenna.radiowaves.left.and.right")
                .font(.caption2)

            Image(systemName: "battery.75percent")
                .font(.caption)
        }
        .foregroundStyle(.white)
        .padding(.horizontal, 10)
        .frame(height: 28)
        .background(.black.opacity(0.38))
    }

    private var header: some View {
        VStack(alignment: .leading, spacing: 3) {
            Text(deviceName)
                .font(.headline)
                .foregroundStyle(.white)

            Text("Hybrid Home · backend Symbian thật")
                .font(.caption)
                .foregroundStyle(.white.opacity(0.82))

            Text("\(visibleApps.count) ứng dụng từ AppList/AppArc")
                .font(.caption2)
                .foregroundStyle(.white.opacity(0.68))
        }
        .padding(.top, 12)
    }

    private var softkeyBar: some View {
        HStack {
            Text("Điện thoại")
            Spacer()
            Text("Menu")
            Spacer()
            Text("Tuỳ chọn")
        }
        .font(.caption.bold())
        .foregroundStyle(.white)
        .padding(.horizontal, 12)
        .frame(height: 36)
        .background(.black.opacity(0.48))
    }
}
'''
home_path.write_text(home, encoding="utf-8")

print("HYBRIDHOME-UPSTREAM1 patch applied")
print(f"  ContentView: {content_path}")
print(f"  Home view:   {home_path}")
print(f"  CMake:       {cmake_path}")
