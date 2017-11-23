Linux, BSD
==========

Prerequisites are SDL 2, ZLib and OpenGL (or Mesa), and SCons for the provided build script. OpenGL 3.2 or better is required at runtime.

Build:

	cd OSBindings/SDL
	scons

Optionally:

	cp clksignal /usr/bin

To launch:

	clksignal file

Setting up clksignal as the associated program for supported file types in your favoured filesystem browser is recommended; it has no file navigation abilities of its own.

Some emulated systems require the provision of original machine ROMs. These are not included and may be located in either /usr/local/share/CLK/ or /usr/share/CLK/. You will be prompted for them if they are found to be missing. The structure should mirror that under OSBindings in the source archive; see the readme.txt in each folder to determine the proper files and names ahead of time.

macOS
=====

There are no prerequisites beyond the normal system libraries; the macOS build is a native Cocoa application.

Build: open the Xcode project in OSBindings/Mac and press command+b.

Machine ROMs are intended to be built into the application bundle; populate the dummy folders below ROMImages before building.
