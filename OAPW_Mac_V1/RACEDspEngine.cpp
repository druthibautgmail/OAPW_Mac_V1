#include "RACEDspEngine.h"
#include <mutex>
#include <cmath>
#include <iostream>
#include <algorithm>
#include <complex>
#include <vector>
#include <chrono>

#ifndef M_PI
#define M_PI 3.14159265358979323846
#endif

// --- NEU: Lock-Free Record Queue Implementierung ---
LockFreeRecordQueue::LockFreeRecordQueue(size_t size) : buffer(size), head(0), tail(0) {}

void LockFreeRecordQueue::pushBlock(const float* left, const float* right, int numFrames) {
    size_t current_tail = tail.load(std::memory_order_relaxed);
    size_t next_tail = current_tail;
    size_t capacity = buffer.size();
    
    for(int i = 0; i < numFrames; ++i) {
        size_t next_plus_1 = (next_tail + 1) % capacity;
        size_t next_plus_2 = (next_tail + 2) % capacity;
        
        if (next_plus_2 == head.load(std::memory_order_acquire)) break; // Puffer voll (Notfall-Drop)
        
        buffer[next_tail] = left[i];
        buffer[next_plus_1] = right[i];
        next_tail = next_plus_2;
    }
    tail.store(next_tail, std::memory_order_release);
}

size_t LockFreeRecordQueue::popBlock(float* outBuffer, size_t maxSamples) {
    size_t current_head = head.load(std::memory_order_relaxed);
    size_t current_tail = tail.load(std::memory_order_acquire);
    size_t samplesRead = 0;
    size_t capacity = buffer.size();
    
    while(current_head != current_tail && samplesRead < maxSamples) {
        outBuffer[samplesRead++] = buffer[current_head];
        current_head = (current_head + 1) % capacity;
    }
    
    head.store(current_head, std::memory_order_release);
    return samplesRead;
}

void LockFreeRecordQueue::clear() {
    head.store(0);
    tail.store(0);
}

// --- FFT & Filter Implementierungen (Unverändert) ---
void computeFFT(std::vector<std::complex<float>>& data) {
    size_t n = data.size();
    if (n <= 1) return;
    for (size_t i = 1, j = 0; i < n; i++) {
        size_t bit = n >> 1;
        for (; j & bit; bit >>= 1) j ^= bit;
        j ^= bit;
        if (i < j) std::swap(data[i], data[j]);
    }
    for (size_t len = 2; len <= n; len <<= 1) {
        float angle = -2.0f * M_PI / len;
        std::complex<float> wlen(std::cos(angle), std::sin(angle));
        for (size_t i = 0; i < n; i += len) {
            std::complex<float> w(1, 0);
            for (size_t j = 0; j < len / 2; j++) {
                std::complex<float> u = data[i + j];
                std::complex<float> v = data[i + j + len / 2] * w;
                data[i + j] = u + v;
                data[i + j + len / 2] = u - v;
                w *= wlen;
            }
        }
    }
}

BiquadFilter::BiquadFilter() : z1(0.0f), z2(0.0f), sampleRate(48000) { setParameters(1000.0f, 0.707f, 0.0f, 48000); }
void BiquadFilter::setParameters(float frequency, float qFactor, float gain, int sr) {
    freq = frequency; q = qFactor; gainDb = gain; sampleRate = sr;
    float A = std::pow(10.0f, gainDb / 40.0f);
    float w0 = 2.0f * M_PI * freq / sampleRate;
    float alpha = std::sin(w0) / (2.0f * q);
    float a0 = 1.0f + alpha / A;
    b0 = (1.0f + alpha * A) / a0;
    b1 = (-2.0f * std::cos(w0)) / a0;
    b2 = (1.0f - alpha * A) / a0;
    a1 = (-2.0f * std::cos(w0)) / a0;
    a2 = (1.0f - alpha / A) / a0;
}
float BiquadFilter::process(float in) {
    float out = in * b0 + z1;
    z1 = in * b1 - out * a1 + z2;
    z2 = in * b2 - out * a2;
    return out;
}

