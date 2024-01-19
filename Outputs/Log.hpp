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
	WDFDC,
	SCSI,
};

constexpr bool is_enabled(Source source) {
#ifdef NDEBUG
	return false;
#endif

	// Allow for compile-time source-level enabling and disabling of different sources.
	switch(source) {
		default: return true;
	}
}

constexpr const char *prefix(Source source) {
	switch(source) {
		case Source::WDFDC:	return "WD FDC";
		case Source::SCSI:	return "SCSI";
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
