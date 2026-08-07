import AVFoundation

class AudioEngineManager {
    static let shared = AudioEngineManager()
    
    private let engine = AVAudioEngine()
    private let playerNode = AVAudioPlayerNode()
    private var debugCounter = 0
    
    var dspCallback: ((UnsafeMutablePointer<Float>, UnsafeMutablePointer<Float>, Int32) -> Void)?
    
    private init() {}
    
    func setupAndStart() {
        let inputNode = engine.inputNode
        let mainMixer = engine.mainMixerNode
        
        // 1. Eingangs- und Ausgangsformate explizit ermitteln
        let inputFormat = inputNode.outputFormat(forBus: 0)
        let outputFormat = mainMixer.outputFormat(forBus: 0)
        
        // 2. Player an den Mixer anschließen
        engine.attach(playerNode)
        engine.connect(playerNode, to: mainMixer, format: inputFormat)
        
        // 3. Tap auf den Eingangs-Node setzen
        inputNode.removeTap(onBus: 0) // Eventuelle alte Taps entfernen
        inputNode.installTap(onBus: 0, bufferSize: 1024, format: inputFormat) { [weak self] (buffer, time) in
            self?.processAudioBuffer(buffer: buffer)
        }
        
        do {
            engine.prepare()
            try engine.start()
            playerNode.play()
            print("AVAudioEngine läuft: Routing aktiv")
        } catch {
            print("Fehler beim Starten der AudioEngine: \(error.localizedDescription)")
        }
    }
    
    private func processAudioBuffer(buffer: AVAudioPCMBuffer) {
        debugCounter += 1
                if debugCounter % 40 == 0 {
                    print("Herzschlag: Audio-Puffer mit \(buffer.frameLength) Frames empfangen!")
                }
        let frameLength = buffer.frameLength
        guard frameLength > 0 else { return }
        
        // 1. Eigenen Puffer erzeugen
        guard let newBuffer = AVAudioPCMBuffer(pcmFormat: buffer.format, frameCapacity: frameLength) else { return }
        newBuffer.frameLength = frameLength
        
        guard let srcData = buffer.floatChannelData,
              let dstData = newBuffer.floatChannelData else { return }
        
        let channelCount = Int(buffer.format.channelCount)
        
        // 2. Daten kopieren
        for ch in 0..<channelCount {
            memcpy(dstData[ch], srcData[ch], Int(frameLength) * MemoryLayout<Float>.size)
        }
        
        // 3. DSP-Verarbeitung (C++ Engine)
        let leftChannel = dstData[0]
        let rightChannel = channelCount > 1 ? dstData[1] : leftChannel
        
        dspCallback?(leftChannel, rightChannel, Int32(frameLength))
        
        // 4. Dem Player übergeben
                playerNode.scheduleBuffer(newBuffer, at: nil, options: [], completionHandler: nil)
    }
}
