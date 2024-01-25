![Clock Signal Application Icon](READMEImages/Icon.png)
# Building Clock Signal

Clock Signal is available as [a macOS native application using
Metal](#macos-app) or as [a cross-platform command-line-driven SDL executable
using OpenGL](#sdl-app).

## macOS app

The macOS native application requires a Metal-capable Mac running macOS 10.13 or
later and has no prerequisites beyond the normal system libraries. It can be
built [using Xcode](#building-the-macos-app-using-xcode) or on the command line
[using `xcodebuild`](#building-the-macos-app-using-xcodebuild).

Machine ROMs are intended to be built into the application bundle; populate the
dummy folders below ROMImages before building.

The Xcode project is configured to sign the application using the developer's
certificate, but if you are not the developer then you will get a "No signing
certificate" error. To avoid this, you'll specify that you want to sign the
application to run locally.

### Building the macOS app using Xcode

Open the Clock Signal Xcode project in OSBindings/Mac.

To avoid signing errors, edit the project, select the Signing & Capabilities
tab, and change the Signing Certificate drop-down menu from "Development" to
"Sign to Run Locally".

To avoid crashes when running Clock Signal via Xcode on older Macs due to
"unrecognized selector sent to instance" errors, edit the scheme, and in the Run
section, scroll down to the Metal heading and uncheck the "API Validation"
checkbox.

To build, choose "Build" from Xcode's Product menu or press
<kbd>Command</kbd> + <kbd>B</kbd>.

To build and run, choose "Run" from the Product menu or press
<kbd>Command</kbd> + <kbd>R</kbd>.

To see the folder where the Clock Signal application was built, choose "Show
Build Folder in Finder" from the Product menu. Look in the "Products" folder for
a folder named after the configuration (e.g. "Debug" or "Release").

### Building the macOS app using `xcodebuild`

To build, change to the OSBindings/Mac directory in the Terminal, then run
`xcodebuild`, specifying `-` as the code sign identity to sign the application
to run locally to avoid signing errors:

	cd OSBindings/Mac
	xcodebuild CODE_SIGN_IDENTITY=-

`xcodebuild` will create a "build" folder in this directory which is where you
can find the Clock Signal application after it's compiled, in a directory named
after the configuration (e.g. "Debug" or "Release").

## SDL app

The SDL app can be built on Linux, BSD, macOS, and other Unix-like operating
systems. Prerequisites are SDL 2, ZLib and OpenGL (or Mesa). OpenGL 3.2 or
better is required at runtime. It can be built [using
SCons](#building-the-sdl-app-using-scons).

### Building the SDL app using SCons

To build, change to the OSBindings/SDL directory and run `scons`. You can add a
`-j` flag to build in parallel. For example, if you have 8 processor cores:

	cd OSBindings/SDL
	scons -j8

The `clksignal` executable will be created in this directory. You can run it
from here or install it by copying it where you want it, for example:

	cp clksignal /usr/local/bin

To start an emulator with a particular disk image `file`, if you've installed
`clksignal` to a directory in your `PATH`, run:

	clksignal file

Or if you're running it from the current directory:

	./clksignal file

Other options are availble. Run `clksignal` or `./clksignal` with no arguments
to learn more.

Setting up `clksignal` as the associated program for supported file types in
your favoured filesystem browser is recommended; it has no file navigation
abilities of its own.

Some emulated systems require the provision of original machine ROMs. These are
not included and may be located in either /usr/local/share/CLK/ or
/usr/share/CLK/. You will be prompted for them if they are found to be missing.
The structure should mirror that under OSBindings in the source archive; see the
readme.txt in each folder to determine the proper files and names ahead of time.