IIRFilter::IIRFilter(const std::vector<double>& a, const std::vector<double>& b) : ac(a), bc(b) {
    order = static_cast<int>(ac.size()) - 1;
    x.resize(ac.size(), 0.0);
    y.resize(ac.size(), 0.0);
}
double IIRFilter::process(double input) {
    for (int n = order; n > 0; --n) { x[n] = x[n - 1]; y[n] = y[n - 1]; }
    x[0] = input;
    y[0] = ac[0] * x[0];
    for (int n = 1; n <= order; ++n) { y[0] += (ac[n] * x[n] - bc[n] * y[n]); }
    return y[0];
}

const std::vector<double> AC_LP = { 1.707930066E-11, 3.245067125E-10, 2.92056041249E-9, 1.654984233742E-8, 6.619936934968E-8, 1.9859810804904E-7, 4.6339558544777E-7, 8.6059180154585E-7, 1.29088770231878E-6, 1.5777516361674E-6, 1.5777516361674E-6, 1.29088770231878E-6, 8.6059180154585E-7, 4.6339558544777E-7, 1.9859810804904E-7, 6.619936934968E-8, 1.654984233742E-8, 2.92056041249E-9, 3.245067125E-10, 1.707930066E-11 };
const std::vector<double> BC_LP = { 1.0, -11.23829931452124, 60.88662121602015, -211.01791497183748, 523.7659541722651, -988.1021170488126, 1467.8446929796328, -1755.5644457376006, 1714.2251020332717, -1377.7264003960127, 914.6210265583827, -501.2759048988774, 225.7748760019649, -82.8018677820916, 24.357062095258804, -5.613879220355998, 0.9772797930828155, -0.12090187565166291, 0.009478452885452278, -3.541797659591719E-4 };
const std::vector<double> AC_HP = { 0.018501938638030006, -0.35153683412257014, 3.163831507103131, -17.928378540251078, 71.71351416100431, -215.14054248301292, 501.99459912703014, -932.2756840930559, 1398.413526139584, -1709.172087503936, 1709.172087503936, -1398.413526139584, 932.2756840930559, -501.99459912703014, 215.14054248301292, -71.71351416100431, 17.928378540251078, -3.163831507103131, 0.35153683412257014, -0.018501938638030006 };
const std::vector<double> BC_HP = { 1.0, -11.23829931456545, 60.8866212164491, -211.01791497385992, 523.7659541784101, -988.102117062272, 1467.8446930021598, -1755.5644457674098, 1714.2251020651054, -1377.7264004237784, 914.6210265782755, -501.27590491059107, 225.77487600760983, -82.80186778429722, 24.35706209594665, -5.613879220523086, 0.9772797931132484, -0.1209018756555653, 0.009478452885765523, -3.54179765970956E-4 };

// --- ENGINE ---
RACEDspEngine::RACEDspEngine(float initialDn, float initialAttenuation, float initialCenterP, bool initialFreqLimit)
    : raceHpL(AC_HP, BC_HP), raceHpR(AC_HP, BC_HP), raceLpL(AC_LP, BC_LP), raceLpR(AC_LP, BC_LP) {
    
    dspMutexPtr = new std::mutex();
    delayBufferL = nullptr; delayBufferR = nullptr; bufferSize = 0;
    volume = 1.0f; raceEnabled = true; filtersEnabled = true; eqEnabled = false; centerLevel = 0.0f;
    
    for (int i = 0; i < 3; ++i) {
        float f = (i == 0) ? 100.0f : (i == 1) ? 1000.0f : 5000.0f;
        eqL[i].setParameters(f, 0.707f, 0.0f, 48000); eqR[i].setParameters(f, 0.707f, 0.0f, 48000);
    }
    
    spectrumBuffer.resize(1024, 0.0f); spectrumIndex = 0;
    setParameters(initialDn, initialAttenuation, initialCenterP, initialFreqLimit);

    // Initialisiere die Lock-Free Queue mit Puffer für ~10 Sekunden Audio bei 48kHz Stereo
    recordQueue = new LockFreeRecordQueue(960000);
    isRecording = false;
    writerThread = nullptr;
}

