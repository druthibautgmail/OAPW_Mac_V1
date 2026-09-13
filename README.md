# OAPW for macOS (Open Ambiophonics Processing Window)

OAPW for macOS ist eine native, eigenständige Desktop-Applikation zur Echtzeit-Audiosignalverarbeitung. Sie implementiert den **Recursive Ambiophonic Crosstalk Elimination (RACE)** Algorithmus, um eine extrem breite, holografische Stereobühne über nah beieinander stehende Lautsprecher zu erzeugen.

Entwickelt von Dr. Ulrich Thibaut mit Unterstützung durch Gemini.

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

## 🛠 Installation & Nutzung
1. Lade die fertige App `.zip`-Datei unter **Releases** herunter und entpacke sie.
2. **Wichtiger Hinweis (Apple Gatekeeper):** Da diese Open-Source-App nicht über ein kostenpflichtiges Apple-Zertifikat signiert ist, warnt macOS beim ersten Start möglicherweise vor einem "nicht verifizierten Entwickler". Mache einfach einen **Rechtsklick** auf die App und wähle **"Öffnen"** (oder bestätige es in den Systemeinstellungen unter *Datenschutz & Sicherheit*).
3. Stelle sicher, dass das Audio-Routing (BlackHole) eingerichtet ist (siehe oben).
4. Gib im Geometrie-Rechner der App deinen eigenen Hörabstand ein und klicke auf "Calculate & Apply".
5. Schalte den Ambiophonics-Effekt über den "Ambiophonics Active"-Button ein und aus, um den Unterschied zu hören!
6. Viel Spaß bei der Installation und viel Freude beim Experimentieren mit dem Sound-Effekt "Ambiophonie!"
