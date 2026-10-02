import AVFoundation

class AudioEngineManager {
    static let shared = AudioEngineManager()
    
    private let engine = AVAudioEngine()
    private let playerNode = AVAudioPlayerNode()
    
    var dspEnginePtr: UnsafeMutableRawPointer? = nil
    
    var isLowLatencyMode: Bool = false {
        didSet {
            updateBufferSize()
        }
    }
    
    private(set) var isRecording = false
    
    // NEU: Hält das Betriebssystem davon ab, die App schlafen zu legen
    private var powerActivity: NSObjectProtocol?
    
    private init() {
        dspEnginePtr = createRACEEngine(68.0, -2.3, 0.5, true)
        
        // NEU: Listener für CoreAudio Resets (Hardware Clock Drift)
        NotificationCenter.default.addObserver(
            self,
            selector: #selector(handleConfigurationChange),
            name: .AVAudioEngineConfigurationChange,
            object: nil
        )
    }
    
    // NEU: Diese Funktion wird automatisch aufgerufen, wenn macOS die Audio-Geräte resettet
    @objc private func handleConfigurationChange() {
        print("AVAudioEngine: Configuration Change erkannt (z.B. durch Clock Drift). Starte Engine neu...")
        // Kurzer Delay, damit macOS die Hardware neu sortieren kann
        DispatchQueue.main.asyncAfter(deadline: .now() + 0.5) {
            self.setupAndStart()
        }
    }
    
    func setupAndStart() {
        // 1. Verhindere App Nap und Ruhezustand während der Audio-Verarbeitung
                if powerActivity == nil {
                    powerActivity = ProcessInfo.processInfo.beginActivity(
                        options: [.userInitiated, .latencyCritical, .idleSystemSleepDisabled],
                        reason: "OAPW Realtime Audio DSP"
                    )
                    print("macOS App Nap und Ruhezustand für OAPW blockiert.")
                }
        
        let inputNode = engine.inputNode
        let mainMixer = engine.mainMixerNode
        let inputFormat = inputNode.outputFormat(forBus: 0)
        
        engine.attach(playerNode)
        engine.connect(playerNode, to: mainMixer, format: inputFormat)
        
        installTap(withMode: isLowLatencyMode)
        
        do {
            engine.prepare()
            try engine.start()
            playerNode.play()
            print("AVAudioEngine läuft: Nutze macOS Standard-Eingabe und Standard-Ausgabe.")
        } catch {
            print("Fehler beim Starten der AudioEngine: \(error.localizedDescription)")
        }
    }
    
    // ... hier folgen unverändert updateBufferSize() und der Rest der Datei ...
        
    private func updateBufferSize() {
        guard engine.isRunning else { return }
        let inputNode = engine.inputNode
        inputNode.removeTap(onBus: 0)
        installTap(withMode: isLowLatencyMode)
        print("Audio-Puffergröße geändert auf: \(isLowLatencyMode ? "256" : "1024")")
    }
    
    private func installTap(withMode lowLatency: Bool) {
        let inputNode = engine.inputNode
        let inputFormat = inputNode.outputFormat(forBus: 0)
        let bufferSize: AVAudioFrameCount = lowLatency ? 256 : 1024
        
        inputNode.removeTap(onBus: 0)
        inputNode.installTap(onBus: 0, bufferSize: bufferSize, format: inputFormat) { [weak self] (buffer, time) in
            self?.processAudioBuffer(buffer: buffer)
        }
    }
    
    // NEU: Aufnahme delegiert komplett an C++
    func startRecording(to url: URL) {
        let path = url.path
        let sampleRate = Int32(engine.inputNode.outputFormat(forBus: 0).sampleRate)
        
        // Wandelt den Swift-String für C++ um
        path.withCString { cString in
            if let ptr = dspEnginePtr {
                wrapper_startRecording(ptr, cString, sampleRate)
            }
        }
        
        isRecording = true
        print("Raw-Aufnahme (Lock-Free) gestartet: \(path)")
    }

    func stopRecording() {
        if let ptr = dspEnginePtr {
            wrapper_stopRecording(ptr)
        }
        isRecording = false
        print("Aufnahme beendet.")
    }
    
    private func processAudioBuffer(buffer: AVAudioPCMBuffer) {
        let frameLength = buffer.frameLength
        guard frameLength > 0 else { return }
        
        guard let newBuffer = AVAudioPCMBuffer(pcmFormat: buffer.format, frameCapacity: frameLength) else { return }
        newBuffer.frameLength = frameLength
        
        guard let srcData = buffer.floatChannelData,
              let dstData = newBuffer.floatChannelData else { return }
        
        let channelCount = Int(buffer.format.channelCount)
        for ch in 0..<channelCount {
            memcpy(dstData[ch], srcData[ch], Int(frameLength) * MemoryLayout<Float>.size)
        }
        
        let leftChannel = dstData[0]
        let rightChannel = channelCount > 1 ? dstData[1] : leftChannel
        
        // --- 1. RAW-AUDIO IN DEN LOCK-FREE RINGPUFFER (Hintergrund-Aufnahme) ---
        if isRecording, let ptr = dspEnginePtr {
            wrapper_enqueueRawSamples(ptr, leftChannel, rightChannel, Int32(frameLength))
        }
        
        // --- 2. DSP-VERARBEITUNG IN C++ (Direkt auf der Kopie) ---
        if let ptr = dspEnginePtr {
            wrapper_processSamples(ptr, leftChannel, rightChannel, Int32(frameLength))
        }
        
        playerNode.scheduleBuffer(newBuffer, at: nil, options: [], completionHandler: nil)
    }
}
