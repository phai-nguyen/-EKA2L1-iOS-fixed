#!/usr/bin/env python3
from pathlib import Path
import sys

root = Path(sys.argv[1] if len(sys.argv) > 1 else ".")
app = root / "src/emu/ios/App"
cmake = root / "src/emu/ios/CMakeLists.txt"

compat = r'''import SwiftUI
import UIKit

// Compatibility shims that keep the modern SwiftUI implementation on iOS 16+
// while providing equivalent navigation/share/layout behaviour on iOS 15.

struct CompatNavigationContainer<Content: View>: View {
    private let content: () -> Content

    init(@ViewBuilder content: @escaping () -> Content) {
        self.content = content
    }

    @ViewBuilder
    var body: some View {
        if #available(iOS 16.0, *) {
            NavigationStack {
                content()
            }
        } else {
            NavigationView {
                content()
            }
            .navigationViewStyle(.stack)
        }
    }
}

extension View {
    @ViewBuilder
    func compatNavigationDestination<Destination: View>(
        isPresented: Binding<Bool>,
        @ViewBuilder destination: @escaping () -> Destination
    ) -> some View {
        if #available(iOS 16.0, *) {
            navigationDestination(isPresented: isPresented) {
                destination()
            }
        } else {
            background(
                NavigationLink(
                    destination: destination(),
                    isActive: isPresented
                ) {
                    EmptyView()
                }
                .hidden()
            )
        }
    }

    @ViewBuilder
    func compatToolbarTitleMenu<MenuContent: View>(
        @ViewBuilder content: @escaping () -> MenuContent
    ) -> some View {
        if #available(iOS 16.0, *) {
            toolbarTitleMenu {
                content()
            }
        } else {
            toolbar {
                ToolbarItem(placement: .navigationBarLeading) {
                    Menu {
                        content()
                    } label: {
                        Image(systemName: "iphone")
                    }
                }
            }
        }
    }

    @ViewBuilder
    func compatLargePresentationDetent() -> some View {
        if #available(iOS 16.0, *) {
            presentationDetents([.large])
        } else {
            self
        }
    }

    @ViewBuilder
    func compatMediumPresentationDetent() -> some View {
        if #available(iOS 16.0, *) {
            presentationDetents([.medium])
        } else {
            self
        }
    }
}

struct CompatLabeledContent<Content: View>: View {
    let title: LocalizedStringKey
    private let content: () -> Content

    init(_ title: LocalizedStringKey, @ViewBuilder content: @escaping () -> Content) {
        self.title = title
        self.content = content
    }

    @ViewBuilder
    var body: some View {
        if #available(iOS 16.0, *) {
            LabeledContent(title) {
                content()
            }
        } else {
            HStack {
                Text(title)
                Spacer()
                content()
            }
        }
    }
}

struct CompatMenu<Content: View>: View {
    let title: LocalizedStringKey
    let systemImage: String
    private let content: () -> Content

    init(_ title: LocalizedStringKey,
         systemImage: String,
         @ViewBuilder content: @escaping () -> Content) {
        self.title = title
        self.systemImage = systemImage
        self.content = content
    }

    var body: some View {
        Menu {
            content()
        } label: {
            Label(title, systemImage: systemImage)
        }
    }
}

struct CompatFileShareLink<Label: View>: View {
    let item: URL
    private let label: () -> Label
    @State private var showingShareSheet = false

    init(item: URL, @ViewBuilder label: @escaping () -> Label) {
        self.item = item
        self.label = label
    }

    @ViewBuilder
    var body: some View {
        if #available(iOS 16.0, *) {
            ShareLink(item: item) {
                label()
            }
        } else {
            Button {
                showingShareSheet = true
            } label: {
                label()
            }
            .sheet(isPresented: $showingShareSheet) {
                CompatActivityView(items: [item])
            }
        }
    }
}


struct CompatAnyShape: Shape {
    private let makePath: (CGRect) -> Path

    init<S: Shape>(_ shape: S) {
        makePath = { rect in shape.path(in: rect) }
    }

    func path(in rect: CGRect) -> Path {
        makePath(rect)
    }
}

func isExternalDisplaySessionRole(_ role: UISceneSession.Role) -> Bool {
    if #available(iOS 16.0, *) {
        return role == .windowExternalDisplayNonInteractive
    } else {
        return role == .windowExternalDisplay
    }
}

extension View {
    @ViewBuilder
    func compatHideNavigationChrome() -> some View {
        if #available(iOS 16.0, *) {
            toolbar(.hidden, for: .navigationBar)
                .persistentSystemOverlays(.hidden)
        } else {
            navigationBarHidden(true)
        }
    }
}

private struct CompatActivityView: UIViewControllerRepresentable {
    let items: [Any]

    func makeUIViewController(context: Context) -> UIActivityViewController {
        UIActivityViewController(activityItems: items, applicationActivities: nil)
    }

    func updateUIViewController(_ uiViewController: UIActivityViewController,
                                context: Context) {}
}
'''

