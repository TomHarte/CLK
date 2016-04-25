# CLK
An attempt to unify various bits of emulation; features:
* a best-in-class emulation of the Acorn Electron; and
* a mediocre emulation of the Atari 2600.

All code is motivated by a signals processing approach and a distinction between execution units and bus logic.

If simulating a TV, the CRT emulation uses your GPU to decode (and, as required by the emulated platform, possibly to encode) a genuine composite video stream — dot crawl et al are present and correct as a natural consequence, not as a post-processing effect. If a machine generates audio at 2Mhz then the source wave is modelled at 2Mhz and a standard windowing filter produces a 44Khz-or-so stream.

The hard emulation parts are C++11 and assume the OpenGL Core Profile; an Objective-C++/Swift UI binding for the Mac is present, making this completely native for Mac users. The intention is to provide additional OS bindings and ensure operation within ES 3.0 environments.
