# OAPW for macOS (Open Ambiophonics Processing Window)

OAPW for macOS ist eine native, eigenständige Desktop-Applikation zur Echtzeit-Audiosignalverarbeitung. Sie implementiert den **Recursive Ambiophonic Crosstalk Elimination (RACE)** Algorithmus, um eine extrem breite, holografische Stereobühne über nah beieinander stehende Lautsprecher zu erzeugen.

Entwickelt von Dr. Ulrich Thibaut mit Unterstützung durch Gemini.

## 💻 Systemvoraussetzungen
* **Betriebssystem:** macOS (nativ kompiliert für Apple Silicon / ARM64, z.B. macOS Tahoe oder macOS 27 Golden Gate).
* **Audio-Subsystem:** Apple CoreAudio.
* **Architektur:** Die DSP-Engine für die Recursive Ambiophonic Crosstalk Elimination (RACE) ist in C++ und Swift implementiert und erfordert für latenzfreien und zukunftssicheren Betrieb auf Apple Silicon eine native ARM64-Kompilierung. 

## 🚀 Features
* **Nativer RACE-DSP:** Rekursive Crosstalk-Cancellation für eine perfekte Phasenauslöschung und massive Verbreiterung der Stereobühne.
* **Geometrie-Rechner:** Automatische Berechnung von Delay und Attenuation basierend auf dem Hörerabstand und der Lautsprecher-Basisbreite.
* **3-Band Parametric EQ:** Integrierter IIR-Biquad-Equalizer zur präzisen klanglichen Anpassung des Signals.
* **Realtime FFT Analyzer:** 32-Band Spektrumanalysator mit logarithmischer Skala und dynamischer Farbgebung zur visuellen Überwachung des Signals.
* **Native macOS UI:** Entwickelt mit SwiftUI für maximale Performance und nahtlose Integration in das Betriebssystem (optimiert für macOS Tahoe 26.2).

## ⚠️ Aktuelle Einschränkungen & Audio-Routing
Aufgrund der strengen Sicherheitsarchitektur von macOS (CoreAudio) ist es nativ nicht ohne Weiteres möglich, den System-Ton (z. B. direkt aus Safari, YouTube oder Apple Music) systemweit abzugreifen. 

Damit OAPW das Audiosignal verarbeiten kann, wird ein virtuelles Audio-Kabel benötigt:
1. Lade und installiere **BlackHole 2ch** (kostenloser Open-Source Audio-Treiber).
2. Nutze einen Mediaplayer, bei dem sich das Audio-Ausgabegerät explizit einstellen lässt (wir empfehlen den **VLC-Player**).
3. Stelle im VLC-Player als Audio-Ausgabegerät `BlackHole 2ch` ein.
4. OAPW greift nun automatisch das Signal von `BlackHole 2ch` ab, wendet den Ambiophonics-Effekt an und leitet den Ton an das aktive macOS-Ausgabegerät (z. B. MacBook Pro Lautsprecher oder den Klinkenausgang) weiter.

### Fluss der Signalverarbeitung

```mermaid
graph TD
    A[Audio-Player / z.B. VLC] -->|Lossless Stereo L/R| B(BlackHole 2ch - Virtuelles Audiogerät)
    B -->|CoreAudio Input| C{OAPW_Mac DSP Engine}
    C -->|IIR Filtering & RACE Crosstalk Elimination| C
    C -->|Ambiophonic Output L/R| D[Interne Mac Audio-Hardware]
    D --> E((Lautsprecher))
