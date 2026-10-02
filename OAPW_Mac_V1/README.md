#  <#OAPW\_Mac\_V2 README.md#>

## OAPW_Mac: Systemvoraussetzungen und Signalfluss

### Systemvoraussetzungen

Betriebssystem: macOS (nativ kompiliert für Apple Silicon / ARM64, z.B. macOS Tahoe oder macOS 27 Golden Gate).

Audio-Subsystem: Apple CoreAudio.

Architektur: Die DSP-Engine für die Recursive Ambiophonic Crosstalk Elimination (RACE) ist in C++ und Swift implementiert und erfordert für latenzfreien und zukunftssicheren Betrieb auf Apple Silicon eine native ARM64-Kompilierung. Dazu wird idealerweise XCode auf dem Zielsystem eingesetzt, um Spezifika der jeweils verfügbaren Hardware bei der Kompilierung zu berücksichtigen.

### Schnittstellen-Anforderungen

Virtueller Audiotreiber: Installation von BlackHole 2ch (oder einem vergleichbaren Loopback-Treiber) für das systeminterne, verlustfreie Routing der Audiosignale zwischen der Abspiel-Software und dem OAPW_Mac Prozessor.

Audio-MIDI-Setup: Die Konfiguration erfolgt über die macOS-System-App "Audio-MIDI-Setup". Hier werden virtuelle Ein- und Ausgabegeräte (Aggregate Devices) verwaltet, um Taktraten und Audioformate (z.B. 44.1 kHz / 16 Bit oder hochauflösend) zu synchronisieren.

Hardware-Ausgabe: Die Applikation dient zur internen Verarbeitung von Audio-Streams und nutzt ausschließlich die interne Audio-Hardware des Macs (z.B. eines MacBook Pro mit M1 Prozessor) zur finalen Signalausgabe.

### Konfiguration der virtuellen Ein-/Ausgabegeräte

Um den Audio-Stream abzufangen und durch den OAPW_Mac zu leiten, sind im Audio-MIDI-Setup folgende Schritte essenziell:

Haupt-Audioquelle: Die systemweite Audioausgabe von macOS oder die Ausgabe der spezifischen Player-Software wird auf das virtuelle Ausgabegerät BlackHole 2ch gesetzt.

OAPW_Mac Eingang: Die OAPW_Mac App greift den eingehenden Stereo-Stream (Links/Rechts) über CoreAudio direkt von BlackHole 2ch ab.

OAPW_Mac Ausgang: Das mittels RACE prozessierte Signal wird von OAPW_Mac an die interne Audioausgabe des Macs gesendet.

Hauptgerät / Aggregate Device (Optional): Falls die Clock-Synchronisation (Drift-Korrektur) zwischen Ein- und Ausgabe zwingend ist, wird im Audio-MIDI-Setup ein "Hauptgerät" erstellt, das BlackHole 2ch und die interne Audioausgabe bündelt.

### Fluss der Signalverarbeitung
```mermaid
graph TD
    A[Audio-Player / macOS System-Audio] -->|Lossless Stereo L/R| B(BlackHole 2ch - Virtuelles Audiogerät)
    B -->|CoreAudio Input| C{OAPW_Mac DSP Engine}
    C -->|IIR Filtering & RACE Crosstalk Elimination| C
    C -->|Ambiophonic Output L/R| D[Interne Mac Audio-Hardware]
    D --> E((Lautsprecher))
    
