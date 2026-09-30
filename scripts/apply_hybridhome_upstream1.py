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

// HYBRIDHOME2
//
// Host-rendered Nokia 5800 RM-356 Home + Menu shell.
// All application metadata, icons and launched processes still come from the
// real EKA2L1 AppList/AppArc bridge. The original Nokia Home UID is excluded
// because this shell replaces only the blocked Home surface, not the backend.
struct S60HybridHomeView: View {
    let deviceName: String
    let apps: [EKA2L1AppItem]
    let onRefresh: () async -> Void

    @State private var showingLauncher = false

    private let realNokiaHomeUID: UInt32 = 0x102750F0
    private let telephoneUID: UInt32 = 0x100058B3
    private let contactsUID: UInt32 = 0x101F4CCE
    private let messagingUID: UInt32 = 0x100058C5
    private let menuUID: UInt32 = 0x101F4CD2

    private var visibleApps: [EKA2L1AppItem] {
        apps
            .filter { $0.uid != realNokiaHomeUID }
            .sorted {
                $0.name.localizedCaseInsensitiveCompare($1.name) == .orderedAscending
            }
    }

    private var homeShortcuts: [EKA2L1AppItem] {
        [telephoneUID, contactsUID, messagingUID]
            .compactMap { uid in apps.first { $0.uid == uid } }
    }

    private func app(_ uid: UInt32) -> EKA2L1AppItem? {
        apps.first { $0.uid == uid }
    }

    private let launcherColumns = Array(
        repeating: GridItem(.flexible(minimum: 68, maximum: 96), spacing: 8),
        count: 4
    )

    private let shortcutColumns = Array(
        repeating: GridItem(.flexible(minimum: 86, maximum: 112), spacing: 10),
        count: 3
    )

    var body: some View {
        ZStack {
            wallpaper

            VStack(spacing: 0) {
                statusBar

                if showingLauncher {
                    launcherSurface
                } else {
                    homeSurface
                }

                softkeyBar
            }
        }
        .onAppear {
            NSLog("[HYBRIDHOME2][SHOW] device=%@ apps=%ld shortcuts=%ld",
                  deviceName, visibleApps.count, homeShortcuts.count)
        }
    }

    private var wallpaper: some View {
        ZStack {
            LinearGradient(
                colors: [
                    Color(red: 0.04, green: 0.18, blue: 0.26),
                    Color(red: 0.06, green: 0.36, blue: 0.43),
                    Color(red: 0.42, green: 0.65, blue: 0.64)
                ],
                startPoint: .topLeading,
                endPoint: .bottomTrailing
            )

            Circle()
                .fill(.white.opacity(0.06))
                .frame(width: 260, height: 260)
                .offset(x: 120, y: -180)

            Circle()
                .stroke(.white.opacity(0.08), lineWidth: 18)
                .frame(width: 330, height: 330)
                .offset(x: -130, y: 210)
        }
        .ignoresSafeArea()
    }

    private var statusBar: some View {
        HStack(spacing: 7) {
            Image(systemName: "antenna.radiowaves.left.and.right")
                .font(.caption2)

            Text("NOKIA")
                .font(.caption.bold())

            Spacer()

            TimelineView(.periodic(from: .now, by: 30)) { context in
                Text(context.date.formatted(date: .omitted, time: .shortened))
                    .font(.caption.monospacedDigit())
            }

            Image(systemName: "battery.75percent")
                .font(.caption)
        }
        .foregroundStyle(.white)
        .padding(.horizontal, 10)
        .frame(height: 28)
        .background(.black.opacity(0.38))
    }

    private var homeSurface: some View {
        ScrollView {
            VStack(spacing: 18) {
                VStack(spacing: 4) {
                    TimelineView(.periodic(from: .now, by: 30)) { context in
                        Text(context.date.formatted(date: .omitted, time: .shortened))
                            .font(.system(size: 46, weight: .light, design: .rounded))
                            .monospacedDigit()
                    }

                    Text(deviceName)
                        .font(.subheadline.weight(.semibold))

                    Text("RM-356 · Hybrid Home 2")
                        .font(.caption2)
                        .opacity(0.78)
                }
                .foregroundStyle(.white)
                .padding(.top, 24)

                if !homeShortcuts.isEmpty {
                    VStack(alignment: .leading, spacing: 10) {
                        Text("Lối tắt")
                            .font(.caption.bold())
                            .foregroundStyle(.white.opacity(0.9))

                        LazyVGrid(columns: shortcutColumns, spacing: 10) {
                            ForEach(homeShortcuts) { app in
                                appLink(app, marker: "SHORTCUT")
                            }
                        }
                    }
                    .padding(12)
                    .background(.black.opacity(0.22), in: RoundedRectangle(cornerRadius: 12))
                }

                Button {
                    showingLauncher = true
                    NSLog("[HYBRIDHOME2][OPEN_MENU] apps=%ld", visibleApps.count)
                } label: {
                    HStack {
                        Image(systemName: "square.grid.3x3.fill")
                        Text("Mở Menu ứng dụng")
                            .fontWeight(.semibold)
                        Spacer()
                        Text("\(visibleApps.count)")
                            .font(.caption.monospacedDigit())
                            .opacity(0.75)
                    }
                    .foregroundStyle(.white)
                    .padding(14)
                    .background(.black.opacity(0.28), in: RoundedRectangle(cornerRadius: 12))
                }
                .buttonStyle(.plain)

                Text("Ứng dụng và biểu tượng được lấy từ AppList/AppArc của firmware.")
                    .font(.caption2)
                    .multilineTextAlignment(.center)
                    .foregroundStyle(.white.opacity(0.70))
                    .padding(.horizontal, 18)
            }
            .padding(.horizontal, 12)
            .padding(.bottom, 20)
        }
        .refreshable {
            NSLog("[HYBRIDHOME2][REFRESH_HOME]")
            await onRefresh()
        }
    }

