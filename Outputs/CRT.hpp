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
#include <stdarg.h>
#include <string>
#include <vector>

#include "CRTFrame.h"

namespace Outputs {

class CRT;
struct CRTFrameBuilder {
	CRTFrame frame;

	CRTFrameBuilder(uint16_t width, uint16_t height, unsigned int number_of_buffers, va_list buffer_sizes);
	~CRTFrameBuilder();

	private:
		std::vector<uint8_t> _all_runs;

		void reset();
		void complete();

		uint8_t *get_next_run();
		friend CRT;

		void allocate_write_area(int required_length);
		uint8_t *get_write_target_for_buffer(int buffer);

		// a pointer to the section of content buffer currently being
		// returned and to where the next section will begin
		uint16_t _next_write_x_position, _next_write_y_position;
		uint16_t _write_x_position, _write_y_position;
		size_t _write_target_pointer;
};

static const int kCRTNumberOfFrames = 4;

class CRT {
	public:
		CRT(unsigned int cycles_per_line, unsigned int height_of_display, unsigned int number_of_buffers, ...);
		~CRT();

		void set_new_timing(unsigned int cycles_per_line, unsigned int height_of_display);

		void output_sync(unsigned int number_of_cycles);
		void output_blank(unsigned int number_of_cycles);
		void output_level(unsigned int number_of_cycles);
		void output_data(unsigned int number_of_cycles);

		class Delegate {
			public:
				virtual void crt_did_end_frame(CRT *crt, CRTFrame *frame, bool did_detect_vsync) = 0;
		};
		void set_delegate(Delegate *delegate);
		void return_frame();

		void allocate_write_area(int required_length);
		uint8_t *get_write_target_for_buffer(int buffer);

	private:
		// the incoming clock lengths will be multiplied by something to give at least 1000
		// sample points per line
		unsigned int _time_multiplier;

		// fundamental creator-specified properties
		unsigned int _cycles_per_line;
		unsigned int _height_of_display;

		// properties directly derived from there
		unsigned int _hsync_error_window;			// the permitted window around the expected sync position in which a sync pulse will be recognised; calculated once at init

		// the current scanning position
		struct Vector {
			uint32_t x, y;
		} _rasterPosition, _scanSpeed[4], _beamWidth[4];

		// the run delegate and the triple buffer
		CRTFrameBuilder *_frame_builders[kCRTNumberOfFrames];
		CRTFrameBuilder *_current_frame_builder;
		int _frames_with_delegate;
		int _frame_read_pointer;
		Delegate *_delegate;

		// outer elements of sync separation
		bool _is_receiving_sync;				// true if the CRT is currently receiving sync (i.e. this is for edge triggering of horizontal sync)
		bool _did_detect_hsync;					// true if horizontal sync was detected during this scanline (so, this affects flywheel adjustments)
		int _sync_capacitor_charge_level;		// this charges up during times of sync and depletes otherwise; needs to hit a required threshold to trigger a vertical sync
		int _sync_capacitor_charge_threshold;	// this charges up during times of sync and depletes otherwise; needs to hit a required threshold to trigger a vertical sync
		int _is_in_vsync;

		// components of the flywheel sync
		unsigned int _horizontal_counter;			// time run since the _start_ of the last horizontal sync
		unsigned int _expected_next_hsync;			// our current expection of when the next horizontal sync will be encountered (which implies current flywheel velocity)
		unsigned int _horizontal_retrace_time;
		bool _is_in_hsync;					// true for the duration of a horizontal sync — used to determine beam running direction and speed
		bool _did_detect_vsync;				// true if vertical sync was detected in the input stream rather than forced by emergency measure

		// the outer entry point for dispatching output_sync, output_blank, output_level and output_data
		enum Type {
			Sync, Level, Data, Blank
		} type;
		void advance_cycles(unsigned int number_of_cycles, bool hsync_requested, bool vsync_requested, bool vsync_charging, Type type);

		// the inner entry point that determines whether and when the next sync event will occur within
		// the current output window
		enum SyncEvent {
			None,
			StartHSync, EndHSync,
			StartVSync, EndVSync
		};
		SyncEvent get_next_vertical_sync_event(bool vsync_is_requested, unsigned int cycles_to_run_for, unsigned int *cycles_advanced);
		SyncEvent get_next_horizontal_sync_event(bool hsync_is_requested, unsigned int cycles_to_run_for, unsigned int *cycles_advanced);
};

}


#endif /* CRT_cpp */