compat_path = app / "IOS15Compat.swift"
compat_path.write_text(compat)

cm = cmake.read_text()
needle = '    "${EKA2L1_IOS_APP_DIR}/EKA2L1App.swift"\n'
add = needle + '    "${EKA2L1_IOS_APP_DIR}/IOS15Compat.swift"\n'
if 'IOS15Compat.swift' not in cm:
    if needle not in cm:
        raise SystemExit("Could not find iOS Swift source list")
    cm = cm.replace(needle, add, 1)
    cmake.write_text(cm)

# APIs whose modern implementations are isolated in IOS15Compat.swift.
for path in app.glob("*.swift"):
    if path.name == "IOS15Compat.swift":
        continue
    text = path.read_text()
    text = text.replace("NavigationStack {", "CompatNavigationContainer {")
    text = text.replace(".navigationDestination(", ".compatNavigationDestination(")
    text = text.replace(".toolbarTitleMenu {", ".compatToolbarTitleMenu {")
    text = text.replace(".presentationDetents([.large])", ".compatLargePresentationDetent()")
    text = text.replace(".presentationDetents([.medium])", ".compatMediumPresentationDetent()")
    text = text.replace("ShareLink(item:", "CompatFileShareLink(item:")
    text = text.replace("LabeledContent(", "CompatLabeledContent(")
    text = text.replace('Menu("home.install", systemImage:', 'CompatMenu("home.install", systemImage:')
    text = text.replace('Menu("home.more", systemImage:', 'CompatMenu("home.more", systemImage:')
    text = text.replace("ToolbarItemGroup(placement: .topBarTrailing)",
                        "ToolbarItemGroup(placement: .navigationBarTrailing)")
    path.write_text(text)


# ToolbarContentBuilder's conditional ToolbarContent conformances arrived in
# iOS 16. Keep the ToolbarContent structure fixed; condition only the inner
# ViewBuilder content, which is iOS 15-safe.
path = app / "ContentView.swift"
text = path.read_text()
start = text.index("    private var statusToolbarItem: some ToolbarContent {")
end = text.index("    private func bootIfNeeded()", start)
toolbar = r'''    @ToolbarContentBuilder
    private var toolbarContent: some ToolbarContent {
        ToolbarItem(placement: .status) {
            if store.busy {
                ProgressView()
                    .controlSize(.small)
                    .fixedSize()
            }
        }

        ToolbarItemGroup(placement: .navigationBarTrailing) {
            Group {
                if !store.devices.isEmpty {
                    CompatMenu("home.install", systemImage: "plus") {
                        Button {
                            homeImportTarget = .sis
                            showingHomeImporter = true
                        } label: {
                            Label("home.install.sis", systemImage: "square.and.arrow.down")
                        }

                        if store.deviceIsEKA1 {
                            Button {
                                homeImportTarget = .ngage
                                showingHomeImporter = true
                            } label: {
                                Text("home.installNGage")
                                Text("home.installNGage.subtitle")
                                Image(systemName: "gamecontroller")
                            }

                            Button {
                                homeImportTarget = .card
                                showingHomeImporter = true
                            } label: {
                                Text("home.mountCard")
                                Text("home.mountCard.subtitle")
                                Image(systemName: "sdcard")
                            }

                            if let card = store.mountedCardName {
                                Button(role: .destructive) {
                                    if store.ejectCard() {
                                        banner = String(localized: "home.card.ejected")
                                    }
                                } label: {
                                    Text("home.card.eject")
                                    Text(card)
                                    Image(systemName: "eject")
                                }
                            }
                        } else {
                            Button {
                                homeImportTarget = .ngage2
                                showingHomeImporter = true
                            } label: {
                                Text("home.installNGage2")
                                Text("home.installNGage2.subtitle")
                                Image(systemName: "arrow.down.doc")
                            }
                        }

                        Divider()

                        Button {
                            homeImportTarget = .font
                            showingHomeImporter = true
                        } label: {
                            Text("home.installFonts")
                            Text("home.installFonts.subtitle")
                            Image(systemName: "textformat")
                        }
                    }
                    .disabled(store.busy)

                    CompatMenu("home.more", systemImage: "ellipsis.circle") {
                        Button {
                            showingSettings = true
                        } label: {
                            Label("settings.title", systemImage: "gearshape")
                        }

                        Button {
                            showSystemApps.toggle()
                        } label: {
                            if showSystemApps {
                                Label("home.hideSystemApps", systemImage: "eye.slash")
                            } else {
                                Label("home.showSystemApps", systemImage: "eye")
                            }
                        }

                        Divider()

                        Button {
                            showingOnboarding = true
                        } label: {
                            Label("onboarding.title", systemImage: "questionmark.circle")
                        }
                    }
                    .disabled(store.busy)
                }
            }
        }
    }

'''
text = text[:start] + toolbar + text[end:]
path.write_text(text)

