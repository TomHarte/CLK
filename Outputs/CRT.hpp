//
//  CRT.hpp
//  Clock Signal
//
//  Created by Thomas Harte on 19/07/2015.
//  Copyright © 2015 Thomas Harte. All rights reserved.
//

#ifndef CRT_cpp
#define CRT_cpp

#include <stdint.h>
#include <string>

namespace Outputs {

class CRT {
	public:
		CRT(int cycles_per_line, int number_of_buffers, ...);
		~CRT();

		void output_sync(int number_of_cycles);
		void output_level(int number_of_cycles, std::string type);
		void output_data(int number_of_cycles, std::string type);

		struct CRTRun {
			struct Point {
				float x, y;
			} start_point, end_point;

			enum Type {
				Sync, Level, Data
			} type;

			std::string data_type;
			uint8_t *data;
		};

		class CRTDelegate {
			public:
				void crt_did_start_vertical_retrace_with_runs(CRTRun *runs, int number_of_runs);
		};
		void set_crt_delegate(CRTDelegate *);

		void allocate_write_area(int required_length);
		uint8_t *get_write_target_for_buffer(int buffer);

	private:
		CRTDelegate *_delegate;

		int _syncCapacitorChargeLevel;
		float _horizontalOffset, _verticalOffset;

		uint8_t **_buffers;
		int *_bufferSizes;
		int _numberOfBuffers;

		int _write_allocation_pointer, _write_target_pointer;
};

}


#endif /* CRT_cpp */
