#ifndef RACEDspEngine_h
#define RACEDspEngine_h

#ifdef __cplusplus
#include <vector>
#include <atomic>
#include <thread>
#include <string>
#include <fstream>

// --- V15.1 BiquadFilter (PEQ) ---
class BiquadFilter {
public:
    BiquadFilter();
    void setParameters(float frequency, float qFactor, float gain, int sr);
    float process(float in);
private:
    float freq, q, gainDb;
    int sampleRate;
    float b0, b1, b2, a1, a2;
    float z1, z2;
};

// --- V15.1 IIRFilter (RACE Frequenzbegrenzung) ---
class IIRFilter {
public:
    IIRFilter(const std::vector<double>& a, const std::vector<double>& b);
    double process(double input);
private:
    std::vector<double> ac, bc;
    std::vector<double> x, y;
    int order;
};

// --- NEU: Lock-Free Ringbuffer für Raw-Audio Export ---
class LockFreeRecordQueue {
public:
    LockFreeRecordQueue(size_t size);
    void pushBlock(const float* left, const float* right, int numFrames);
    size_t popBlock(float* outBuffer, size_t maxSamples);
    void clear();
private:
    std::vector<float> buffer;
    std::atomic<size_t> head;
    std::atomic<size_t> tail;
};

// --- RACEDspEngine ---
class RACEDspEngine {
public:
    RACEDspEngine(float initialDn, float initialAttenuation, float initialCenterP, bool initialFreqLimit);
    ~RACEDspEngine();

    void setParameters(float newDn, float newAttenuation, float newCenterP, bool newFreqLimit);
    void setVolume(float newVolumeDb);
    void setRaceEnabled(bool enabled);
    void setFiltersEnabled(bool enabled);
    void setCenterLevel(float level);
    
    // EQ Steuerung
    void setEqEnabled(bool enabled);
    void setEqBand(int b, float f, float q, float g);

    // Audio-Verarbeitung
    void processSamples(float* leftBuffer, float* rightBuffer, int numFrames);
    
    // Analyzer
    std::vector<float> getSpectrumBands();

    // NEU: Recording Steuerung
    void startRecording(const std::string& path, int sampleRate);
    void stopRecording();
    void enqueueRawSamples(float* leftBuffer, float* rightBuffer, int numFrames);

private:
    void* dspMutexPtr;
    float* delayBufferL;
    float* delayBufferR;
    int bufferSize;
    int writeIndex;

    float delaySamples;
    float dn;
    float attenuation;
    float centerP;
    float centerLevel;
    float volume;

    bool freqLimitRACE;
    bool raceEnabled;
    bool filtersEnabled;
    bool eqEnabled;

    IIRFilter raceHpL, raceHpR, raceLpL, raceLpR;
    BiquadFilter eqL[3], eqR[3];

    std::vector<float> spectrumBuffer;
    int spectrumIndex;

    // NEU: Variablen für den Hintergrund-Export
    LockFreeRecordQueue* recordQueue;
    std::thread* writerThread;
    std::atomic<bool> isRecording;
    std::string currentRecordPath;
    int recordSampleRate;
    void diskWriterLoop();
    void writeWavHeader(std::ofstream& file, uint32_t numFrames, uint32_t sampleRate);

    inline float hermiteInterpolation(float fraction, float y0, float y1, float y2, float y3);
};
#endif /* __cplusplus */

// --- C-WRAPPER FÜR DEN SWIFT BRIDGING-HEADER ---
#ifdef __cplusplus
extern "C" {
#endif

void* createRACEEngine(float initialDn, float initialAttenuation, float initialCenterP, bool initialFreqLimit);
void destroyRACEEngine(void* enginePtr);

void wrapper_setCenterLevel(void* enginePtr, float level);
void wrapper_setParameters(void* enginePtr, float dn, float attenuation, float centerP, bool freqLimit);
void wrapper_setRaceEnabled(void* enginePtr, bool enabled);
void wrapper_setVolume(void* enginePtr, float volume);

void wrapper_setEqEnabled(void* enginePtr, bool enabled);
void wrapper_setEqBand(void* enginePtr, int band, float freq, float q, float gain);

void wrapper_processSamples(void* enginePtr, float* leftBuffer, float* rightBuffer, int numFrames);
void wrapper_getSpectrumBands(void* enginePtr, float* outBuffer, int numBands);

// NEU: Recording C-Schnittstellen
void wrapper_startRecording(void* enginePtr, const char* filePath, int sampleRate);
void wrapper_stopRecording(void* enginePtr);
void wrapper_enqueueRawSamples(void* enginePtr, float* leftBuffer, float* rightBuffer, int numFrames);

#ifdef __cplusplus
}
#endif

#endif /* RACEDspEngine_h */
