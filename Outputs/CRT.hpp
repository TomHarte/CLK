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
#include <vector>

namespace Outputs {

class CRT {
	public:
		CRT(int cycles_per_line, int height_of_display, int number_of_buffers, ...);
		~CRT();

		void output_sync(int number_of_cycles);
		void output_blank(int number_of_cycles);
		void output_level(int number_of_cycles, const char *type);
		void output_data(int number_of_cycles, const char *type);

		struct CRTRun {
			struct Point {
				float dst_x, dst_y;
				int src_x, src_y;
			} start_point, end_point;

			enum Type {
				Sync, Level, Data, Blank
			} type;

			const char *data_type;
		};

		class CRTDelegate {
			public:
				virtual void crt_did_start_vertical_retrace_with_runs(CRTRun *runs, int runs_to_draw) = 0;
		};
		void set_crt_delegate(CRTDelegate *delegate);

		void allocate_write_area(int required_length);
		uint8_t *get_write_target_for_buffer(int buffer);

	private:
		CRTDelegate *_delegate;
		std::vector<CRTRun> _all_runs;
		int _run_pointer;

		float _horizontalOffset, _verticalOffset;

		uint8_t **_buffers;
		int *_bufferSizes;
		int _numberOfBuffers;

		int _write_allocation_pointer, _write_target_pointer;

		void propose_hsync();
		void charge_vsync(int number_of_cycles);
		void drain_vsync(int number_of_cycles);
		void run_line_for_cycles(int number_of_cycles);
		void run_hline_for_cycles(int number_of_cycles);
		void do_hsync();
		void do_vsync();

		int _cycles_per_line;
		int _height_of_display;

		int _hsync_counter;

		enum SyncEvent {
			None,
			StartHSync, EndHSync,
			StartVSync, EndVSync
		};
		SyncEvent advance_to_next_sync_event(bool hsync_is_requested, bool vsync_is_charging, int cycles_to_run_for, int *cycles_advanced);
		bool _is_receiving_sync, _did_detect_hsync;
		int _sync_capacitor_charge_level, _vretrace_counter;
		int _horizontal_counter, _expected_next_hsync, _hsync_error_window;
		bool _is_in_hsync;

		void advance_cycles(int number_of_cycles, bool hsync_requested, bool vsync_charging, CRTRun::Type type, const char *data_type);
};

}


#endif /* CRT_cpp */