# External display role name changed in iOS 16. Accept the legacy iOS 15 role.
for filename in ["EKA2L1App.swift", "ExternalDisplay.swift"]:
    path = app / filename
    text = path.read_text()
    text = text.replace(
        "window?.windowScene?.session.role == .windowExternalDisplayNonInteractive",
        "window?.windowScene.map { isExternalDisplaySessionRole($0.session.role) } == true"
    )
    text = text.replace(
        "session.role == .windowExternalDisplayNonInteractive",
        "isExternalDisplaySessionRole(session.role)"
    )
    path.write_text(text)

# iOS 15 does not have toolbar(for:) / persistentSystemOverlays.
path = app / "EmulatorView.swift"
text = path.read_text()
text = text.replace(
    '''        .toolbar(.hidden, for: .navigationBar)
        .persistentSystemOverlays(.hidden)
''',
    '''        .compatHideNavigationChrome()
'''
)

# Orientation update APIs changed in iOS 16. Use the pre-iOS 16 rotation path
# for iOS 15 and geometry requests on newer systems.
text = text.replace(
    "rootViewController?.setNeedsUpdateOfSupportedInterfaceOrientations()",
    "refreshSupportedOrientations()"
)
old_request = '''        scene.requestGeometryUpdate(.iOS(interfaceOrientations: landscape ? .landscape : .portrait))
        // The request is rejected while a navigation transition is running (the
        // emulator screen is usually mid-push), so confirm after it settles and
        // re-request once if it didn't stick.
        DispatchQueue.main.asyncAfter(deadline: .now() + 0.7) {
            guard let scene = activeScene,
                  scene.interfaceOrientation.isLandscape != landscape else { return }
            routeOrientationThroughLiveControllers(from: scene.keyWindow?.rootViewController)
            scene.requestGeometryUpdate(.iOS(interfaceOrientations: landscape ? .landscape : .portrait))
        }
'''
new_request = '''        requestSceneOrientation(scene, landscape: landscape)
        // A navigation transition can temporarily reject rotation. Confirm
        // after it settles and request once more if needed.
        DispatchQueue.main.asyncAfter(deadline: .now() + 0.7) {
            guard let scene = activeScene,
                  scene.interfaceOrientation.isLandscape != landscape else { return }
            routeOrientationThroughLiveControllers(from: scene.keyWindow?.rootViewController)
            requestSceneOrientation(scene, landscape: landscape)
        }
'''
if old_request not in text:
    raise SystemExit("orientation request block not found")
text = text.replace(old_request, new_request)

insert_before = '''    private static var activeScene: UIWindowScene? {
'''
orientation_helpers = '''    private static func refreshSupportedOrientations() {
        if #available(iOS 16.0, *) {
            rootViewController?.setNeedsUpdateOfSupportedInterfaceOrientations()
        } else {
            UIViewController.attemptRotationToDeviceOrientation()
        }
    }

    private static func requestSceneOrientation(_ scene: UIWindowScene, landscape: Bool) {
        if #available(iOS 16.0, *) {
            scene.requestGeometryUpdate(
                .iOS(interfaceOrientations: landscape ? .landscape : .portrait)
            )
        } else {
            UIDevice.current.setValue(
                landscape ? UIInterfaceOrientation.landscapeRight.rawValue
                          : UIInterfaceOrientation.portrait.rawValue,
                forKey: "orientation"
            )
            UIViewController.attemptRotationToDeviceOrientation()
        }
    }

'''
if "private static func refreshSupportedOrientations()" not in text:
    text = text.replace(insert_before, orientation_helpers + insert_before, 1)
path.write_text(text)

# SwiftUI.AnyShape is iOS 16+. Use the simple local type eraser above.
path = app / "KeypadComponents.swift"
text = path.read_text().replace("AnyShape", "CompatAnyShape")
path.write_text(text)

