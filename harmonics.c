// ===========================================================================
// Harmonics sound toy
// (CC) 2011 Jens Koeplinger
// http://www.jenskoeplinger.com/
//
// License: Creative Commons Attribution "Share Alike 3.0 Unported"
// http://creativecommons.org/licenses/by-sa/3.0
//
// You are free:
// - to Share - to copy, distribute and transmit the work
// - to Remix - to adapt the work
//
// Under the following conditions:
// - Attribution. You must attribute the work in the manner specified by the
//   author or licensor (but not in any way that suggests that they endorse
//   you or your use of the work).
// - Share Alike. If you alter, transform, or build upon this work, you may
//   distribute the resulting work only under the same, similar or a compatible
//   license.
//
// For any reuse or distribution, you must make clear to others the license
// terms of this work. The best way to do this is with a link to:
//   http://creativecommons.org/licenses/by-sa/3.0
//
// Any of the above conditions can be waived if you get permission from the
// copyright holder.
//
// Nothing in this license impairs or restricts the author's moral rights.
// ===========================================================================

#define harmonicsVersion  "Harmonics sound toy version 4 March 2011\n"
#define licenseMain       "(CC) 2011 Jens Koeplinger - http://www.jenskoeplinger.com\n"
#define licenseDetail     "License: Creative Commons Attribution - Share Alike 3.0 Unported\n"
#define licenseReference  "http://creativecommons.org/licenses/by-sa/3.0\n\n"

#define outFileName       "out.wav"
#define DEBUG             0
#define PI                3.141592653589793116

#include <stdlib.h>
#include <stdio.h>
#include <math.h>
#include <time.h>

// ---------------------------------------------------------------------------
// global declarations - functions
// ---------------------------------------------------------------------------

// lib-harmonics-util.h
void   writeTwoByteInteger(FILE *, int);
void   writeFourByteInteger(FILE *, int);
void   writeOutputWaveFileHeader(void);
void   closeOutputWaveFile(void);
void   writeSweep(void);
void   setPrettyTone(void);
void   setPrettyPrime(void);

// lib-harmonics-dingdong.h
void   writeDingDong(void);

// ---------------------------------------------------------------------------
// global declarations - variables
// ---------------------------------------------------------------------------

// output wave file variables
FILE   *fp;                       // output wave file pointer
int    totalDataLength;           // e.g. 20 seconds 16 bit stereo at 44.1kHz = 3528000
int    thisTotalBin;              // absolute bin number counter (0 ... totalDataLength-1)
int    sampleFrequency;           // sampling frequency (e.g. 44100 for 44.1kHz)
double prettyTone[25];            // the 25 nearest overtone relations to a base tone
int    prettyPrime[11];           // an array of 11 pretty prime numbers

// ---------------------------------------------------------------------------
// main()
// ---------------------------------------------------------------------------

int main(int argc, char **argv) {

    int i; // throwaway

    printf(harmonicsVersion);
    printf(licenseMain);
    printf(licenseDetail);
    printf(licenseReference);

    // writeSweep();
    writeDingDong();

    printf(harmonicsVersion);
    printf(licenseMain);
    printf(licenseDetail);
    printf(licenseReference);

    printf("Good-bye\n");

}

// ---------------------------------------------------------------------------

#include "lib-harmonics-util.h"
#include "lib-harmonics-dingdong.h"

// ---------------------------------------------------------------------------
