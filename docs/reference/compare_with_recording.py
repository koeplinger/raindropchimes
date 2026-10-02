#!/usr/bin/env python3
"""Compare a traced program's out.wav with a 2011 example recording.

    python3 compare_with_recording.py out.wav example.ogg

Needs ffmpeg (to decode the OGG) and numpy. Prints the number of frames of both
and, per channel, the correlation over the full length with no time shift. As a
control it also prints which shift within +-2000 samples lines the two up best
(over the first 2^20 frames); it should be 0. Exit status 0 means: same number
of frames, both correlations above 0.99, best shift 0.

out.wav must come from one of the 2011 programs: a 44-byte header, then 16-bit
stereo samples at 44.1 kHz. (The header's length fields are zero, so the file
is read as raw samples rather than through a WAV reader.)
"""
import subprocess
import sys

import numpy as np

HEADER_BYTES = 44
PASS_MARK = 0.99
MAX_SHIFT = 2000


def best_shift(a, b):
    """The shift of b against a, within +-MAX_SHIFT samples, with the largest
    cross-correlation."""
    n = min(len(a), 1 << 20)
    spectrum_a = np.fft.rfft(a[:n] - a[:n].mean(), 2 * n)
    spectrum_b = np.fft.rfft(b[:n] - b[:n].mean(), 2 * n)
    cross = np.fft.irfft(spectrum_a * np.conj(spectrum_b))
    shifts = np.concatenate([np.arange(0, MAX_SHIFT + 1), np.arange(-MAX_SHIFT, 0)])
    values = np.concatenate([cross[:MAX_SHIFT + 1], cross[-MAX_SHIFT:]])
    return int(shifts[np.argmax(values)])


def main():
    wav_path, ogg_path = sys.argv[1], sys.argv[2]

    wav = np.fromfile(wav_path, dtype='<i2', offset=HEADER_BYTES).reshape(-1, 2).astype(np.float64)
    decoded = subprocess.run(['ffmpeg', '-v', 'error', '-i', ogg_path, '-f', 's16le',
                              '-acodec', 'pcm_s16le', '-ac', '2', '-ar', '44100', '-'],
                             capture_output=True, check=True).stdout
    ogg = np.frombuffer(decoded, dtype='<i2').reshape(-1, 2).astype(np.float64)

    ok = len(wav) == len(ogg)
    print(f"frames: {len(wav)} (out.wav), {len(ogg)} (recording): {'same' if ok else 'DIFFERENT'}")
    n = min(len(wav), len(ogg))
    for channel, name in ((0, 'left'), (1, 'right')):
        a, b = wav[:n, channel], ogg[:n, channel]
        correlation = np.corrcoef(a, b)[0, 1]
        shift = best_shift(a, b)
        print(f"{name}: correlation {correlation:.5f}, best shift {shift}")
        ok = ok and correlation > PASS_MARK and shift == 0
    sys.exit(0 if ok else 1)


if __name__ == '__main__':
    main()
