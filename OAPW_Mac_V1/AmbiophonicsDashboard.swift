import SwiftUI

// MARK: - ViewModel für die C++ RACE Engine
@Observable
class RACEProcessorState {
    
    static let shared = RACEProcessorState()
    
    private var enginePtr: UnsafeMutableRawPointer?
    
    var isEnabled: Bool = true
    // Vorher: var masterGain: Double = 1.0
    var masterGain: Double = 0.0 // Jetzt in dB
    var attenuation: Double = -2.3
    
    // NEU: Jetzt in echten Mikrosekunden
    var delayMicroseconds: Double = 68.0
    var centerLevel: Double = 0.5
    
    var stageWidth: Double = 100.0
    var activeInput: String = "BlackHole 2ch"
    var activeOutput: String = "HDMI"
    
    private init() {
        // Init mit Float(delayMicroseconds) aufrufen
        enginePtr = createRACEEngine(Float(delayMicroseconds), Float(attenuation), 1.0, true)
        
        applyParameters()
        applyCenterLevel()
        applyVolume() // NEU: Lautstärke initial an C++ übergeben
        
        AudioEngineManager.shared.dspCallback = { [weak self] left, right, frames in
            if let ptr = self?.enginePtr {
                wrapper_processSamples(ptr, left, right, frames)
            }
        }
        
        AudioEngineManager.shared.setupAndStart()
    }
    
    // ... restlicher Code bleibt ...
    
    deinit {
        destroyRACEEngine(enginePtr)
    }
    
    func applyEnabled() {
        wrapper_setRaceEnabled(enginePtr, isEnabled)
    }
    
    func applyVolume() {
        wrapper_setVolume(enginePtr, Float(masterGain))
    }
    
    func applyParameters() {
            // Hier schicken wir jetzt die reinen Mikrosekunden an den C-Wrapper
            wrapper_setParameters(enginePtr, Float(delayMicroseconds), Float(attenuation), 1.0, true)
        }
    
    func applyCenterLevel() {
        wrapper_setCenterLevel(enginePtr, Float(centerLevel))
    }
}

// MARK: - Main Dashboard View
struct AmbiophonicsDashboard: View {
    @State private var dspState = RACEProcessorState.shared
    
    var body: some View {
        NavigationStack {
            ScrollView {
                VStack(spacing: 24) {
                    headerSection
                    
                    LazyVGrid(columns: [GridItem(.flexible()), GridItem(.flexible())], spacing: 24) {
                        
                        CardContainer {
                            VStack(alignment: .leading, spacing: 20) {
                                Label("RACE Parameters", systemImage: "waveform.path.ecg")
                                    .font(.headline)
                                    .foregroundColor(.secondary)
                                
                                // Die .onChange-Modifier erzwingen den Sync mit C++
                                // ... innerhalb des CardContainers für die "RACE Parameters":

                                SliderControl(title: "Attenuation", value: $dspState.attenuation, range: -12...0, unit: "dB")
                                    .onChange(of: dspState.attenuation) { _, _ in dspState.applyParameters() }

                                // NEU: Der Slider arbeitet jetzt direkt mit den Mikrosekunden und einem passenden Range
                                SliderControl(title: "Speaker Delay", value: $dspState.delayMicroseconds, range: 0...500, unit: "µs")
                                    .onChange(of: dspState.delayMicroseconds) { _, _ in dspState.applyParameters() }

                                SliderControl(title: "Center Level", value: $dspState.centerLevel, range: 0.0...1.0, unit: "")
                                    .onChange(of: dspState.centerLevel) { _, _ in dspState.applyCenterLevel() }
                                
                                // NEU: Der Make-Up Gain Slider
                                    SliderControl(title: "Make-Up Gain", value: $dspState.masterGain, range: 0...12, unit: "dB")
                                        .onChange(of: dspState.masterGain) { _, _ in dspState.applyVolume() }

                                // ... restlicher Code bleibt ...
                            }
                        }
                        
                        CardContainer {
                            VStack(alignment: .leading, spacing: 16) {
                                Label("Audio Routing", systemImage: "arrow.left.and.right.square")
                                    .font(.headline)
                                    .foregroundColor(.secondary)
                                
                                RoutingRow(icon: "macwindow", label: "Input", value: dspState.activeInput)
                                Divider()
                                RoutingRow(icon: "tv", label: "Output", value: dspState.activeOutput)
                                
                                Spacer(minLength: 20)
                                
                                Toggle(isOn: $dspState.isEnabled.animation(.spring)) {
                                    HStack {
                                        Image(systemName: dspState.isEnabled ? "speaker.wave.3.fill" : "speaker.slash.fill")
                                            .foregroundColor(dspState.isEnabled ? .green : .red)
                                        Text(dspState.isEnabled ? "Ambiophonics Active" : "Bypass (Stereo)")
                                            .fontWeight(.medium)
                                    }
                                }
                                .toggleStyle(.button)
                                .buttonStyle(.borderedProminent)
                                .tint(dspState.isEnabled ? .blue : .gray)
                                .frame(maxWidth: .infinity)
                                .onChange(of: dspState.isEnabled) { _, _ in dspState.applyEnabled() }
                            }
                        }
                    }
                }
                .padding(32)
            }
            .background(
                LinearGradient(
                    colors: [Color(NSColor.windowBackgroundColor), Color(NSColor.underPageBackgroundColor)],
                    startPoint: .topLeading,
                    endPoint: .bottomTrailing
                ).ignoresSafeArea()
            )
            .navigationTitle("OAPW Control Center")
        }
    }
    
    private var headerSection: some View {
        HStack {
            VStack(alignment: .leading) {
                Text("Recursive Ambiophonic Crosstalk Elimination")
                    .font(.subheadline)
                    .foregroundColor(.secondary)
                Text("DSP Engine Running")
                    .font(.caption)
                    .padding(.horizontal, 8)
                    .padding(.vertical, 4)
                    .background(Color.green.opacity(0.2))
                    .foregroundColor(.green)
                    .cornerRadius(8)
            }
            Spacer()
        }
    }
}

// MARK: - Reusable UI Components (Bleiben unverändert)
struct CardContainer<Content: View>: View {
    @ViewBuilder var content: Content
    var body: some View {
        VStack(alignment: .leading) { content }
        .padding(24)
        .frame(maxWidth: .infinity, maxHeight: .infinity, alignment: .topLeading)
        .background(.regularMaterial)
        .clipShape(RoundedRectangle(cornerRadius: 24, style: .continuous))
        .shadow(color: Color.black.opacity(0.05), radius: 10, x: 0, y: 4)
        .overlay(RoundedRectangle(cornerRadius: 24, style: .continuous).stroke(Color.white.opacity(0.2), lineWidth: 1))
    }
}

struct SliderControl: View {
    let title: String
    @Binding var value: Double
    let range: ClosedRange<Double>
    let unit: String
    var body: some View {
        VStack(alignment: .leading, spacing: 8) {
            HStack {
                Text(title).font(.subheadline)
                Spacer()
                Text("\(value, specifier: "%.1f") \(unit)")
                    .font(.system(.subheadline, design: .rounded).monospacedDigit())
                    .foregroundColor(.secondary)
            }
            Slider(value: $value, in: range).tint(.blue)
        }
    }
}

struct RoutingRow: View {
    let icon: String
    let label: String
    let value: String
    var body: some View {
        HStack(spacing: 12) {
            Image(systemName: icon).foregroundColor(.secondary).frame(width: 24)
            Text(label).font(.subheadline).foregroundColor(.secondary)
            Spacer()
            Text(value).font(.subheadline).fontWeight(.medium)
        }
    }
}
