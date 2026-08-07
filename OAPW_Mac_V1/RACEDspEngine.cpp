#include "RACEDspEngine.h"
#include <mutex> // Darf nur in der .cpp stehen!
#include <cmath>

RACEDspEngine::RACEDspEngine(float initialDn, float initialAttenuation, float initialCenterP, bool initialFreqLimit) {
    dspMutexPtr = new std::mutex();
    
    delayBufferL = nullptr;
    delayBufferR = nullptr;
    bufferSize = 0;

    volume = 1.0f;
    raceEnabled = true;
    filtersEnabled = true;

    updateFilters(44100.0);
    setParameters(initialDn, initialAttenuation, initialCenterP, initialFreqLimit);
}

RACEDspEngine::~RACEDspEngine() {
    delete static_cast<std::mutex*>(dspMutexPtr);
    if (delayBufferL) delete[] delayBufferL;
    if (delayBufferR) delete[] delayBufferR;
}

void RACEDspEngine::updateFilters(double sampleRate) {
    hpL1.setButterworth(Biquad::HIGHPASS, 150.0, sampleRate);
    hpR1.setButterworth(Biquad::HIGHPASS, 150.0, sampleRate);
    hsL.setHighShelf(2000.0, sampleRate, -12.0);
    hsR.setHighShelf(2000.0, sampleRate, -12.0);
}

void RACEDspEngine::setParameters(float newDn, float newAttenuation, float newCenterP, bool newFreqLimit) {
    std::lock_guard<std::mutex> lock(*static_cast<std::mutex*>(dspMutexPtr));
    
    // Wir sichern ab, dass der Delay-Wert nicht negativ wird
    dn = (newDn >= 0.0f) ? newDn : 0.0f;
    
    // Dezibel in linearen Multiplikator umrechnen
    attenuation = std::pow(10.0f, newAttenuation / 20.0f);
    
    centerP = newCenterP;
    freqLimitRACE = newFreqLimit;
    
    // Die NEUE Physik: Mikrosekunden in Samples umrechnen (bei 44.1 kHz)
    delaySamples = dn * 0.0441f;

    // Puffer nur beim allerersten Mal anlegen, um Audio-Aussetzer zu verhindern
    if (bufferSize == 0 || delayBufferL == nullptr || delayBufferR == nullptr) {
        bufferSize = 2205;
        if (delayBufferL) delete[] delayBufferL;
        if (delayBufferR) delete[] delayBufferR;
        
        delayBufferL = new float[bufferSize];
        delayBufferR = new float[bufferSize];
        
        for(int i = 0; i < bufferSize; i++) {
            delayBufferL[i] = 0.0f;
            delayBufferR[i] = 0.0f;
        }
        writeIndex = 0;
    }
}

void RACEDspEngine::setVolume(float newVolumeDb) {
    std::lock_guard<std::mutex> lock(*static_cast<std::mutex*>(dspMutexPtr));
    // Umrechnung von Dezibel (dB) in einen linearen Verstärkungsfaktor
    // 0 dB = 1.0 | +6 dB = ~2.0 (doppelte Spannung) | -6 dB = ~0.5
    volume = std::pow(10.0f, newVolumeDb / 20.0f);
}
void RACEDspEngine::setRaceEnabled(bool enabled) { std::lock_guard<std::mutex> lock(*static_cast<std::mutex*>(dspMutexPtr)); raceEnabled = enabled; }
void RACEDspEngine::setFiltersEnabled(bool enabled) { std::lock_guard<std::mutex> lock(*static_cast<std::mutex*>(dspMutexPtr)); filtersEnabled = enabled; }
void RACEDspEngine::setCenterLevel(float level) {
    std::lock_guard<std::mutex> lock(*static_cast<std::mutex*>(dspMutexPtr));
    centerLevel = level;
}
float RACEDspEngine::getDn() { std::lock_guard<std::mutex> lock(*static_cast<std::mutex*>(dspMutexPtr)); return dn; }
float RACEDspEngine::getAttenuation() { std::lock_guard<std::mutex> lock(*static_cast<std::mutex*>(dspMutexPtr)); return attenuation; }
float RACEDspEngine::getCenterP() { std::lock_guard<std::mutex> lock(*static_cast<std::mutex*>(dspMutexPtr)); return centerP; }
bool RACEDspEngine::getFreqLimit() { std::lock_guard<std::mutex> lock(*static_cast<std::mutex*>(dspMutexPtr)); return freqLimitRACE; }
float RACEDspEngine::getVolume() { std::lock_guard<std::mutex> lock(*static_cast<std::mutex*>(dspMutexPtr)); return volume; }
bool RACEDspEngine::getRaceEnabled() { std::lock_guard<std::mutex> lock(*static_cast<std::mutex*>(dspMutexPtr)); return raceEnabled; }
bool RACEDspEngine::getFiltersEnabled() { std::lock_guard<std::mutex> lock(*static_cast<std::mutex*>(dspMutexPtr)); return filtersEnabled; }

inline float RACEDspEngine::applyBandpassL(float in) {
    float x = static_cast<float>(hpL1.process(in));
    x = static_cast<float>(hsL.process(x));
    return x;
}

