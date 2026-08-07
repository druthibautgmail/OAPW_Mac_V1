#ifndef RACEDspEngine_h
#define RACEDspEngine_h

#include "Biquad.h"

class RACEDspEngine {
public: // WICHTIG: Alles hierunter ist von außen (und für unsere Wrapper) sichtbar!
    
    // Konstruktor muss exakt mit der .cpp übereinstimmen
    RACEDspEngine(float initialDn, float initialAttenuation, float initialCenterP, bool initialFreqLimit);
    ~RACEDspEngine();

    void updateFilters(double sampleRate);
    
    void setParameters(float newDn, float newAttenuation, float newCenterP, bool newFreqLimit);
    void setVolume(float newVolume);
    void setRaceEnabled(bool enabled);
    void setFiltersEnabled(bool enabled);
    void setCenterLevel(float level); // Unser neuer Parameter

    float getDn();
    float getAttenuation();
    float getCenterP();
    bool getFreqLimit();
    float getVolume();
    bool getRaceEnabled();
    bool getFiltersEnabled();

    void processSamples(float* leftBuffer, float* rightBuffer, int numFrames);

private: // WICHTIG: Interne Variablen, die niemand von außen anfassen darf
    void* dspMutexPtr;
    float* delayBufferL;
    float* delayBufferR;
    int bufferSize;
    int writeIndex = 0;

    float volume;
    bool raceEnabled;
    bool filtersEnabled;
    
    float dn;
    float attenuation;
    float centerP;
    bool freqLimitRACE;
    float delaySamples;
    
    float centerLevel = 0.0f; // Der interne Center-Wert

    Biquad hpL1, hpR1, hsL, hsR;

    inline float applyBandpassL(float in);
    inline float applyBandpassR(float in);
    inline float hermiteInterpolation(float fraction, float y0, float y1, float y2, float y3);
};

// --- C WRAPPERS (Die Brücke für Swift) ---
#ifdef __cplusplus
extern "C" {
#endif

// Diese beiden existieren vermutlich schon bei dir, wir deklarieren sie der Vollständigkeit halber
void* createRACEEngine(float initialDn, float initialAttenuation, float initialCenterP, bool initialFreqLimit);
void destroyRACEEngine(void* enginePtr);

// UNSERE NEUEN SICHEREN SETTER
void wrapper_setCenterLevel(void* enginePtr, float level);
void wrapper_setParameters(void* enginePtr, float dn, float attenuation, float centerP, bool freqLimit);
void wrapper_setRaceEnabled(void* enginePtr, bool enabled);
void wrapper_setVolume(void* enginePtr, float volume);
void wrapper_processSamples(void* enginePtr, float* leftBuffer, float* rightBuffer, int numFrames);
#ifdef __cplusplus
}
#endif

#endif /* RACEDspEngine_h */
