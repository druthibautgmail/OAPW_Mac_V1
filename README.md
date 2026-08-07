# OAPW Mac V1 (Ambiophonics Audio Engine)

A high-performance, ultra-low-latency macOS application executing the **Recursive Ambiophonic Crosstalk Elimination (RACE)** algorithm in real time. Designed for audiophile playback and synchronized multimedia viewing.

---

## 🚀 Key Features

* **Real-Time DSP Engine (C++17):** Core signal processing written in optimized C++ utilizing custom biquad filters, Hermite interpolation for sub-sample fractional delays, and thread-safe mutex locking.
* **Modern SwiftUI Dashboard (macOS):** Native, reactive interface built with Swift's `@Observable` macro framework and a robust Singleton architecture.
* **Microsecond-Precision Delays:** Fine-grained interaural time difference (ITD) tuning down to microsecond ($\mu s$) increments.
* **Dynamic A/V Sync (Movie vs. Music Mode):** 
  * *Music Mode (HQ):* Utilizes relaxed buffer sizes (1024 frames) to optimize CPU headroom for future high-order room correction filters.
  * *Movie Mode (Low Latency):* Forces CoreAudio hardware buffers down to 256 frames (~5.8 ms) to guarantee absolute lip-sync accuracy for video streaming (VLC, YouTube, Netflix).
* **Acoustic Make-Up Gain:** Independent decibel-scaled volume compensation to seamlessly offset the inherent energy loss caused by destructive phase cancellation in RACE processing.
* **Exclusive BlackHole Integration:** Seamlessly intercepts virtual system audio routes to process and route signals directly to high-end audio hardware (e.g., HDMI / DAC setups).

---

## 🛠️ System Architecture

1. **Audio Tap (`AVAudioEngine`):** Captures incoming stereo streams via the `BlackHole 2ch` virtual driver.
2. **C-Language Wrapper Bridge:** Safely encapsulates the C++ `RACEDspEngine` instance as an opaque pointer (`void*`), preventing memory corruption across the Swift/C++ boundary.
3. **Lock-Free Audio Callback:** Executes sample-by-sample crosstalk elimination inside a high-priority realtime audio thread without blocking the UI.

---

## 🎛️ Parameters & Controls

* **Attenuation:** Controls the cancellation factor in decibels ($\text{dB}$).
* **Speaker Delay:** Interaural time delay in microseconds ($\mu s$).
* **Center Level:** Direct mono-sum blending for center-stage imaging control.
* **Make-Up Gain:** Output volume compensation in decibels ($\text{dB}$).
* **Processing Mode:** Switch dynamically between high-quality music playback and low-latency video synchronization.

---

## 📄 License

Open-source project developed for high-end personal acoustic engineering and research.
