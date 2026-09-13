import SwiftUI

@main
struct OAPW_Mac_V2App: App {
    var body: some Scene {
        // Hier steht jetzt unser gemeinsamer Titel für die Fensterleiste!
        WindowGroup("OAPW Ambiophonics – by Dr. Ulrich Thibaut") {
            AmbiophonicsDashboard()
                .frame(minWidth: 950, minHeight: 950)
        }
        .defaultSize(width: 1000, height: 1000)
        .windowResizability(.contentMinSize)
    }
}
