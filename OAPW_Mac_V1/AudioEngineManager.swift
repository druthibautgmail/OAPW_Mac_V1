import AVFoundation

class AudioEngineManager {
    static let shared = AudioEngineManager()
    
    private let engine = AVAudioEngine()
    private let playerNode = AVAudioPlayerNode()
    
    // NEU: Globaler Pointer für die C++ Engine, damit das Dashboard (FFT/EQ) darauf zugreifen kann
    var dspEnginePtr: UnsafeMutableRawPointer? = nil
    
    // Steuert die Puffergröße dynamisch je nach Modus
    var isLowLatencyMode: Bool = false {
        didSet {
            updateBufferSize()
        }
    }
    
    // Variablen für die native macOS Aufnahme
    private var audioFile: AVAudioFile?
    private var isRecording = false
    
    private init() {
            // NEU: Startet mit echten 68.0 µs Delay statt 0.5 µs
            dspEnginePtr = createRACEEngine(68.0, -2.3, 0.5, true)
        }
    
    func setupAndStart() {
        let inputNode = engine.inputNode
        let mainMixer = engine.mainMixerNode
        
        // Holt sich das Format des aktuellen macOS-Standardeingangs
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

    // ----------------------------------------
        
    // Methode zum Umschalten der Puffergröße im laufenden Betrieb
    private func updateBufferSize() {
        guard engine.isRunning else { return }
        
        let inputNode = engine.inputNode
        inputNode.removeTap(onBus: 0)
        installTap(withMode: isLowLatencyMode)
        print("Audio-Puffergröße dynamisch geändert auf: \(isLowLatencyMode ? "256 (Movie / Low-Latency)" : "1024 (Music / HQ)")")
    }
    
    private func installTap(withMode lowLatency: Bool) {
        let inputNode = engine.inputNode
        let inputFormat = inputNode.outputFormat(forBus: 0)
        
        // 256 Frames im Movie-Modus (~5.8ms), 1024 Frames im Music-Modus (~23.2ms)
        let bufferSize: AVAudioFrameCount = lowLatency ? 256 : 1024
        
        inputNode.removeTap(onBus: 0)
        inputNode.installTap(onBus: 0, bufferSize: bufferSize, format: inputFormat) { [weak self] (buffer, time) in
            self?.processAudioBuffer(buffer: buffer)
        }
    }
    
    // Aufnahme-Steuerung über AVAudioFile
    func startRecording(to url: URL) {
        let format = engine.inputNode.outputFormat(forBus: 0)
        do {
            audioFile = try AVAudioFile(forWriting: url, settings: format.settings)
            isRecording = true
            print("Aufnahme gestartet: \(url.path)")
        } catch {
            print("Fehler beim Erstellen der Audiodatei: \(error.localizedDescription)")
        }
    }

    func stopRecording() {
        isRecording = false
        audioFile = nil // Schließt die Datei sauber ab
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
        
        // 1. C++ DSP-Verarbeitung: Sendet den Ton an die RACE Engine, welche ihn in-place verändert
        if let ptr = dspEnginePtr {
            wrapper_processSamples(ptr, leftChannel, rightChannel, Int32(frameLength))
        }
        
        // 2. Das verarbeitete RACE/EQ-Signal bitgenau in die Datei schreiben
        if isRecording, let file = audioFile {
            do {
                try file.write(from: newBuffer)
            } catch {
                print("Fehler beim Schreiben der Audiodaten: \(error.localizedDescription)")
            }
        }
        
        playerNode.scheduleBuffer(newBuffer, at: nil, options: [], completionHandler: nil)
    }
}
