import SwiftUI

// MACHINE1-L is deliberately a research-only surface. It runs an isolated ARM
// core against the installed RH-29 ROM and never replaces the normal HLE boot.
struct RH29MachineProbeView: View {
    private let budgets: [UInt32] = [1_000, 10_000, 100_000, 1_000_000]

    @State private var instructionBudget: UInt32 = 1_000
    @State private var running = false
    @State private var reportText = ""
    @State private var reportSucceeded = false
    @State private var reportURL: URL?

    var body: some View {
        Form {
            Section("MACHINE1-L") {
                Text("Probe giữ state machine flash và thêm đúng unlock cycle 2 AMD x16 đã quan sát: chỉ sau stage 1, 0x0055 → 0x02000554 (word 0x2AA) mới được chấp nhận. Command thứ ba vẫn chưa giả lập.")
                    .font(.footnote)
                    .foregroundStyle(.secondary)

                Picker("Ngân sách lệnh", selection: $instructionBudget) {
                    ForEach(budgets, id: \.self) { budget in
                        Text(budget.formatted()).tag(budget)
                    }
                }
                .pickerStyle(.menu)

                Button {
                    runProbe()
                } label: {
                    if running {
                        HStack {
                            ProgressView()
                            Text("Đang chạy probe…")
                        }
                    } else {
                        Label("Chạy probe", systemImage: "play.fill")
                    }
                }
                .disabled(running)
            }

            if !reportText.isEmpty {
                Section {
                    ScrollView(.horizontal) {
                        Text(reportText)
                            .font(.system(.caption, design: .monospaced))
                            .textSelection(.enabled)
                    }
                    .frame(minHeight: 240, alignment: .topLeading)

                    if let reportURL {
                        ShareLink(item: reportURL) {
                            Label("Chia sẻ báo cáo", systemImage: "square.and.arrow.up")
                        }
                    }
                } header: {
                    Text(reportSucceeded ? "Báo cáo" : "Probe dừng có kiểm soát")
                }
            }
        }
        .navigationTitle("RH-29 Machine Probe")
        .navigationBarTitleDisplayMode(.inline)
    }

    private func runProbe() {
        guard !running else { return }
        running = true
        let budget = instructionBudget

        Task { @MainActor in
            let item = await Task.detached(priority: .userInitiated) {
                EKA2L1Bridge.runRH29MachineProbe(instructionBudget: budget)
            }.value

            reportSucceeded = item.succeeded
            reportText = item.text
            let url = URL(fileURLWithPath: documentsRoot())
                .appendingPathComponent("RH29_MACHINE1_L.txt")
            do {
                try item.text.write(to: url, atomically: true, encoding: .utf8)
                reportURL = url
            } catch {
                reportURL = nil
                reportText += "\nHOST_REPORT_WRITE_ERROR=\(error.localizedDescription)\n"
            }
            running = false
        }
    }
}
