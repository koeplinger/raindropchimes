// ======================================================================
// Harmonics sounds toy
// (CC) 2011/2020 Jens Koeplinger, http://www.jenskoeplinger.com/P
// License: Creative Commons Attribution "Share Alike 3.0 Unported"
// http://creativecommons.org/licenses/by-sa/3.0
// See main file "harmonics.c" for what this means to you!
// ======================================================================
// lib-harmonics-util.h
// ----------------------------------------------------------------------
// utilities library
//
// Examples:
/*

    #define outFileName "out.wav"

    FILE   *fp;                       // output wave file pointer
    int    totalDataLength;

    totalDataLength = 3528000;        // 20 seconds 16 bit stereo at 44.1kHz
    writeOutputWaveFileHeader();

       ...

    writeTwoByteInteger(filePointer, 16);
    fprintf(filePointer, "data");
    writeFourByteInteger(filePointer, totalDataLength);

       ...

    closeOutputWaveFile();

*/
// ---------------------------------------------------------------------- 

void writeTwoByteInteger(FILE *fp, int byteToWrite) {
// only values from negative 0xFFFF to positive 0xFFFF are allowed

    if (byteToWrite < 0) {
    // add 0x10000 to negative values

        fputc((int) (byteToWrite + 0x10000)         & 0xFF, fp);
        fputc((int) ((byteToWrite + 0x10000) >> 8)  & 0xFF, fp);

    } else {

        fputc((int)  byteToWrite        & 0xFF, fp);
        fputc((int) (byteToWrite >> 8)  & 0xFF, fp);

    }

}

// ---------------------------------------------------------------------- 

void writeFourByteInteger(FILE *fp, int byteToWrite) {
// only positive values between 0 and 0xFFFFFFFF are allowed

    fputc((int)  byteToWrite        & 0xFF, fp);
    fputc((int) (byteToWrite >> 8)  & 0xFF, fp);
    fputc((int) (byteToWrite >> 16) & 0xFF, fp);
    fputc((int) (byteToWrite >> 24) & 0xFF, fp);

}

// ---------------------------------------------------------------------- 

void writeOutputWaveFileHeader() {
// opens the output wave file for writing, and
// writes header information; currently supported
// format: 16 bit stereo at 44.1kHz sampling rate.
//
// IN:  totalDataLength is the length (in words/bins) of the sample
//      fp is a file pointer; closed
// OUT: fp (file pointer; file will be open)

    printf("Open file for output: ");
    printf(outFileName);
    printf("\n");

    fp = fopen(outFileName, "wb");

    printf("... write header\n");

    fprintf(fp, "RIFF");                            // put text RIFF
    writeFourByteInteger(fp, totalDataLength * 4 + 36); // length of data + header
    fprintf(fp, "WAVEfmt ");                        // write the text "WAVEfmt "
    writeFourByteInteger(fp, 0x10);                 // write 0x10 (four byte)
    writeTwoByteInteger(fp, 1);                     // write 0x01 (two byte)
    writeTwoByteInteger(fp, 2);                     // 2 channels (=stereo)
    writeFourByteInteger(fp, 44100);                // sampled at 44.1kHz
    writeFourByteInteger(fp, 176400);               // bytes per second (=sampling rate * 4)
    writeTwoByteInteger(fp, 4);                     // bytes per sample: 4 (both channels)
    writeTwoByteInteger(fp, 16);                    // write bits per channel per sample: 16
    fprintf(fp, "data");                            // write the text "data"
    writeFourByteInteger(fp, totalDataLength * 4);  // total length of the data

    // Header is written,
    // file is open;
    // may the data come!

    printf("... done.\n");

}

// ---------------------------------------------------------------------- 

void closeOutputWaveFile() {

    printf("Close file\n");
    fclose(fp);
    printf("... done.\n");

}

// ---------------------------------------------------------------------- 