RACEDspEngine::~RACEDspEngine() {
    stopRecording();
    delete recordQueue;
    delete static_cast<std::mutex*>(dspMutexPtr);
    if (delayBufferL) delete[] delayBufferL;
    if (delayBufferR) delete[] delayBufferR;
}

// --- RECORDING FUNKTIONEN (Die Rettung vor dem SSD-Bottleneck) ---
void RACEDspEngine::writeWavHeader(std::ofstream& file, uint32_t numFrames, uint32_t sampleRate) {
    uint16_t numChannels = 2;
    uint32_t byteRate = sampleRate * numChannels * 4;
    uint32_t dataSize = numFrames * numChannels * 4;
    
    file.seekp(0);
    file.write("RIFF", 4);
    uint32_t chunkSize = 50 + dataSize;
    file.write(reinterpret_cast<const char*>(&chunkSize), 4);
    file.write("WAVE", 4);
    
    // fmt chunk (IEEE Float 32-bit)
    file.write("fmt ", 4);
    uint32_t fmtSize = 18;
    file.write(reinterpret_cast<const char*>(&fmtSize), 4);
    uint16_t format = 3;
    file.write(reinterpret_cast<const char*>(&format), 2);
    file.write(reinterpret_cast<const char*>(&numChannels), 2);
    file.write(reinterpret_cast<const char*>(&sampleRate), 4);
    file.write(reinterpret_cast<const char*>(&byteRate), 4);
    uint16_t blockAlign = numChannels * 4;
    file.write(reinterpret_cast<const char*>(&blockAlign), 2);
    uint16_t bitDepth = 32;
    file.write(reinterpret_cast<const char*>(&bitDepth), 2);
    uint16_t cbSize = 0;
    file.write(reinterpret_cast<const char*>(&cbSize), 2);
    
    // fact chunk (Zwingend für Float WAVs)
    file.write("fact", 4);
    uint32_t factSize = 4;
    file.write(reinterpret_cast<const char*>(&factSize), 4);
    uint32_t sampleLength = numFrames * numChannels;
    file.write(reinterpret_cast<const char*>(&sampleLength), 4);
    
    // data chunk
    file.write("data", 4);
    file.write(reinterpret_cast<const char*>(&dataSize), 4);
}

void RACEDspEngine::diskWriterLoop() {
    std::ofstream wavFile(currentRecordPath, std::ios::binary);
    if (!wavFile.is_open()) return;
    
    // Dummy-Header schreiben
    writeWavHeader(wavFile, 0, recordSampleRate);
    
    uint32_t totalFramesWritten = 0;
    std::vector<float> localBuffer(16384); // 16k Lese-Puffer
    
    while(isRecording.load()) {
        size_t samplesPopped = recordQueue->popBlock(localBuffer.data(), localBuffer.size());
        
        if (samplesPopped > 0) {
            wavFile.write(reinterpret_cast<char*>(localBuffer.data()), samplesPopped * sizeof(float));
            totalFramesWritten += (samplesPopped / 2);
        } else {
            // Wenn die Queue leer ist, lege den Thread für 5ms schlafen (minimaler CPU Load)
            std::this_thread::sleep_for(std::chrono::milliseconds(5));
        }
    }
    
    // Queue final leeren, falls noch Reste vorhanden sind
    size_t samplesPopped;
    do {
        samplesPopped = recordQueue->popBlock(localBuffer.data(), localBuffer.size());
        if (samplesPopped > 0) {
            wavFile.write(reinterpret_cast<char*>(localBuffer.data()), samplesPopped * sizeof(float));
            totalFramesWritten += (samplesPopped / 2);
        }
    } while (samplesPopped > 0);
    
    // Header mit finaler Dateigröße patchen und schließen
    writeWavHeader(wavFile, totalFramesWritten, recordSampleRate);
    wavFile.close();
}

