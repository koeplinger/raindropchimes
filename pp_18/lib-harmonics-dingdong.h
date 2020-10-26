// ======================================================================
// Harmonics sounds toy
// (CC) 2011/2020 Jens Koeplinger, http://www.jenskoeplinger.com/P
// License: Creative Commons Attribution "Share Alike 3.0 Unported"
// http://creativecommons.org/licenses/by-sa/3.0
// See main file "harmonics.c" for what this means to you!
// ======================================================================
// lib-harmonics-dingdong.h
// ----------------------------------------------------------------------
// writes the dingdong music
//
// Examples:
/*

    writeDingDong();

*/
// ---------------------------------------------------------------------- 

void writeDingDong(void) {
// writes the dingdong music: into 11 equally time slots, play a 
// frequency. That frequency is played over on the same time slot, but
// with reduced amplitude. The frequency is repeated up to a (prime)
// number of times, then the slot is made available. A free slot is
// calculated by taking the frequency of the previous slot and chosing
// a frequency at random that relates to it through its 25 nearest
// overtone relations.

    double slotFrequency[11];       // the frequency assigned to each slot
    double slotPlay[11];            // 0 (or less: don't play slot; 1.: play full volume
    double slotCurRad[11];          // current radians for this frequency in the current bin
    double slotPos[11];             // 0.: sound on left, 1.: sound on right
    int    slotAmplitudeCnt[11];    // total number of repetitions for this frequency
    int    slotAmplitudeCur[11];    // ... repetition counter
    double density;                 // how many slots to fill, from 0. (10%) to 1. (100%)
    int    densityCnt;              // make sure at least one tone is playing at any time
    double vibrato;                 // how much vibrato to apply (e.g. 0.02==2% frequency)
    double vibratoBinsAvg;          // number of bins for a full vibrato cycle
    double vibratoBins[11];         // ... actual bins chosen for the slot, with some variability
    double vibratoBinCnt[11];       // ... counter for vibrato
    int    binsPerSlot;             // how many bins per slot
    int    binsPerDing;             // ... binsPerSlot * 11;
    int    curBin;                  // current bin counter (0..binsPerDing)
    int    curSlot;                 // current slot counter
    int    prevSlot;                // previous slot counter
    double prevFrequency;           // ... frequency thereof
    double newFrequency;            // ... new frequency going forward
    int    curSlotBin;              // bin counter relative to the beginning of the current slot
    int    songTime;                // current time in song, in number of 11-slots played
    int    totalSongTime;           // ... total time
    double aLeft;                   // left ear aggregate amplitude
    double aRight;                  // ... right ear
    double reverbLeft[40000][10];  // reverb tracker left ear (up to 10 reverbs);
    double reverbRight[40000][10]; // ... right ear
    int    reverbBins[10];          // number of bins to wait for reverb
    int    reverbBinCnt[10];        // ... current (counter)
    int    reverbType[10];          // 0: same position, 1: left/right inverted
    double reverbStrength[10];      // percentage of reverb (e.g. 0.3 = 30%)
    int    numReverbs;              // total number of reverbs in use

    int    i, j;                    // throwaway
    double dd;                      // throwaway
    int    seedTime;                // display the seed time

    printf("writeDingDong...\n");

    writeOutputWaveFileHeader();
    seedTime = time(NULL);
    // seedTime = 10;
    srand ( seedTime ); // initialize randomizer

    printf("...random seed: %d\n", seedTime);

    i = 0;

    while (i < 11) {
    //  initialize arrays

        slotFrequency[i] = 0.;
        slotPlay[i] = 0.;
        slotCurRad[i] = 0.;
        slotPos[i] = 0.;
        slotAmplitudeCnt[i] = 0;
        slotAmplitudeCur[i] = 0;
        vibratoBinCnt[i] = 0;
        vibratoBins[i] = 5000.; // dummy value, will be overwritten

        i++;
    }

    i = 0;

    while (i < 40000) {
    //  initialize all possible reverb bins

        j = 0;

        while (j < 10) {

            reverbLeft[i][j] = 0.;
            reverbRight[i][j] = 0.;

            j++;

        }

        i++;

    }

    setPrettyTone();  // defines the 25 nearest overtone relations for a base
    setPrettyPrime(); // defines an array of 11 pretty prime numbers

//  song parameters
    binsPerSlot       =    1700     ; // 4000 is about one cycle per second
    totalSongTime     =    800     ; // ... how many cycles
    vibrato           =      0.0010 ; // very slight vibrato
    vibratoBinsAvg    =  18000.     ; // about 3 vibrato cycles per second

//  reverbs
    numReverbs        =      7      ;
    reverbBins[0]     =   7000      ; // echo after about 1/4s
    reverbStrength[0] =      0.28   ; // 28%
    reverbType[0]     =      0      ; // same position
    reverbBinCnt[0]   =      0      ;
    reverbBins[1]     =   9500      ; 
    reverbStrength[1] =      0.09   ; 
    reverbType[1]     =      1      ; // swap left/right position
    reverbBinCnt[1]   =      0      ;
    reverbBins[2]     =  12000      ; 
    reverbStrength[2] =      0.17   ; 
    reverbType[2]     =      0      ; 
    reverbBinCnt[2]   =      0      ;
    reverbBins[3]     =  17000      ; 
    reverbStrength[3] =      0.07   ; 
    reverbType[3]     =      1      ; 
    reverbBinCnt[3]   =      0      ;
    reverbBins[4]     =  19000      ; 
    reverbStrength[4] =      0.11   ; 
    reverbType[4]     =      0      ; 
    reverbBinCnt[4]   =      0      ;
    reverbBins[5]     =  30000      ; 
    reverbStrength[5] =      0.05   ; 
    reverbType[5]     =      1      ; 
    reverbBinCnt[5]   =      0      ;
    reverbBins[6]     =  35000      ; 
    reverbStrength[6] =      0.07   ; 
    reverbType[6]     =      0      ; 
    reverbBinCnt[6]   =      0      ;

    songTime = 0;
    curBin   = 0;
    curSlot  = 0;
    binsPerDing = binsPerSlot * 11;

//  first sound
    slotFrequency[0]    = exp(((double) (rand() % 10000)) / 1000.) / 1.5 + 50. ; // start frequency
    slotPlay[0]         = .7                               ; // 70% volume
    slotPos[0]          = ((double) (rand() % 101)) / 100. ; // left-/right-position
    slotAmplitudeCnt[0] = prettyPrime[(rand() % 18)]       ; // start number of repetitions
    slotAmplitudeCur[0] = slotAmplitudeCnt[0]              ;
    densityCnt          = 1.                               ; // one note is playing; moving logarithmic avg
    vibratoBins[0]      = vibratoBinsAvg                   ; // set vibrato

    while (songTime < totalSongTime) {
    //  write the song, bin by bin

        if (DEBUG == 2) printf("[DEB] curBin = %d , curSlot = %d , songTime = %d , slotAmplCnt = %d\n",
                        curBin, curSlot, songTime, slotAmplitudeCnt[curSlot]);

        curBin++; // bin counter within the current slot curSlot

    //  first, write all amplitudes from all notes

        aLeft = 0.;
        aRight = 0.;
        i = 0;
        densityCnt = 0;

        while (i < 11) {
        //  loop through all slots for current frequency parts

            if ((slotFrequency[i] > 0) && (slotPlay[i] > 0.)) {
            //  nonempty, play: first add radians

            //  check whether this note is playing "loud enough"
                if (slotPlay[i] > 0.69) { densityCnt = densityCnt + 1; }

                slotCurRad[i] = slotCurRad[i] + ((2. * PI * slotFrequency[i]) / 44100.)
                                   * (1. + vibrato * (sin(2. * PI * vibratoBinCnt[i] / vibratoBins[i])));

                if (slotCurRad[i] > (2. * PI)) { slotCurRad[i] = slotCurRad[i] - (2. * PI); }

                vibratoBinCnt[i] = vibratoBinCnt[i] + 1.;
                if (vibratoBinCnt[i] > vibratoBins[i]) {
                    vibratoBinCnt[i] = vibratoBinCnt[i] - vibratoBins[i]; }

            //  then calculate current amplitude
                dd = 30000. * 20. / slotFrequency[i]; // maximum amplitude
                dd = dd * ((double) slotAmplitudeCur[i]) / ((double) slotAmplitudeCnt[i]); // tone down every repetition

                curSlotBin = curBin + ((curSlot - i) * binsPerSlot); // bin position relative to start of slot
                if (curSlotBin < 0) { curSlotBin = curSlotBin + binsPerDing; }

                if ((3 * curSlotBin) < binsPerSlot) {
                //  onset of the tone, ramp up

                    dd = dd * 3. * ((double) curSlotBin) / ((double) binsPerSlot);

                } else if ((3 * curSlotBin) < (2 * binsPerSlot)) {
                //  initial ramp down (to give a distinct "ding" effect)

                    dd = dd * (3. / 2.) * (((double) binsPerSlot) - ((double) curSlotBin)) / ((double) binsPerSlot);

                } else if (curSlotBin > (10 * binsPerSlot)) {
                //  last slot: soften out to avoid "click" artifact when compressing
                
                    dd = dd * 0.5 * 0.549 * (((double) binsPerDing) - ((double) curSlotBin)) / (((double) binsPerSlot));
                
                } else {
                //  tone has been played, fade out

                    dd = dd * 0.5 * sqrt(sqrt( (((double) binsPerDing) - ((double) curSlotBin)) / (((double) binsPerDing)) ));

                }

            //  total volume adjustment
                dd = dd * slotPlay[i];

            //  now put in the right place
                aLeft  = aLeft  + cos(slotCurRad[i]) * dd * (1. - slotPos[i]);
                aRight = aRight + cos(slotCurRad[i]) * dd * slotPos[i];

            }

            i++;

        }

    //  add reverbs
        i = 0;

        while (i < numReverbs) {

            switch (reverbType[i]) {

            case 0: // regular

                aLeft  = aLeft  + reverbLeft[  reverbBinCnt[i] ][i] * reverbStrength[i];
                aRight = aRight + reverbRight[ reverbBinCnt[i] ][i] * reverbStrength[i];
                break;

            case 1: // swapped left/right echo

                aLeft  = aLeft  + reverbRight[ reverbBinCnt[i] ][i] * reverbStrength[i];
                aRight = aRight + reverbLeft[  reverbBinCnt[i] ][i] * reverbStrength[i];
                break;

            }

            i++;

        }

        i = 0;

        while (i < numReverbs) {
        //  capture new amplitude for future reverb

            reverbLeft[  reverbBinCnt[i] ][i] = aLeft;
            reverbRight[ reverbBinCnt[i] ][i] = aRight;
            reverbBinCnt[i] = (reverbBinCnt[i] + 1) % reverbBins[i];

            i++;

        }

    //  write to file

        writeTwoByteInteger(fp, aLeft);
        writeTwoByteInteger(fp, aRight);

    //  now check for slot advance, new tone, and songTime advance

        if (curBin > binsPerSlot) {
        //  current slot has been fully played, advance to next slot

            curBin = 0;
            curSlot = curSlot + 1;

            if (curSlot > 10) {
            //  one whole ding was played, advance next sequence

                curSlot = 0;
                songTime = songTime + 1;

                printf("...(songTime %d, densityCnt %d, binsPerSlot %d, totalSongTime %d)\n",
                           songTime, densityCnt, binsPerSlot, totalSongTime);

            //  slowdown effect here:
                // binsPerSlot = binsPerSlot + 1;
                // binsPerDing = binsPerSlot * 11;

            }

        //  now advance the slot counter for the current slot
            slotAmplitudeCur[curSlot] = slotAmplitudeCur[curSlot] - 1;

            if (slotAmplitudeCur[curSlot] < 0) {
            //  slot has finished playing; get a new one

            	// ramp up and phase out note density gradually to not offend the ear
                density = 2.5 * sin(PI * ((double) songTime) / ((double) (totalSongTime - 31)));
                if (density > 1.) {
                	density = 1.;
                }

                if (songTime > (totalSongTime - 31)) {
                //  end of song; phase out, no more new notes

                    density = 0.;
                    densityCnt = -99;

                }

                density = density * density; // start low density, then sin^2 of total song

            //  find the previous slot

                prevSlot = curSlot - 1;
                if (prevSlot < 0) { prevSlot = 10; }

                prevFrequency = slotFrequency[prevSlot];

            //  determine frequency for the new sound (current slot)
                newFrequency = 0.;

                while ((newFrequency < 100.) || (newFrequency > 3000.)) {
                //  get an audible new frequency
                
                    newFrequency = slotFrequency[prevSlot] * prettyTone[(rand() % 25)];
                
                }

                slotFrequency[curSlot] = newFrequency;
                slotPos[curSlot] = ((double) (rand() % 101)) / 100.;

            //  determine how often it is to be played
                if ((densityCnt < 17) && (densityCnt > -1)) {
                //  play each note more often; if N tones are playing, then play
                //  a new note as often as up to the (N+2)th prime number

                    slotAmplitudeCnt[curSlot] = prettyPrime[(rand() % (densityCnt + 2))];
    
                } else {
                //  play each note as often as up to the 18th prime number
  
                    slotAmplitudeCnt[curSlot] = prettyPrime[(rand() % 18)];
    
                }

                slotAmplitudeCur[curSlot] = slotAmplitudeCnt[curSlot];

           //   based on density, determine whether it should be
           //   played at all (or whether just tracked silently)
                if (densityCnt < 0) {
                //  end of song phase out

                    slotPlay[curSlot] = 0.;

                } else if (densityCnt == 0) {
                //  nothing is playing right now; play one note at 70%

                    slotPlay[curSlot] = .7;
                    densityCnt = 1;

                } else {
                //  play it volume-density adjusted

                    slotPlay[curSlot] = (150. * (density + 0.05)) / (1. + ((double) (rand() % 100)));
                    if (slotPlay[curSlot] > 1.) { slotPlay[curSlot] = 1.; }

                }

            //  calculate vibrato (don't use the exact same vibrato
            //  frequency for all notes, instead vary the rate by
            //  some 10% or so)

                vibratoBins[curSlot] = vibratoBinsAvg * (1. + 0.001 * ((double) (rand() % 100)));
                vibratoBinCnt[curSlot] = 0.;

            } // ... end if slot finished playing

        } // ... end if curBin > binsPerSlot

    } // ... end songTime

    closeOutputWaveFile();

    printf("...writeDingDong done.\n");

    printf("Song specs:\n...random seed: %d\n", seedTime);
    printf("...binsPerSlot: %d\n", binsPerSlot);
    printf("...totalSongTime: %d\n", totalSongTime);

}

// ---------------------------------------------------------------------- 