void setPrettyTone() {
// initializes the prettyTone[] array

    prettyTone[ 0] =  5. / 16.; // large 3rd up (small 6th down)
    prettyTone[ 1] =  5. /  8.;
    prettyTone[ 2] =  5. /  4.;
    prettyTone[ 3] =  5. /  2.;
    prettyTone[ 4] =  5. /  1.;

    prettyTone[ 5] =  3. /  8.; // 5th up (4th down)
    prettyTone[ 6] =  3. /  4.;
    prettyTone[ 7] =  3. /  2.;
    prettyTone[ 8] =  3. /  1.;
    prettyTone[ 9] =  6. /  1.;

    prettyTone[10] =  1. /  4.; // octave
    prettyTone[11] =  1. /  2.;
    prettyTone[12] =  1. /  1.;
    prettyTone[13] =  2. /  1.;
    prettyTone[14] =  4. /  1.;

    prettyTone[15] =  1. /  6.; // 5th down (4th up)
    prettyTone[16] =  1. /  3.;
    prettyTone[17] =  2. /  3.;
    prettyTone[18] =  4. /  3.;
    prettyTone[19] =  8. /  3.;

    prettyTone[20] =  1. /  5.; // large 3rd down (small 6th up)
    prettyTone[21] =  2. /  5.;
    prettyTone[22] =  4. /  5.;
    prettyTone[23] =  8. /  5.;
    prettyTone[24] = 16. /  5.;

}

// ---------------------------------------------------------------------- 

void setPrettyPrime() {
//  sets pretty prime numbers

    prettyPrime[ 0] =  1;
    prettyPrime[ 1] =  2;
    prettyPrime[ 2] =  3;
    prettyPrime[ 3] =  5;
    prettyPrime[ 4] =  7;
    prettyPrime[ 5] = 11;
    prettyPrime[ 6] = 13;
    prettyPrime[ 7] = 17;
    prettyPrime[ 8] = 19;
    prettyPrime[ 9] = 23;
    prettyPrime[10] = 29;
    prettyPrime[11] = 31;
    prettyPrime[12] = 37;
    prettyPrime[13] = 41;
    prettyPrime[14] = 43;
    prettyPrime[15] = 47;
    prettyPrime[16] = 53;
    prettyPrime[17] = 59;
}

// ---------------------------------------------------------------------- 

void writeSweep() {
// writes a 20Hz-20000kHz sweep in 20 seconds

    double thisFrequency;                       // 20Hz - 20000Hz
    int thisNumBins;                            // number of bins in a full wave of the current freq
    double thisNumBinsD;                        // ... double thereof
    double thisBinRad;                          // current bin position in radians (0..2pi)
    double thisBinRadRight;                     // ... offset for the right ear
    double thisAmplitude;
    int thisAmplitudeWaveInt;
    
    sampleFrequency = 44100;
    totalDataLength = sampleFrequency * 20;
    thisTotalBin = 0;
    thisBinRad = 0.;
    thisBinRadRight = 0.;

    printf("writeSweep...\n");

    writeOutputWaveFileHeader();

    while (thisTotalBin < totalDataLength) {
    //  do the sweep from 20Hz through roughly 20kHz

        thisFrequency = 20. + 0.9 * exp(10. * ((double) thisTotalBin) / ((double) totalDataLength));
        thisNumBinsD = ((double) sampleFrequency) / thisFrequency;
        thisNumBins = (int) thisNumBinsD;

        thisBinRad = thisBinRad + 2. * PI / thisNumBinsD;
        if (thisBinRad > (2. * PI)) { thisBinRad = thisBinRad - (2. * PI); }

        thisAmplitude = 30000. * 20. / thisFrequency;
        thisAmplitudeWaveInt = (int) (thisAmplitude * cos(thisBinRad));

        writeTwoByteInteger(fp, thisAmplitudeWaveInt); // left ear

    //  do something funny for the right ear: shift the phase back and forth a few times

        thisBinRadRight = cos(60. * ((double) thisTotalBin) / ((double) totalDataLength)) * 15.;
        thisAmplitudeWaveInt = (int) (thisAmplitude * cos(thisBinRad + thisBinRadRight));

        writeTwoByteInteger(fp, thisAmplitudeWaveInt); // right

        if (DEBUG == 1) printf("Bin %d / %d - f: %f - a: %d\n", thisTotalBin, totalDataLength, thisFrequency, thisAmplitudeWaveInt);

        thisTotalBin++;

    }

    closeOutputWaveFile();

    printf("...writeSweep done.\n");

}

// ----------------------------------------------------------------------
