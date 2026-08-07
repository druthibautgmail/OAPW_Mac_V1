import SwiftUI

@main
struct OAPW_Mac_V1App: App {
    var body: some Scene {
        WindowGroup {
            AmbiophonicsDashboard()
                .frame(minWidth: 800, minHeight: 600)
                .onAppear {
                    // Zwingt die App beim Start garantiert in den Vordergrund
                    NSApplication.shared.activate(ignoringOtherApps: true)
                }
        }
        // Den hiddenTitleBar-Befehl haben wir testweise entfernt
    }
}
