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
#include <cstring>
#include <string>

namespace Log {
// TODO: if adopting C++20, std::format would be a better model to apply below.
// But I prefer C files to C++ streams, so here it is for now.
// Also noting: Apple's std::format support seems to require macOS 13.3, so
// that'd be a bitter pill to swallow.

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
	CMOSRTC,
	DirectAccessDevice,
	Enterprise,
	Floppy,
	i8272,
	I2C,
	IDE,
	IntelligentKeyboard,	// Could probably be subsumed into 'Keyboard'?
	IWM,
	Keyboard,
	M50740,
	Macintosh,
	MasterSystem,
	MultiMachine,
	MFP68901,
	MOS6526,
	MSX,
	NCR5380,
	OpenGL,
	PCCompatible,
	PCPOST,
	PIC,
	PIT,
	Plus4,
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

enum class EnabledLevel {
	None,				// No logged statements are presented.
	Errors,				// The error stream is presented, but not the info stream.
	ErrorsAndInfo,		// All streams are presented.
};

constexpr EnabledLevel enabled_level(const Source source) {
#ifdef NDEBUG
	return EnabledLevel::None;
#endif

	// Allow for compile-time source-level enabling and disabling of different sources.
	switch(source) {
		default:
			return EnabledLevel::ErrorsAndInfo;

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
//		case Source::PCPOST:
			return EnabledLevel::None;

		case Source::Floppy:
//		case Source::Keyboard:
			return EnabledLevel::Errors;
	}
}

constexpr const char *prefix(const Source source) {
	switch(source) {
		case Source::ADBDevice:					return "ADB device";
		case Source::ADBGLU:					return "ADB GLU";
		case Source::Amiga:						return "Amiga";
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
		case Source::CMOSRTC:					return "CMOSRTC";
		case Source::DirectAccessDevice:		return "Direct Access Device";
		case Source::Enterprise:				return "Enterprise";
		case Source::Floppy:					return "Floppy";
		case Source::i8272:						return "i8272";
		case Source::I2C:						return "I2C";
		case Source::IDE:						return "IDE";
		case Source::IntelligentKeyboard:		return "IKYB";
		case Source::IWM:						return "IWM";
		case Source::Keyboard:					return "Keyboard";
		case Source::M50740:					return "M50740";
		case Source::Macintosh:					return "Macintosh";
		case Source::MasterSystem:				return "SMS";
		case Source::MOS6526:					return "MOS6526";
		case Source::MFP68901:					return "MFP68901";
		case Source::MultiMachine:				return "Multi-machine";
		case Source::MSX:						return "MSX";
		case Source::NCR5380:					return "5380";
		case Source::OpenGL:					return "OpenGL";
		case Source::Plus4:						return "Plus4";
		case Source::PCCompatible:				return "PC";
		case Source::PCPOST:					return "POST";
		case Source::PIC:						return "PIC";
		case Source::PIT:						return "PIT";
		case Source::PCMTrack:					return "PCM Track";
		case Source::SCSI:						return "SCSI";
		case Source::SCC:						return "SCC";
		case Source::SZX:						return "SZX";
		case Source::TapeUEF:					return "UEF";
		case Source::TMS9918:					return "TMS9918";
		case Source::TZX:						return "TZX";
		case Source::Vic20:						return "Vic20";
		case Source::WDFDC:						return "WD FDC";
	}

	return nullptr;
}

template <Source source, bool enabled>
struct LogLine;

struct RepeatAccumulator {
	std::string last;
	Source source;

	size_t count = 0;
	FILE *stream;
};

struct AccumulatingLog {
	inline static thread_local RepeatAccumulator accumulator_;
};

template <Source source>
struct LogLine<source, true>: private AccumulatingLog {
public:
	explicit LogLine(FILE *const stream) noexcept :
		stream_(stream) {}

	~LogLine() {
		if(output_ == accumulator_.last && source == accumulator_.source && stream_ == accumulator_.stream) {
			++accumulator_.count;
			return;
		}

		if(!accumulator_.last.empty()) {
			const char *const unadorned_prefix = prefix(accumulator_.source);
			std::string prefix;
			if(unadorned_prefix) {
				prefix = "[";
				prefix += unadorned_prefix;
				prefix += "] ";
			}

			if(accumulator_.count > 1) {
				fprintf(
					accumulator_.stream,
					"%s%s [* %zu]\n",
						prefix.c_str(),
						accumulator_.last.c_str(),
						accumulator_.count
				);
			} else {
				fprintf(
					accumulator_.stream,
					"%s%s\n",
						prefix.c_str(),
						accumulator_.last.c_str()
				);
			}
		}

		accumulator_.count = 1;
		accumulator_.last = output_;
		accumulator_.source = source;
		accumulator_.stream = stream_;
	}

	template <size_t size, typename... Args>
	auto &append(const char (&format)[size], Args... args) {
#pragma GCC diagnostic push
#pragma GCC diagnostic ignored "-Wformat"
		const auto append_size = std::snprintf(nullptr, 0, format, args...);
		const auto end = output_.size();
		output_.resize(output_.size() + size_t(append_size) + 1);
		std::snprintf(output_.data() + end, size_t(append_size) + 1, format, args...);
		output_.pop_back();
#pragma GCC diagnostic pop
		return *this;
	}

	template <size_t size, typename... Args>
	auto &append_if(const bool condition, const char (&format)[size], Args... args) {
		if(!condition) return *this;
		return append(format, args...);
	}

private:
	FILE *stream_;
	std::string output_;
};

template <Source source>
struct LogLine<source, false> {
	explicit LogLine(FILE *) noexcept {}

	template <size_t size, typename... Args>
	auto &append(const char (&)[size], Args...) { return *this; }

	template <size_t size, typename... Args>
	auto &append_if(bool, const char (&)[size], Args...) { return *this; }
};

template <Source source>
class Logger {
public:
	static constexpr bool InfoEnabled = enabled_level(source) == EnabledLevel::ErrorsAndInfo;
	static constexpr bool ErrorsEnabled = enabled_level(source) >= EnabledLevel::Errors;

	static auto info()	{	return LogLine<source, InfoEnabled>(stdout);	}
	static auto error()	{	return LogLine<source, ErrorsEnabled>(stderr);	}
};

}
