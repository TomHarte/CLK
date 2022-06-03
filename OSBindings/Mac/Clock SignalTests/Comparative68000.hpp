//
//  Comparative68000.hpp
//  Clock SignalTests
//
//  Created by Thomas Harte on 29/04/2019.
//  Copyright © 2019 Thomas Harte. All rights reserved.
//

#ifndef Comparative68000_hpp
#define Comparative68000_hpp

#include <zlib.h>

#include "68000Mk2.hpp"

class ComparativeBusHandler: public CPU::MC68000Mk2::BusHandler {
	public:
		ComparativeBusHandler(const char *trace_name) {
			trace = gzopen(trace_name, "rt");
		}

		~ComparativeBusHandler() {
			gzclose(trace);
		}

		void will_perform(uint32_t address, uint16_t) {
			// Obtain the next line from the trace file.
			char correct_state[300] = "\n";
			gzgets(trace, correct_state, sizeof(correct_state));
			++line_count;

			// Generate state locally.
			const auto state = get_state().registers;
			char local_state[300];
			sprintf(local_state, "%04x: %02x %08x %08x %08x %08x %08x %08x %08x %08x %08x %08x %08x %08x %08x %08x %08x %08x\n",
				address,
				state.status,
				state.data[0], state.data[1], state.data[2], state.data[3], state.data[4], state.data[5], state.data[6], state.data[7],
				state.address[0], state.address[1], state.address[2], state.address[3], state.address[4], state.address[5], state.address[6],
				state.stack_pointer()
				);

			// Check that the two coincide.
			if(strcmp(correct_state, local_state)) {
				fprintf(stderr, "Diverges at line %d\n", line_count);
				fprintf(stderr, "Good: %s", correct_state);
				fprintf(stderr, "Bad:  %s", local_state);
				throw std::exception();
			}
		}

		virtual CPU::MC68000Mk2::State get_state() = 0;

	private:
		int line_count = 0;
		gzFile trace;
};

#endif /* Comparative68000_hpp */
