QT += core gui multimedia widgets

# Be specific about C++17 but also try the vaguer C++1z for older
# versions of Qt.
CONFIG += c++17
CONFIG += c++1z

# Permit multiple source files in different directories to have the same file name.
CONFIG += object_parallel_to_source

# Link against ZLib.
INCLUDEPATH += $$[QT_INSTALL_PREFIX]/src/3rdparty/zlib
LIBS += -lz

# If targetting X11, link against that.
linux {
	QT += x11extras
	LIBS += -lX11
}

# Add flags (i) to identify that this is a Qt build; and
# (ii) to disable asserts in release builds.
DEFINES += TARGET_QT
DEFINES += IGNORE_APPLE
QMAKE_CXXFLAGS_RELEASE += -DNDEBUG

# Generate warnings for any use of APIs deprecated prior to Qt 6.0.0.
# Development was performed against Qt 5.14.
DEFINES += QT_DEPRECATED_WARNINGS
DEFINES += QT_DISABLE_DEPRECATED_BEFORE=0x060000

SRC = $$PWD/../..

SOURCES += \
	$$SRC/Analyser/Dynamic/*.cpp \
	$$SRC/Analyser/Dynamic/MultiMachine/*.cpp \
	$$SRC/Analyser/Dynamic/MultiMachine/Implementation/*.cpp \
\
	$$SRC/Analyser/Static/*.cpp \
	$$SRC/Analyser/Static/Acorn/*.cpp \
	$$SRC/Analyser/Static/Amiga/*.cpp \
	$$SRC/Analyser/Static/AmstradCPC/*.cpp \
	$$SRC/Analyser/Static/AppleII/*.cpp \
	$$SRC/Analyser/Static/AppleIIgs/*.cpp \
	$$SRC/Analyser/Static/Atari2600/*.cpp \
	$$SRC/Analyser/Static/AtariST/*.cpp \
	$$SRC/Analyser/Static/Coleco/*.cpp \
	$$SRC/Analyser/Static/Commodore/*.cpp \
	$$SRC/Analyser/Static/Disassembler/*.cpp \
	$$SRC/Analyser/Static/DiskII/*.cpp \
	$$SRC/Analyser/Static/Enterprise/*.cpp \
	$$SRC/Analyser/Static/Macintosh/*.cpp \
	$$SRC/Analyser/Static/MSX/*.cpp \
	$$SRC/Analyser/Static/Oric/*.cpp \
	$$SRC/Analyser/Static/Sega/*.cpp \
	$$SRC/Analyser/Static/ZX8081/*.cpp \
	$$SRC/Analyser/Static/ZXSpectrum/*.cpp \
\
	$$SRC/Components/1770/*.cpp \
	$$SRC/Components/5380/*.cpp \
	$$SRC/Components/6522/Implementation/*.cpp \
	$$SRC/Components/6560/*.cpp \
	$$SRC/Components/6850/*.cpp \
	$$SRC/Components/68901/*.cpp \
	$$SRC/Components/8272/*.cpp \
	$$SRC/Components/8530/*.cpp \
	$$SRC/Components/9918/*.cpp \
	$$SRC/Components/9918/Implementation/*.cpp \
	$$SRC/Components/AudioToggle/*.cpp \
	$$SRC/Components/AY38910/*.cpp \
	$$SRC/Components/DiskII/*.cpp \
	$$SRC/Components/KonamiSCC/*.cpp \
	$$SRC/Components/OPx/*.cpp \
	$$SRC/Components/SN76489/*.cpp \
	$$SRC/Components/Serial/*.cpp \
	$$SRC/Components/RP5C01/*.cpp \
\
	$$SRC/Inputs/*.cpp \
\
	$$SRC/InstructionSets/M50740/*.cpp \
	$$SRC/InstructionSets/M68k/*.cpp \
	$$SRC/InstructionSets/PowerPC/*.cpp \
	$$SRC/InstructionSets/x86/*.cpp \
\
	$$SRC/Machines/*.cpp \
	$$SRC/Machines/Amiga/*.cpp \
	$$SRC/Machines/AmstradCPC/*.cpp \
	$$SRC/Machines/Apple/ADB/*.cpp \
	$$SRC/Machines/Apple/AppleII/*.cpp \
	$$SRC/Machines/Apple/AppleIIgs/*.cpp \
	$$SRC/Machines/Apple/Macintosh/*.cpp \
	$$SRC/Machines/Atari/2600/*.cpp \
	$$SRC/Machines/Atari/ST/*.cpp \
	$$SRC/Machines/ColecoVision/*.cpp \
	$$SRC/Machines/Commodore/*.cpp \
	$$SRC/Machines/Commodore/1540/Implementation/*.cpp \
	$$SRC/Machines/Commodore/Vic-20/*.cpp \
	$$SRC/Machines/Electron/*.cpp \
	$$SRC/Machines/Enterprise/*.cpp \
	$$SRC/Machines/MasterSystem/*.cpp \
	$$SRC/Machines/MSX/*.cpp \
	$$SRC/Machines/Oric/*.cpp \
	$$SRC/Machines/Utility/*.cpp \
	$$SRC/Machines/Sinclair/Keyboard/*.cpp \
	$$SRC/Machines/Sinclair/ZX8081/*.cpp \
	$$SRC/Machines/Sinclair/ZXSpectrum/*.cpp \
\
	$$SRC/Outputs/*.cpp \
	$$SRC/Outputs/CRT/*.cpp \
	$$SRC/Outputs/ScanTargets/*.cpp \
	$$SRC/Outputs/OpenGL/*.cpp \
	$$SRC/Outputs/OpenGL/Primitives/*.cpp \
\
	$$SRC/Processors/6502/Implementation/*.cpp \
	$$SRC/Processors/6502/State/*.cpp \
	$$SRC/Processors/65816/Implementation/*.cpp \
	$$SRC/Processors/68000/Implementation/*.cpp \
	$$SRC/Processors/68000/State/*.cpp \
	$$SRC/Processors/Z80/Implementation/*.cpp \
	$$SRC/Processors/Z80/State/*.cpp \
\
	$$SRC/Reflection/*.cpp \
\
	$$SRC/SignalProcessing/*.cpp \
\
	$$SRC/Storage/*.cpp \
	$$SRC/Storage/Cartridge/*.cpp \
	$$SRC/Storage/Cartridge/Encodings/*.cpp \
	$$SRC/Storage/Cartridge/Formats/*.cpp \
	$$SRC/Storage/Data/*.cpp \
	$$SRC/Storage/Disk/*.cpp \
	$$SRC/Storage/Disk/Controller/*.cpp \
	$$SRC/Storage/Disk/DiskImage/Formats/*.cpp \
	$$SRC/Storage/Disk/DiskImage/Formats/Utility/*.cpp \
	$$SRC/Storage/Disk/Encodings/*.cpp \
	$$SRC/Storage/Disk/Encodings/AppleGCR/*.cpp \
	$$SRC/Storage/Disk/Encodings/MFM/*.cpp \
	$$SRC/Storage/Disk/Parsers/*.cpp \
	$$SRC/Storage/Disk/Track/*.cpp \
	$$SRC/Storage/MassStorage/*.cpp \
	$$SRC/Storage/MassStorage/Encodings/*.cpp \
	$$SRC/Storage/MassStorage/Formats/*.cpp \
	$$SRC/Storage/MassStorage/SCSI/*.cpp \
	$$SRC/Storage/State/*.cpp \
	$$SRC/Storage/Tape/*.cpp \
	$$SRC/Storage/Tape/Formats/*.cpp \
	$$SRC/Storage/Tape/Parsers/*.cpp \
\
	main.cpp \
	mainwindow.cpp \
	scantargetwidget.cpp \
	timer.cpp \
	keyboard.cpp

HEADERS += \
	$$SRC/Activity/*.hpp \
\
	$$SRC/Analyser/*.hpp \
	$$SRC/Analyser/Dynamic/*.hpp \
	$$SRC/Analyser/Dynamic/MultiMachine/*.hpp \
	$$SRC/Analyser/Dynamic/MultiMachine/Implementation/*.hpp \
\
	$$SRC/Analyser/Static/*.hpp \
	$$SRC/Analyser/Static/Acorn/*.hpp \
	$$SRC/Analyser/Static/Amiga/*.hpp \
	$$SRC/Analyser/Static/AmstradCPC/*.hpp \
	$$SRC/Analyser/Static/AppleII/*.hpp \
	$$SRC/Analyser/Static/AppleIIgs/*.hpp \
	$$SRC/Analyser/Static/Atari2600/*.hpp \
	$$SRC/Analyser/Static/AtariST/*.hpp \
	$$SRC/Analyser/Static/Coleco/*.hpp \
	$$SRC/Analyser/Static/Commodore/*.hpp \
	$$SRC/Analyser/Static/Disassembler/*.hpp \
	$$SRC/Analyser/Static/DiskII/*.hpp \
	$$SRC/Analyser/Static/Enterprise/*.hpp \
	$$SRC/Analyser/Static/Macintosh/*.hpp \
	$$SRC/Analyser/Static/MSX/*.hpp \
	$$SRC/Analyser/Static/Oric/*.hpp \
	$$SRC/Analyser/Static/Sega/*.hpp \
	$$SRC/Analyser/Static/ZX8081/*.hpp \
\
	$$SRC/ClockReceiver/*.hpp \
\
	$$SRC/Components/1770/*.hpp \
	$$SRC/Components/5380/*.hpp \
	$$SRC/Components/6522/*.hpp \
	$$SRC/Components/6522/Implementation/*.hpp \
	$$SRC/Components/6532/*.hpp \
	$$SRC/Components/6560/*.hpp \
	$$SRC/Components/6845/*.hpp \
	$$SRC/Components/6850/*.hpp \
	$$SRC/Components/8255/*.hpp \
	$$SRC/Components/8272/*.hpp \
	$$SRC/Components/8530/*.hpp \
	$$SRC/Components/9918/*.hpp \
	$$SRC/Components/9918/Implementation/*.hpp \
	$$SRC/Components/68901/*.hpp \
	$$SRC/Components/AudioToggle/*.hpp \
	$$SRC/Components/AY38910/*.hpp \
	$$SRC/Components/DiskII/*.hpp \
	$$SRC/Components/KonamiSCC/*.hpp \
	$$SRC/Components/OPx/*.hpp \
	$$SRC/Components/OPx/Implementation/*.hpp \
	$$SRC/Components/Serial/*.hpp \
	$$SRC/Components/SN76489/*.hpp \
	$$SRC/Components/RP5C01/*.hpp \
\
	$$SRC/Concurrency/*.hpp \
\
	$$SRC/Configurable/*.hpp \
\
	$$SRC/Inputs/*.hpp \
	$$SRC/Inputs/QuadratureMouse/*.hpp \
\
	$$SRC/InstructionSets/M50740/*.hpp \
	$$SRC/InstructionSets/M68k/*.hpp \
	$$SRC/InstructionSets/PowerPC/*.hpp \
	$$SRC/InstructionSets/x86/*.hpp \
\
	$$SRC/Machines/*.hpp \
	$$SRC/Machines/Amiga/*.hpp \
	$$SRC/Machines/AmstradCPC/*.hpp \
	$$SRC/Machines/Apple/ADB/*.hpp \
	$$SRC/Machines/Apple/AppleII/*.hpp \
	$$SRC/Machines/Apple/AppleIIgs/*.hpp \
	$$SRC/Machines/Apple/Macintosh/*.hpp \
	$$SRC/Machines/Atari/2600/*.hpp \
	$$SRC/Machines/Atari/ST/*.hpp \
	$$SRC/Machines/ColecoVision/*.hpp \
	$$SRC/Machines/Commodore/*.hpp \
	$$SRC/Machines/Commodore/1540/Implementation/*.hpp \
	$$SRC/Machines/Commodore/Vic-20/*.hpp \
	$$SRC/Machines/Electron/*.hpp \
	$$SRC/Machines/Enterprise/*.hpp \
	$$SRC/Machines/MasterSystem/*.hpp \
	$$SRC/Machines/MSX/*.hpp \
	$$SRC/Machines/Oric/*.hpp \
	$$SRC/Machines/Utility/*.hpp \
	$$SRC/Machines/Sinclair/Keyboard/*.hpp \
	$$SRC/Machines/Sinclair/ZX8081/*.hpp \
	$$SRC/Machines/Sinclair/ZXSpectrum/*.hpp \
\
	$$SRC/Numeric/*.hpp \
\
	$$SRC/Outputs/*.hpp \
	$$SRC/Outputs/CRT/*.hpp \
	$$SRC/Outputs/CRT/Internals/*.hpp \
	$$SRC/Outputs/ScanTargets/*.hpp \
	$$SRC/Outputs/OpenGL/*.hpp \
	$$SRC/Outputs/OpenGL/Primitives/*.hpp \
	$$SRC/Outputs/Speaker/*.hpp \
	$$SRC/Outputs/Speaker/Implementation/*.hpp \
\
	$$SRC/Processors/6502/*.hpp \
	$$SRC/Processors/6502/Implementation/*.hpp \
	$$SRC/Processors/6502/State/*.hpp \
	$$SRC/Processors/6502Esque/*.hpp \
	$$SRC/Processors/6502Esque/Implementation/*.hpp \
	$$SRC/Processors/65816/*.hpp \
	$$SRC/Processors/65816/Implementation/*.hpp \
	$$SRC/Processors/68000/*.hpp \
	$$SRC/Processors/68000/Implementation/*.hpp \
	$$SRC/Processors/68000/State/*.hpp \
	$$SRC/Processors/Z80/*.hpp \
	$$SRC/Processors/Z80/Implementation/*.hpp \
	$$SRC/Processors/Z80/State/*.hpp \
\
	$$SRC/Reflection/*.hpp \
\
	$$SRC/SignalProcessing/*.hpp \
\
	$$SRC/Storage/*.hpp \
	$$SRC/Storage/Cartridge/*.hpp \
	$$SRC/Storage/Cartridge/Encodings/*.hpp \
	$$SRC/Storage/Cartridge/Formats/*.hpp \
	$$SRC/Storage/Data/*.hpp \
	$$SRC/Storage/Disk/*.hpp \
	$$SRC/Storage/Disk/Controller/*.hpp \
	$$SRC/Storage/Disk/DiskImage/*.hpp \
	$$SRC/Storage/Disk/DiskImage/Formats/*.hpp \
	$$SRC/Storage/Disk/DiskImage/Formats/Utility/*.hpp \
	$$SRC/Storage/Disk/DPLL/*.hpp \
	$$SRC/Storage/Disk/Encodings/*.hpp \
	$$SRC/Storage/Disk/Encodings/AppleGCR/*.hpp \
	$$SRC/Storage/Disk/Encodings/MFM/*.hpp \
	$$SRC/Storage/Disk/Parsers/*.hpp \
	$$SRC/Storage/Disk/Track/*.hpp \
	$$SRC/Storage/MassStorage/*.hpp \
	$$SRC/Storage/MassStorage/Encodings/*.hpp \
	$$SRC/Storage/MassStorage/Formats/*.hpp \
	$$SRC/Storage/MassStorage/SCSI/*.hpp \
	$$SRC/Storage/State/*.hpp \
	$$SRC/Storage/Tape/*.hpp \
	$$SRC/Storage/Tape/Formats/*.hpp \
	$$SRC/Storage/Tape/Parsers/*.hpp \
\
	audiobuffer.h \
	functionthread.h \
	mainwindow.h \
	scantargetwidget.h \
	settings.h \
	keyboard.h \
	timer.h

FORMS += \
	mainwindow.ui

TRANSLATIONS += \
	clksignal_en_GB.ts

# Default rules for deployment.
qnx: target.path = /tmp/$${TARGET}/bin
else: unix:!android: target.path = /opt/$${TARGET}/bin
!isEmpty(target.path): INSTALLS += target