# Register a legacy external-display scene role so iOS 15 can create the same
# delegate. Newer iOS continues to use the non-interactive role.
plist = root / "src/emu/ios/Resources/Info.plist"
text = plist.read_text()
if "UIWindowSceneSessionRoleExternalDisplay</key>" not in text:
    anchor = '''            <key>UIWindowSceneSessionRoleExternalDisplayNonInteractive</key>
'''
    legacy = '''            <key>UIWindowSceneSessionRoleExternalDisplay</key>
            <array>
                <dict>
                    <key>UISceneConfigurationName</key>
                    <string>Game Display</string>
                    <key>UISceneClassName</key>
                    <string>UIWindowScene</string>
                    <key>UISceneDelegateClassName</key>
                    <string>$(PRODUCT_MODULE_NAME).ExternalDisplaySceneDelegate</string>
                </dict>
            </array>
'''
    if anchor not in text:
        raise SystemExit("scene manifest anchor not found")
    text = text.replace(anchor, legacy + anchor, 1)
    plist.write_text(text)


# LocalizedStringResource is newer than our minimum. Store keys as ordinary
# strings and hand them to SwiftUI as LocalizedStringKey on every supported OS.
path = app / "OnboardingView.swift"
text = path.read_text()
text = text.replace(
    "private let pages: [(symbol: String, title: LocalizedStringResource, body: LocalizedStringResource)]",
    "private let pages: [(symbol: String, title: String, body: String)]"
)
text = text.replace("Text(pages[index].title)", "Text(LocalizedStringKey(pages[index].title))")
text = text.replace("Text(pages[index].body)", "Text(LocalizedStringKey(pages[index].body))")
path.write_text(text)

# Clock-based Task.sleep is newer than iOS 15. The nanosecond overload is
# available on the deployment range we support.
path = app / "ContentView.swift"
text = path.read_text()
text = text.replace(
    "try? await Task.sleep(until: .now + .seconds(1))",
    "try? await Task.sleep(nanoseconds: 1_000_000_000)"
)
path.write_text(text)

# ViewThatFits is iOS 16+. Preserve it on modern systems and use the compact
# vertical editor controls on iOS 15.
def patch_view_that_fits(path: Path, leading: str, bar_name: str, menu_name: str):
    text = path.read_text()
    old = f'''    private var editorSettings: some View {{
        ViewThatFits(in: .horizontal) {{
            HStack(spacing: 8) {{
                {bar_name}
                    {leading}
                {menu_name}
            }}

            VStack(spacing: 8) {{
                {bar_name}
                {menu_name}
            }}
        }}
    }}
'''
    new = f'''    @ViewBuilder
    private var editorSettings: some View {{
        if #available(iOS 16.0, *) {{
            ViewThatFits(in: .horizontal) {{
                HStack(spacing: 8) {{
                    {bar_name}
                        {leading}
                    {menu_name}
                }}

                VStack(spacing: 8) {{
                    {bar_name}
                    {menu_name}
                }}
            }}
        }} else {{
            VStack(spacing: 8) {{
                {bar_name}
                {menu_name}
            }}
        }}
    }}
'''
    if old not in text:
        raise SystemExit(f"ViewThatFits block not found in {path.name}")
    path.write_text(text.replace(old, new, 1))

patch_view_that_fits(
    app / "DisplayLayout.swift",
    ".frame(minWidth: 260, maxWidth: 300)",
    "scaleBar",
    "gravityPicker"
)
patch_view_that_fits(
    app / "VirtualKeypad.swift",
    ".frame(minWidth: 240, maxWidth: 300)",
    "opacityBar",
    "visibilityMenu"
)

# Make iOS 15 the patched source tree's own default as well as the CI value.
root_cmake = root / "CMakeLists.txt"
root_cmake_text = root_cmake.read_text()
root_cmake_text = root_cmake_text.replace(
    'set(EKA2L1_IOS_DEPLOYMENT_TARGET "16.0" CACHE STRING "Minimum iOS version to target")',
    'set(EKA2L1_IOS_DEPLOYMENT_TARGET "15.0" CACHE STRING "Minimum iOS version to target")'
)
root_cmake.write_text(root_cmake_text)

build_ios = root / "scripts/build_ios.sh"
build_ios_text = build_ios.read_text()
build_ios_text = build_ios_text.replace(
    "EKA2L1_IOS_DEPLOYMENT_TARGET   default 16.0",
    "EKA2L1_IOS_DEPLOYMENT_TARGET   default 15.0"
)
build_ios_text = build_ios_text.replace(
    'DEPLOYMENT_TARGET="${EKA2L1_IOS_DEPLOYMENT_TARGET:-16.0}"',
    'DEPLOYMENT_TARGET="${EKA2L1_IOS_DEPLOYMENT_TARGET:-15.0}"'
)
build_ios.write_text(build_ios_text)

print("Applied iOS 15 compatibility layer")
