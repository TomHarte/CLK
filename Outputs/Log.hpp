//
//  Log.hpp
//  Clock Signal
//
//  Created by Thomas Harte on 18/06/2018.
//  Copyright © 2018 Thomas Harte. All rights reserved.
//

#pragma once

namespace Log {
// TODO: if adopting C++20, std::format would be a better model to apply below.
// But I prefer C files to C++ streams, so here it is for now.

enum class Source {
	ADB,
	Amiga,
	AmigaDisk,
	AmigaCopper,
	AmigaChipset,
	AmigaBlitter,
	AtariST,
	CommodoreStaticAnalyser,
	DirectAccessDevice,
	Enterprise,
	i8272,
	IntelligentKeyboard,
	IWM,
	M50740,
	Macintosh,
	MasterSystem,
	MFP68901,
	MSX,
	NCR5380,
	OpenGL,
	SCC,
	SCSI,
	SZX,
	TapeUEF,
	TMS9918,
	TZX,
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
		case Source::NCR5380:
		case Source::SCC:	return false;
	}
}

constexpr const char *prefix(Source source) {
	switch(source) {
		default: return nullptr;

		case Source::CommodoreStaticAnalyser:	return "Commodore StaticAnalyser";
		case Source::i8272:						return "i8272";
		case Source::NCR5380:					return "5380";
		case Source::SCSI:						return "SCSI";
		case Source::SCC:						return "SCC";
		case Source::WDFDC:						return "WD FDC";
	}
}

#include <cstdio>
#include <cstdarg>

template <Source source>
class Logger {
	public:
		Logger() {}

		struct LogLine {
			public:
				LogLine(FILE *stream) : stream_(stream) {
					if constexpr (!is_enabled(source)) return;

					const auto source_prefix = prefix(source);
					if(source_prefix) {
						fprintf(stream_, "[%s] ", source_prefix);
					}
				}

				~LogLine() {
					if constexpr (!is_enabled(source)) return;
					fprintf(stream_, "\n");
				}

				void append(const char *format, ...) {
					if constexpr (!is_enabled(source)) return;
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
