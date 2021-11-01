# 2011 DingDong Doodle (v10)

Toy project to make beautiful sound from simple mathematical rules.

For algorithm details and samples see http://www.jenskoeplinger.com/RC/raindrops.html )


## Compile

Use a standard C compiler with math library. Example for the GNU C compiler:

`gcc harmonics.c -lm`

Then run `a.out`. A file `out.wav` will be created, which is a raw WAV file at
44.1kHz sampling rate (stereo). You should compress it, e.g. into OGG using
`ffmpeg`:

`ffmpeg -i out.wav -c:a libvorbis -qscale:a 10 out_1603376412_1700_800.ogg`

In this example I've put the random seed (1603376412) and song and cycle
duration (800 and 1700, respectively) into the file name.
Only this way will you be able to recreate the
song if you like it! The quality parameter `-qscale:a 10` means "best quality",
which results in unneccessarily large files. Omit this parameter for default
quality, or set to something smaller (e.g. 7 or 5) if the quality is still OK.


## Example: Change tune tempo and duration

In order to slow down or speed up the song, and make it longer or shorter,
edit the `lib-harmonics-dingdong.h` file. For example, this created
a nice slow song that's about 5 minutes long:

```text
...
  seedTime = 1635773128;
...
 binsPerSlot       =   11000     ; // 4000 is about one cycle per second
 totalSongTime     =     110     ; // ... how many cycles
...

```

Make these edits, then compile and run it. You'll get the same tune as I did.


## Variations

Just for fun, the following subfolders have tweaked variants:

* `pfib_18` use Fibunacci numbers instead of primes
* `pp_18` use 18 prime numbers (instead of 11)
* `psq_18` use squares (instead of primes)

