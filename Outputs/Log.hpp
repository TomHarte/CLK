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
};

constexpr bool is_enabled(Source source) {
	// Allow for compile-time source-level enabling and disabling of different sources.
	switch(source) {
		default: return true;
	}
}

constexpr const char *prefix(Source source) {
	switch(source) {
		case Source::WDFDC:	return "[WD FDC]";
	}
}

#ifdef NDEBUG

class Logger {
	public:
		Logger(Source) {}

		struct LogLine {
			void append(const char *, ...) {}
		};
		LogLine info() {	return LogLine();	}
		LogLine error() {	return LogLine();	}
};

#else

#include <cstdio>
#include <cstdarg>

class Logger {
	public:
		Logger(Source source) : source_(source) {}

		struct LogLine {
			public:
				LogLine(Source source, FILE *stream) : source_(source), stream_(stream) {
					if(!is_enabled(source_)) return;

					const auto source_prefix = prefix(source);
					if(source_prefix) {
						fprintf(stream_, "%s ", source_prefix);
					}
				}

				~LogLine() {
					if(!is_enabled(source_)) return;
					fprintf(stream_, "\n");
				}

				void append(const char *format, ...) {
					if(!is_enabled(source_)) return;
					va_list args;
					va_start(args, format);
					vfprintf(stream_, format, args);
					va_end(args);
				}

			private:
				Source source_;
				FILE *stream_;
		};

		LogLine info() {	return LogLine(source_, stdout);	}
		LogLine error() {	return LogLine(source_, stderr);	}

	private:
		Source source_;
};

#endif

}
