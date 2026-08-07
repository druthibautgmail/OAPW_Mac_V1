//
//  Use this file to import your target's public headers that you would like to expose to Swift.
//

#include "RACEDspEngine.h"
#import "Biquad.h"

// Die Konstruktoren
void* createRACEEngine(float initialDn, float initialAttenuation, float initialCenterP, bool initialFreqLimit);
void destroyRACEEngine(void* enginePtr);

// Die sicheren Wrapper-Funktionen für Swift
void wrapper_setCenterLevel(void* enginePtr, float level);
void wrapper_setParameters(void* enginePtr, float dn, float attenuation, float centerP, bool freqLimit);
void wrapper_setRaceEnabled(void* enginePtr, bool enabled);
void wrapper_setVolume(void* enginePtr, float volume);
void wrapper_processSamples(void* enginePtr, float* leftBuffer, float* rightBuffer, int numFrames);
