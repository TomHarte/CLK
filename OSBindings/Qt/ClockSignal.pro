QT       += core gui

greaterThan(QT_MAJOR_VERSION, 4): QT += widgets

CONFIG += c++17

# Permit multiple source files in different directories to have the same file name.
CONFIG += object_parallel_to_source

# Ensure ZLib is linked against.
INCLUDEPATH += $$[QT_INSTALL_PREFIX]/src/3rdparty/zlib
LIBS += -lz

# The following define makes your compiler emit warnings if you use
# any Qt feature that has been marked deprecated (the exact warnings
# depend on your compiler). Please consult the documentation of the
# deprecated API in order to know how to port your code away from it.
DEFINES += QT_DEPRECATED_WARNINGS

# You can also make your code fail to compile if it uses deprecated APIs.
# In order to do so, uncomment the following line.
# You can also select to disable deprecated APIs only up to a certain version of Qt.
#DEFINES += QT_DISABLE_DEPRECATED_BEFORE=0x060000    # disables all the APIs deprecated before Qt 6.0.0

SOURCES += \
    ../../Analyser/Dynamic/*.cpp \
    ../../Analyser/Dynamic/MultiMachine/*.cpp \
    ../../Analyser/Dynamic/MultiMachine/Implementation/*.cpp \
\
    ../../Analyser/Static/*.cpp \
    ../../Analyser/Static/Acorn/*.cpp \
    ../../Analyser/Static/AmstradCPC/*.cpp \
    ../../Analyser/Static/AppleII/*.cpp \
    ../../Analyser/Static/Atari2600/*.cpp \
    ../../Analyser/Static/AtariST/*.cpp \
    ../../Analyser/Static/Coleco/*.cpp \
    ../../Analyser/Static/Commodore/*.cpp \
    ../../Analyser/Static/Disassembler/*.cpp \
    ../../Analyser/Static/DiskII/*.cpp \
    ../../Analyser/Static/Macintosh/*.cpp \
    ../../Analyser/Static/MSX/*.cpp \
    ../../Analyser/Static/Oric/*.cpp \
    ../../Analyser/Static/Sega/*.cpp \
    ../../Analyser/Static/ZX8081/*.cpp \
\
    ../../Components/1770/*.cpp \
    ../../Components/5380/*.cpp \
    ../../Components/6522/Implementation/*.cpp \
    ../../Components/6560/*.cpp \
    ../../Components/6850/*.cpp \
    ../../Components/68901/*.cpp \
    ../../Components/8272/*.cpp \
    ../../Components/8530/*.cpp \
    ../../Components/9918/*.cpp \
    ../../Components/AudioToggle/*.cpp \
    ../../Components/AY38910/*.cpp \
    ../../Components/DiskII/*.cpp \
    ../../Components/KonamiSCC/*.cpp \
    ../../Components/OPx/*.cpp \
    ../../Components/SN76489/*.cpp \
    ../../Components/Serial/*.cpp \
\
    ../../Concurrency/*.cpp \
\
    ../../Inputs/*.cpp \
\
    ../../Machines/*.cpp \
    ../../Machines/AmstradCPC/*.cpp \
    ../../Machines/Apple/AppleII/*.cpp \
    ../../Machines/Apple/Macintosh/*.cpp \
    ../../Machines/Atari/2600/*.cpp \
    ../../Machines/Atari/ST/*.cpp \
    ../../Machines/ColecoVision/*.cpp \
    ../../Machines/Commodore/*.cpp \
    ../../Machines/Commodore/1540/Implementation/*.cpp \
    ../../Machines/Commodore/Vic-20/*.cpp \
    ../../Machines/Electron/*.cpp \
    ../../Machines/MasterSystem/*.cpp \
    ../../Machines/MSX/*.cpp \
    ../../Machines/Oric/*.cpp \
    ../../Machines/Utility/*.cpp \
    ../../Machines/ZX8081/*.cpp \
\
    ../../Outputs/*.cpp \
    ../../Outputs/CRT/*.cpp \
    ../../Outputs/OpenGL/*.cpp \
    ../../Outputs/OpenGL/Primitives/*.cpp \
\
    ../../Processors/6502/Implementation/*.cpp \
    ../../Processors/6502/State/*.cpp \
    ../../Processors/68000/Implementation/*.cpp \
    ../../Processors/68000/State/*.cpp \
    ../../Processors/Z80/Implementation/*.cpp \
    ../../Processors/Z80/State/*.cpp \
\
    ../../Reflection/*.cpp \
\
    ../../SignalProcessing/*.cpp \
\
    ../../Storage/*.cpp \
    ../../Storage/Cartridge/*.cpp \
    ../../Storage/Cartridge/Encodings/*.cpp \
    ../../Storage/Cartridge/Formats/*.cpp \
    ../../Storage/Data/*.cpp \
    ../../Storage/Disk/*.cpp \
    ../../Storage/Disk/Controller/*.cpp \
    ../../Storage/Disk/DiskImage/Formats/*.cpp \
    ../../Storage/Disk/DiskImage/Formats/Utility/*.cpp \
    ../../Storage/Disk/Encodings/*.cpp \
    ../../Storage/Disk/Encodings/AppleGCR/*.cpp \
    ../../Storage/Disk/Encodings/MFM/*.cpp \
    ../../Storage/Disk/Parsers/*.cpp \
    ../../Storage/Disk/Track/*.cpp \
    ../../Storage/MassStorage/*.cpp \
    ../../Storage/MassStorage/Encodings/*.cpp \
    ../../Storage/MassStorage/Formats/*.cpp \
    ../../Storage/MassStorage/SCSI/*.cpp \
    ../../Storage/Tape/*.cpp \
    ../../Storage/Tape/Formats/*.cpp \
    ../../Storage/Tape/Parsers/*.cpp \
\
    main.cpp \
    mainwindow.cpp

HEADERS += \
    ../../Activity/*.hpp \
    ../../Analyser/*.hpp \
    ../../Analyser/Dynamic/*.hpp \
    ../../Analyser/Dynamic/MultiMachine/*.hpp \
    ../../Analyser/Dynamic/MultiMachine/Implementation/*.hpp \
    ../../ClockReceiver/*.hpp \
    mainwindow.h

FORMS += \
    mainwindow.ui

TRANSLATIONS += \
    ClockSignal_en_GB.ts

# Default rules for deployment.
qnx: target.path = /tmp/$${TARGET}/bin
else: unix:!android: target.path = /opt/$${TARGET}/bin
!isEmpty(target.path): INSTALLS += target
