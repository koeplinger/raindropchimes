/* cli_wav_peak.c -- test helper: prints the smallest and the largest sample
 * of a 16-bit WAV file written by chimes (44 bytes of header, then samples,
 * lowest byte first).
 */
#include <stdio.h>

int main(int argc, char **argv)
{
    FILE *file;
    int low, high, smallest = 32767, largest = -32768;
    long samples = 0;

    if (argc != 2 || !(file = fopen(argv[1], "rb"))) {
        fprintf(stderr, "usage: cli_wav_peak FILE.wav\n");
        return 1;
    }
    if (fseek(file, 44L, SEEK_SET) != 0) return 1;
    while ((low = fgetc(file)) != EOF && (high = fgetc(file)) != EOF) {
        int value = low | (high << 8);
        if (value >= 32768) value -= 65536;
        if (value < smallest) smallest = value;
        if (value > largest) largest = value;
        samples++;
    }
    fclose(file);
    if (samples == 0) return 1;
    printf("%d %d\n", smallest, largest);
    return 0;
}