    private var launcherSurface: some View {
        ScrollView {
            VStack(alignment: .leading, spacing: 12) {
                HStack {
                    VStack(alignment: .leading, spacing: 2) {
                        Text("Menu ứng dụng")
                            .font(.headline)
                        Text("\(visibleApps.count) ứng dụng Symbian")
                            .font(.caption2)
                            .opacity(0.72)
                    }

                    Spacer()

                    Button {
                        showingLauncher = false
                        NSLog("[HYBRIDHOME2][CLOSE_MENU]")
                    } label: {
                        Label("Trang chủ", systemImage: "house.fill")
                            .font(.caption.bold())
                    }
                    .buttonStyle(.bordered)
                    .tint(.white.opacity(0.22))
                }
                .foregroundStyle(.white)
                .padding(.top, 10)

                LazyVGrid(columns: launcherColumns, spacing: 8) {
                    ForEach(visibleApps) { app in
                        appLink(app, marker: "LAUNCHER")
                    }
                }
            }
            .padding(.horizontal, 10)
            .padding(.bottom, 20)
        }
        .refreshable {
            NSLog("[HYBRIDHOME2][REFRESH_MENU]")
            await onRefresh()
        }
    }

    @ViewBuilder
    private func appLink(_ app: EKA2L1AppItem, marker: String) -> some View {
        NavigationLink(destination: EmulatorView(uid: app.uid)) {
            AppGridCell(uid: app.uid, name: app.name)
                .foregroundStyle(.white)
        }
        .buttonStyle(.plain)
        .simultaneousGesture(
            TapGesture().onEnded {
                NSLog("[HYBRIDHOME2][%@] uid=0x%08X name=%@",
                      marker, app.uid, app.name)
            }
        )
    }

    private var softkeyBar: some View {
        HStack(spacing: 8) {
            if let telephone = app(telephoneUID) {
                NavigationLink(destination: EmulatorView(uid: telephone.uid)) {
                    Label("Điện thoại", systemImage: "phone.fill")
                        .frame(maxWidth: .infinity)
                }
                .buttonStyle(.plain)
                .simultaneousGesture(
                    TapGesture().onEnded {
                        NSLog("[HYBRIDHOME2][SOFTKEY_PHONE] uid=0x%08X", telephone.uid)
                    }
                )
            } else {
                Text("Điện thoại")
                    .frame(maxWidth: .infinity)
                    .opacity(0.45)
            }

            Button {
                showingLauncher.toggle()
                NSLog("[HYBRIDHOME2][SOFTKEY_MENU] launcher=%@",
                      showingLauncher ? "YES" : "NO")
            } label: {
                Label(showingLauncher ? "Trang chủ" : "Menu",
                      systemImage: showingLauncher ? "house.fill" : "square.grid.3x3.fill")
                    .frame(maxWidth: .infinity)
            }
            .buttonStyle(.plain)

            if let contacts = app(contactsUID) {
                NavigationLink(destination: EmulatorView(uid: contacts.uid)) {
                    Label("Danh bạ", systemImage: "person.crop.circle.fill")
                        .frame(maxWidth: .infinity)
                }
                .buttonStyle(.plain)
                .simultaneousGesture(
                    TapGesture().onEnded {
                        NSLog("[HYBRIDHOME2][SOFTKEY_CONTACTS] uid=0x%08X", contacts.uid)
                    }
                )
            } else {
                Text("Danh bạ")
                    .frame(maxWidth: .infinity)
                    .opacity(0.45)
            }
        }
        .font(.caption.bold())
        .foregroundStyle(.white)
        .padding(.horizontal, 8)
        .frame(height: 42)
        .background(.black.opacity(0.54))
    }
}
'''
home_path.write_text(home, encoding="utf-8")

print("HYBRIDHOME-UPSTREAM1 patch applied")
print(f"  ContentView: {content_path}")
print(f"  Home view:   {home_path}")
print(f"  CMake:       {cmake_path}")
