//
//  Log.hpp
//  Clock Signal
//
//  Created by Thomas Harte on 18/06/2018.
//  Copyright © 2018 Thomas Harte. All rights reserved.
//

#pragma once

// TODO: if adopting C++20, std::format would be a better model to apply below.
// But I prefer C files to C++ streams, so here it is for now.

enum Category {
	Info,
	Error,
};

#ifdef NDEBUG

class Logger {
	public:
		Logger(const char *) {}

		struct LogLine {
			void append(const char *, ...) {}
		};
		template <Category category> LogLine line() {
			return LogLine();
		}
};

#else

#include <cstdio>
#include <cstdarg>

class Logger {
	public:
		Logger(const char *prefix) : prefix_(prefix) {}

		struct LogLine {
			public:
				LogLine(const char *prefix, FILE *stream) : stream_(stream) {
					if(prefix) {
						fprintf(stream_, "%s ", prefix);
					}
				}

				~LogLine() {
					fprintf(stream_, "\n");
				}

				void append(const char *format, ...) {
					va_list args;
					va_start(args, format);
					vfprintf(stream_, format, args);
					va_end(args);
				}

			private:
				FILE *stream_;
		};

		template <Category category> LogLine line() {
			return LogLine(prefix_, category == Info ? stdout : stderr);
		}

	private:
		const char *prefix_;
};

#endif
