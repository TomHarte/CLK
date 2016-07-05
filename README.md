# CLK
An attempt to unify various bits of emulation; features:
* a best-in-class emulation of the Acorn Electron; and
* mediocre emulations of the Atari 2600 and Commodore Vic-20.

All code is motivated by a signals processing approach and a distinction between execution units and bus logic.

If simulating a TV, the CRT emulation uses your GPU to decode (and, as required by the emulated platform, possibly to encode) a genuine composite video stream — dot crawl et al are present and correct as a natural consequence, not as a post-processing effect. If a machine generates audio at 2Mhz then the source wave is modelled at 2Mhz and a standard windowing filter produces a 44Khz-or-so stream.

The hard emulation parts are C++11 and assume the OpenGL Core Profile; an Objective-C++/Swift UI binding for the Mac is present, making this completely native for Mac users. The intention is to provide additional OS bindings and ensure operation within ES 3.0 environments.

## TV Emulation

Composite decoding is currently performed purely by notch filtering; this produces worse separation than a comb but remained the predominant method for cheap TVs into the 1980s so is nevertheless not unrealistic. As I have yet to introduce any sort of inter-line processing, when running in PAL mode mine is the equivalent of a PAL-S. Since all signals propagate within a closed circuit there's no opportunity for a phase change that would produce Hanover bars but it's probably something that needs addressing regardless.

I've hesitated on a comb since it becomes complicated with machines — including the already-supported Atari 2600 — that use a not-strictly-conformant line length†, or, more substantially, with those that reset phase every line††.

All filtering is windowed finite impulse response, coefficients via Kaisser-Bessel, with up to 21 taps (adjacent samples are obtained from a single point via bilinear filtering where possible but that requires the adjacent coefficients to have the same sign; where that's not possible the number of taps may drop as the number of GLSL samples remains the same; it'll almost certainly be either 19 or 21 taps given the other operating conditions).

† per the documentation, its 228 cycles per line make each of its pixels exactly one NTSC colour clock long. There are 227.5 NTSC colour clocks per line so its hardware would appear to produce longer-than-specified lines (albeit still well within tolerable variation).

†† I suspect that a real TV will switch to a notch if adjacent colour bursts appear to keep resetting the colour oscillator, amongst other sanity checks, as analogue delay lines have a physically-fixed duration. I just need to do the same.
