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
		// fundamental creator-specified properties
		int _cycles_per_line;
		int _height_of_display;

		// properties directly derived from there
		int _hsync_error_window;			// the permitted window around the expected sync position in which a sync pulse will be recognised; calculated once at init

		// the run delegate, buffer and buffer pointer
		CRTDelegate *_delegate;
		std::vector<CRTRun> _all_runs;
		int _run_pointer;

		// the current scanning position
		float _horizontalOffset, _verticalOffset;

		// the content buffers
		uint8_t **_buffers;
		int *_bufferSizes;
		int _numberOfBuffers;

		// a pointer to the section of content buffer currently being
		// returned and to where the next section will begin
		int _write_allocation_pointer, _write_target_pointer;

		// a counter of horizontal syncs, to allow an automatic vertical
		// sync to be triggered if we appear to be exiting the display
		// (TODO: switch to evaluating _verticalOffset for this)
		int _hsync_counter;

		// outer elements of sync separation
		bool _is_receiving_sync;			// true if the CRT is currently receiving sync (i.e. this is for edge triggering of horizontal sync)
		bool _did_detect_hsync;				// true if horizontal sync was detected during this scanline (so, this affects flywheel adjustments)
		int _sync_capacitor_charge_level;	// this charges up during times of sync and depletes otherwise; needs to hit a required threshold to trigger a vertical sync
		int _vretrace_counter;				// a down-counter for time during a vertical retrace

		// components of the flywheel sync
		int _horizontal_counter;			// time run since the _start_ of the last horizontal sync
		int _expected_next_hsync;			// our current expection of when the next horizontal sync will be encountered (which implies current flywheel velocity)
		bool _is_in_hsync;					// true for the duration of a horizontal sync — used to determine beam running direction and speed

		// the outer entry point for dispatching output_sync, output_blank, output_level and output_data
		void advance_cycles(int number_of_cycles, bool hsync_requested, bool vsync_charging, CRTRun::Type type, const char *data_type);

		// the inner entry point that determines whether and when the next sync event will occur within
		// the current output window
		enum SyncEvent {
			None,
			StartHSync, EndHSync,
			StartVSync, EndVSync
		};
		SyncEvent advance_to_next_sync_event(bool hsync_is_requested, bool vsync_is_charging, int cycles_to_run_for, int *cycles_advanced);
};

}


#endif /* CRT_cpp */