inline float RACEDspEngine::applyBandpassR(float in) {
    float x = static_cast<float>(hpR1.process(in));
    x = static_cast<float>(hsR.process(x));
    return x;
}

inline float RACEDspEngine::hermiteInterpolation(float fraction, float y0, float y1, float y2, float y3) {
    float c0 = y1;
    float c1 = 0.5f * (y2 - y0);
    float c2 = y0 - 2.5f * y1 + 2.0f * y2 - 0.5f * y3;
    float c3 = 0.5f * (y3 - y0) + 1.5f * (y1 - y2);
    return c0 + fraction * (c1 + fraction * (c2 + fraction * c3));
}

void RACEDspEngine::processSamples(float* leftBuffer, float* rightBuffer, int numFrames) {
    std::lock_guard<std::mutex> lock(*static_cast<std::mutex*>(dspMutexPtr));
    if (bufferSize == 0 || delayBufferL == nullptr || delayBufferR == nullptr) return;

    for (int i = 0; i < numFrames; i++) {
        float currentL = leftBuffer[i];
        float currentR = rightBuffer[i];

        if (!raceEnabled) {
            leftBuffer[i] = currentL * volume;
            rightBuffer[i] = currentR * volume;
            continue;
        }

        delayBufferL[writeIndex] = currentL;
        delayBufferR[writeIndex] = currentR;

        int delayInt = static_cast<int>(delaySamples);
        float fraction = delaySamples - delayInt;

        int idx0 = (writeIndex - delayInt + 1 + bufferSize) % bufferSize;
        int idx1 = (writeIndex - delayInt + bufferSize) % bufferSize;
        int idx2 = (writeIndex - delayInt - 1 + bufferSize) % bufferSize;
        int idx3 = (writeIndex - delayInt - 2 + bufferSize) % bufferSize;

        float delayedL = hermiteInterpolation(fraction, delayBufferL[idx0], delayBufferL[idx1], delayBufferL[idx2], delayBufferL[idx3]);
        float delayedR = hermiteInterpolation(fraction, delayBufferR[idx0], delayBufferR[idx1], delayBufferR[idx2], delayBufferR[idx3]);

        writeIndex = (writeIndex + 1) % bufferSize;

        float crossTalkL = attenuation * (delayedR - centerP * delayedL) / (1.0f + centerP);
        float crossTalkR = attenuation * (delayedL - centerP * delayedR) / (1.0f + centerP);

        float diffL = -crossTalkL;
        float diffR = -crossTalkR;

        if (filtersEnabled && freqLimitRACE) {
            diffL = applyBandpassL(diffL);
            diffR = applyBandpassR(diffR);
        }

        // 1. Die reine Mitte (Mono-Summe) des aktuellen rohen Inputs berechnen
        float midSignal = (currentL + currentR) * 0.5f;

        // 2. Das Center-Signal basierend auf dem Regler beimischen und globale Lautstärke anwenden
        float outL = (currentL + diffL + (midSignal * centerLevel)) * volume;
        float outR = (currentR + diffR + (midSignal * centerLevel)) * volume;

        // 3. Clipping verhindern
        if (outL < -1.0f) outL = -1.0f;
        if (outL > 1.0f)  outL = 1.0f;
        if (outR < -1.0f) outR = -1.0f;
        if (outR > 1.0f)  outR = 1.0f;

        leftBuffer[i] = outL;
        rightBuffer[i] = outR;
    }
}
// ... Ganz unten in der RACEDspEngine.cpp hinzugefügt: ...

extern "C" {
    
    // 1. DIE FEHLENDEN KONSTRUKTOREN FÜR SWIFT
    void* createRACEEngine(float initialDn, float initialAttenuation, float initialCenterP, bool initialFreqLimit) {
        return new RACEDspEngine(initialDn, initialAttenuation, initialCenterP, initialFreqLimit);
    }

    void destroyRACEEngine(void* enginePtr) {
        if (enginePtr) {
            delete static_cast<RACEDspEngine*>(enginePtr);
        }
    }

    // 2. DEINE BESTEHENDEN WRAPPER
    void wrapper_setCenterLevel(void* enginePtr, float level) {
        if(enginePtr) static_cast<RACEDspEngine*>(enginePtr)->setCenterLevel(level);
    }

    void wrapper_setParameters(void* enginePtr, float dn, float attenuation, float centerP, bool freqLimit) {
        if(enginePtr) static_cast<RACEDspEngine*>(enginePtr)->setParameters(dn, attenuation, centerP, freqLimit);
    }

    void wrapper_setRaceEnabled(void* enginePtr, bool enabled) {
        if(enginePtr) static_cast<RACEDspEngine*>(enginePtr)->setRaceEnabled(enabled);
    }

    void wrapper_setVolume(void* enginePtr, float volume) {
        if(enginePtr) static_cast<RACEDspEngine*>(enginePtr)->setVolume(volume);
    }

    void wrapper_processSamples(void* enginePtr, float* leftBuffer, float* rightBuffer, int numFrames) {
        if(enginePtr) static_cast<RACEDspEngine*>(enginePtr)->processSamples(leftBuffer, rightBuffer, numFrames);
    }
}