void RACEDspEngine::startRecording(const std::string& path, int sampleRate) {
    if (isRecording.load()) return;
    currentRecordPath = path;
    recordSampleRate = sampleRate;
    recordQueue->clear();
    isRecording.store(true);
    writerThread = new std::thread(&RACEDspEngine::diskWriterLoop, this);
}

void RACEDspEngine::stopRecording() {
    if (!isRecording.load()) return;
    isRecording.store(false);
    if (writerThread && writerThread->joinable()) {
        writerThread->join();
        delete writerThread;
        writerThread = nullptr;
    }
}

void RACEDspEngine::enqueueRawSamples(float* leftBuffer, float* rightBuffer, int numFrames) {
    if (isRecording.load()) {
        recordQueue->pushBlock(leftBuffer, rightBuffer, numFrames);
    }
}

// --- Parameter & Processing Setup (Unverändert) ---
void RACEDspEngine::setParameters(float newDn, float newAttenuation, float newCenterP, bool newFreqLimit) {
    std::lock_guard<std::mutex> lock(*static_cast<std::mutex*>(dspMutexPtr));
    dn = (newDn >= 0.0f) ? newDn : 0.0f;
    attenuation = std::pow(10.0f, newAttenuation / 20.0f);
    centerP = newCenterP; freqLimitRACE = newFreqLimit;
    delaySamples = dn * 0.048f;
    if (delaySamples < 1.0f) delaySamples = 1.0f;
    if (bufferSize == 0 || delayBufferL == nullptr || delayBufferR == nullptr) {
        bufferSize = 2205;
        if (delayBufferL) delete[] delayBufferL;
        if (delayBufferR) delete[] delayBufferR;
        delayBufferL = new float[bufferSize]; delayBufferR = new float[bufferSize];
        for(int i = 0; i < bufferSize; i++) { delayBufferL[i] = 0.0f; delayBufferR[i] = 0.0f; }
        writeIndex = 0;
    }
}
void RACEDspEngine::setVolume(float newVolume) { std::lock_guard<std::mutex> lock(*static_cast<std::mutex*>(dspMutexPtr)); volume = newVolume; }
void RACEDspEngine::setRaceEnabled(bool enabled) { std::lock_guard<std::mutex> lock(*static_cast<std::mutex*>(dspMutexPtr)); raceEnabled = enabled; }
void RACEDspEngine::setFiltersEnabled(bool enabled) { std::lock_guard<std::mutex> lock(*static_cast<std::mutex*>(dspMutexPtr)); filtersEnabled = enabled; }
void RACEDspEngine::setCenterLevel(float level) { std::lock_guard<std::mutex> lock(*static_cast<std::mutex*>(dspMutexPtr)); centerLevel = level; }
void RACEDspEngine::setEqEnabled(bool enabled) { std::lock_guard<std::mutex> lock(*static_cast<std::mutex*>(dspMutexPtr)); eqEnabled = enabled; }
void RACEDspEngine::setEqBand(int b, float f, float q, float g) {
    std::lock_guard<std::mutex> lock(*static_cast<std::mutex*>(dspMutexPtr));
    if(b >= 0 && b < 3) { eqL[b].setParameters(f, q, g, 48000); eqR[b].setParameters(f, q, g, 48000); }
}

