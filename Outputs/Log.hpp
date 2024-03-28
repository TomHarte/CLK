//
//  Log.hpp
//  Clock Signal
//
//  Created by Thomas Harte on 18/06/2018.
//  Copyright © 2018 Thomas Harte. All rights reserved.
//

#pragma once

#include <cstdio>
#include <cstdarg>

namespace Log {
// TODO: if adopting C++20, std::format would be a better model to apply below.
// But I prefer C files to C++ streams, so here it is for now.

enum class Source {
	ADBDevice,
	ADBGLU,
	Amiga,
	AmigaDisk,
	AmigaCopper,
	AmigaChipset,
	AmigaBlitter,
	AppleIISCSICard,
	Archimedes,
	ARMIOC,
	ARMMEMC,
	ARMVIDC,
	AtariST,
	AtariSTDMAController,
	CommodoreStaticAnalyser,
	DirectAccessDevice,
	Enterprise,
	i8272,
	I2C,
	IntelligentKeyboard,
	IWM,
	M50740,
	Macintosh,
	MasterSystem,
	MultiMachine,
	MFP68901,
	MOS6526,
	MSX,
	NCR5380,
	OpenGL,
	PCMTrack,
	SCC,
	SCSI,
	SZX,
	TapeUEF,
	TMS9918,
	TZX,
	Vic20,
	WDFDC,
};

constexpr bool is_enabled(Source source) {
#ifdef NDEBUG
	return false;
#endif

	// Allow for compile-time source-level enabling and disabling of different sources.
	switch(source) {
		default: return true;

		// The following are all things I'm not actively working on.
		case Source::AmigaBlitter:
		case Source::AmigaChipset:
		case Source::AmigaCopper:
		case Source::AmigaDisk:
		case Source::DirectAccessDevice:
		case Source::IWM:
		case Source::MFP68901:
		case Source::NCR5380:
		case Source::SCC:
		case Source::SCSI:
		case Source::I2C:
			return false;
	}
}

constexpr const char *prefix(Source source) {
	switch(source) {
		default: return nullptr;

		case Source::ADBDevice:					return "ADB device";
		case Source::ADBGLU:					return "ADB GLU";
		case Source::AmigaBlitter:				return "Blitter";
		case Source::AmigaChipset:				return "Chipset";
		case Source::AmigaCopper:				return "Copper";
		case Source::AmigaDisk:					return "Disk";
		case Source::AppleIISCSICard:			return "SCSI card";
		case Source::Archimedes:				return "Archimedes";
		case Source::ARMIOC:					return "IOC";
		case Source::ARMMEMC:					return "MEMC";
		case Source::ARMVIDC:					return "VIDC";
		case Source::AtariST:					return "AtariST";
		case Source::AtariSTDMAController:		return "DMA";
		case Source::CommodoreStaticAnalyser:	return "Commodore Static Analyser";
		case Source::DirectAccessDevice:		return "Direct Access Device";
		case Source::Enterprise:				return "Enterprise";
		case Source::i8272:						return "i8272";
		case Source::I2C:						return "I2C";
		case Source::IntelligentKeyboard:		return "IKYB";
		case Source::IWM:						return "IWM";
		case Source::M50740:					return "M50740";
		case Source::Macintosh:					return "Macintosh";
		case Source::MasterSystem:				return "SMS";
		case Source::MOS6526:					return "MOS6526";
		case Source::MFP68901:					return "MFP68901";
		case Source::MultiMachine:				return "Multi-machine";
		case Source::MSX:						return "MSX";
		case Source::NCR5380:					return "5380";
		case Source::OpenGL:					return "OpenGL";
		case Source::PCMTrack:					return "PCM Track";
		case Source::SCSI:						return "SCSI";
		case Source::SCC:						return "SCC";
		case Source::SZX:						return "SZX";
		case Source::TapeUEF:					return "UEF";
		case Source::TZX:						return "TZX";
		case Source::Vic20:						return "Vic20";
		case Source::WDFDC:						return "WD FDC";
	}
}

template <Source source>
class Logger {
	public:
		static constexpr bool enabled = is_enabled(source);

		struct LogLine {
			public:
				LogLine(FILE *stream) : stream_(stream) {
					if constexpr (!enabled) return;

					const auto source_prefix = prefix(source);
					if(source_prefix) {
						fprintf(stream_, "[%s] ", source_prefix);
					}
				}

				~LogLine() {
					if constexpr (!enabled) return;
					fprintf(stream_, "\n");
				}

				void append(const char *format, ...) {
					if constexpr (!enabled) return;
					va_list args;
					va_start(args, format);
					vfprintf(stream_, format, args);
					va_end(args);
				}

			private:
				FILE *stream_;
		};

		LogLine info() {	return LogLine(stdout);	}
		LogLine error() {	return LogLine(stderr);	}
};

}