inline float RACEDspEngine::hermiteInterpolation(float fraction, float y0, float y1, float y2, float y3) {
    float c0 = y1; float c1 = 0.5f * (y2 - y0);
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
            float bypassL = currentL * volume; float bypassR = currentR * volume;
            leftBuffer[i] = bypassL; rightBuffer[i] = bypassR;
            spectrumBuffer[spectrumIndex] = (bypassL + bypassR) * 0.5f;
            spectrumIndex = (spectrumIndex + 1) % 1024;
            continue;
        }

        float lpL = currentL, lpR = currentR;
        float hpL = 0.0f, hpR = 0.0f;
        
        if (filtersEnabled && freqLimitRACE) {
            hpL = static_cast<float>(raceHpL.process(currentL)); hpR = static_cast<float>(raceHpR.process(currentR));
            lpL = static_cast<float>(raceLpL.process(currentL)); lpR = static_cast<float>(raceLpR.process(currentR));
        }

        int delayInt = static_cast<int>(delaySamples);
        float fraction = delaySamples - delayInt;
        int idx0 = (writeIndex - delayInt + 1 + bufferSize) % bufferSize;
        int idx1 = (writeIndex - delayInt + bufferSize) % bufferSize;
        int idx2 = (writeIndex - delayInt - 1 + bufferSize) % bufferSize;
        int idx3 = (writeIndex - delayInt - 2 + bufferSize) % bufferSize;

        float delayedL = hermiteInterpolation(fraction, delayBufferL[idx0], delayBufferL[idx1], delayBufferL[idx2], delayBufferL[idx3]);
        float delayedR = hermiteInterpolation(fraction, delayBufferR[idx0], delayBufferR[idx1], delayBufferR[idx2], delayBufferR[idx3]);

        float crossTalkL = attenuation * (delayedR - centerP * delayedL) / (1.0f + centerP);
        float crossTalkR = attenuation * (delayedL - centerP * delayedR) / (1.0f + centerP);

        delayBufferL[writeIndex] = lpL - crossTalkL;
        delayBufferR[writeIndex] = lpR - crossTalkR;
        writeIndex = (writeIndex + 1) % bufferSize;

        float rawOutL = delayedL + hpL;
        float rawOutR = delayedR + hpR;
        float midSignal = (currentL + currentR) * 0.5f;
        rawOutL += (midSignal * centerLevel);
        rawOutR += (midSignal * centerLevel);

        if (eqEnabled) {
            for (int b = 0; b < 3; ++b) {
                rawOutL = eqL[b].process(rawOutL); rawOutR = eqR[b].process(rawOutR);
            }
        }

        float outL = rawOutL * volume;
        float outR = rawOutR * volume;

        if (outL < -1.0f) outL = -1.0f; if (outL > 1.0f)  outL = 1.0f;
        if (outR < -1.0f) outR = -1.0f; if (outR > 1.0f)  outR = 1.0f;

        spectrumBuffer[spectrumIndex] = (outL + outR) * 0.5f;
        spectrumIndex = (spectrumIndex + 1) % 1024;
        leftBuffer[i] = outL; rightBuffer[i] = outR;
    }
}

std::vector<float> RACEDspEngine::getSpectrumBands() {
    std::vector<float> audioData(1024, 0.0f);
    {
        std::lock_guard<std::mutex> lock(*static_cast<std::mutex*>(dspMutexPtr));
        for(int i = 0; i < 1024; i++) audioData[i] = spectrumBuffer[(spectrumIndex + i) % 1024];
    }
    std::vector<std::complex<float>> complexData(1024);
    for(int i = 0; i < 1024; i++) {
        float multiplier = 0.5f * (1.0f - std::cos(2.0f * M_PI * i / 1023.0f));
        complexData[i] = std::complex<float>(audioData[i] * multiplier, 0.0f);
    }
    computeFFT(complexData);
    std::vector<float> bands(32, 0.0f);
    float minFreq = 20.0f, maxFreq = 20000.0f;
    float logMin = std::log10(minFreq), logMax = std::log10(maxFreq);
    for (int b = 0; b < 32; b++) {
        float freqStart = std::pow(10.0f, logMin + (float)b / 32.0f * (logMax - logMin));
        float freqEnd   = std::pow(10.0f, logMin + (float)(b + 1) / 32.0f * (logMax - logMin));
        int binStart = (int)(freqStart * 1024.0f / 48000.0f);
        int binEnd   = (int)(freqEnd * 1024.0f / 48000.0f);
        if (binStart < 1) binStart = 1; if (binEnd > 511) binEnd = 511; if (binStart > binEnd) binEnd = binStart;
        float maxMag = 0.0f;
        for (int i = binStart; i <= binEnd; i++) {
            float mag = std::abs(complexData[i]);
            if (mag > maxMag) maxMag = mag;
        }
        bands[b] = maxMag;
    }
    for(int i = 0; i < 32; i++) {
        float db = 20.0f * std::log10(bands[i] + 1e-6f);
        float normalized = (db + 70.0f) / 70.0f;
        if (normalized < 0.0f) normalized = 0.0f; if (normalized > 1.0f) normalized = 1.0f;
        bands[i] = normalized;
    }
    return bands;
}

extern "C" {
    void* createRACEEngine(float initialDn, float initialAttenuation, float initialCenterP, bool initialFreqLimit) {
        return new RACEDspEngine(initialDn, initialAttenuation, initialCenterP, initialFreqLimit);
    }
    void destroyRACEEngine(void* enginePtr) {
        if (enginePtr) delete static_cast<RACEDspEngine*>(enginePtr);
    }
    void wrapper_setCenterLevel(void* enginePtr, float level) { if(enginePtr) static_cast<RACEDspEngine*>(enginePtr)->setCenterLevel(level); }
    void wrapper_setParameters(void* enginePtr, float dn, float attenuation, float centerP, bool freqLimit) {
        if(enginePtr) static_cast<RACEDspEngine*>(enginePtr)->setParameters(dn, attenuation, centerP, freqLimit);
    }
    void wrapper_setRaceEnabled(void* enginePtr, bool enabled) { if(enginePtr) static_cast<RACEDspEngine*>(enginePtr)->setRaceEnabled(enabled); }
    void wrapper_setVolume(void* enginePtr, float volume) { if(enginePtr) static_cast<RACEDspEngine*>(enginePtr)->setVolume(volume); }
    void wrapper_setEqEnabled(void* enginePtr, bool enabled) { if(enginePtr) static_cast<RACEDspEngine*>(enginePtr)->setEqEnabled(enabled); }
    void wrapper_setEqBand(void* enginePtr, int band, float freq, float q, float gain) {
        if(enginePtr) static_cast<RACEDspEngine*>(enginePtr)->setEqBand(band, freq, q, gain);
    }
    void wrapper_processSamples(void* enginePtr, float* leftBuffer, float* rightBuffer, int numFrames) {
        if(enginePtr) static_cast<RACEDspEngine*>(enginePtr)->processSamples(leftBuffer, rightBuffer, numFrames);
    }
    void wrapper_getSpectrumBands(void* enginePtr, float* outBuffer, int numBands) {
        if (!enginePtr || !outBuffer) return;
        std::vector<float> bands = static_cast<RACEDspEngine*>(enginePtr)->getSpectrumBands();
        int limit = (bands.size() < numBands) ? (int)bands.size() : numBands;
        for (int i = 0; i < limit; i++) outBuffer[i] = bands[i];
    }
    
    // NEU: Die Wrapper für die Aufnahme
    void wrapper_startRecording(void* enginePtr, const char* filePath, int sampleRate) {
        if(enginePtr && filePath) static_cast<RACEDspEngine*>(enginePtr)->startRecording(std::string(filePath), sampleRate);
    }
    void wrapper_stopRecording(void* enginePtr) {
        if(enginePtr) static_cast<RACEDspEngine*>(enginePtr)->stopRecording();
    }
    void wrapper_enqueueRawSamples(void* enginePtr, float* leftBuffer, float* rightBuffer, int numFrames) {
        if(enginePtr) static_cast<RACEDspEngine*>(enginePtr)->enqueueRawSamples(leftBuffer, rightBuffer, numFrames);
    }
}
