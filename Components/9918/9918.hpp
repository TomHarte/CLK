//
//  9918.hpp
//  Clock Signal
//
//  Created by Thomas Harte on 25/11/2017.
//  Copyright 2017 Thomas Harte. All rights reserved.
//

#ifndef TMS9918_hpp
#define TMS9918_hpp

#include "../../Outputs/CRT/CRT.hpp"
#include "../../ClockReceiver/ClockReceiver.hpp"

#include "Implementation/9918Base.hpp"

#include <cstdint>

namespace TI {
namespace TMS {

/*!
	Provides emulation of the TMS9918a, TMS9928 and TMS9929. Likely in the future to be the
	vessel for emulation of sufficiently close derivatives, such as the Master System VDP.

	The TMS9918 and descendants are video display generators that own their own RAM, making it
	accessible through an implicitly-timed register interface, and (depending on model) can generate
	PAL and NTSC component and composite video.

	These chips have only one non-on-demand interaction with the outside world: an interrupt line.
	See get_time_until_interrupt and get_interrupt_line for asynchronous operation options.
*/
class TMS9918: public Base {
	public:
		/*!
			Constructs an instance of the drive controller that behaves according to personality @c p.
			@param p The type of controller to emulate.
		*/
		TMS9918(Personality p);

		/*! Sets the TV standard for this TMS, if that is hard-coded in hardware. */
		void set_tv_standard(TVStandard standard);

		/*! Sets the scan target this TMS will post content to. */
		void set_scan_target(Outputs::Display::ScanTarget *);

		/// Gets the current scan status.
		Outputs::Display::ScanStatus get_scaled_scan_status() const;

		/*! Sets the type of display the CRT will request. */
		void set_display_type(Outputs::Display::DisplayType);

		/*! Gets the type of display the CRT will request. */
		Outputs::Display::DisplayType get_display_type();

		/*!
			Runs the VCP for the number of cycles indicate; it is an implicit assumption of the code
			that the input clock rate is 3579545 Hz, the NTSC colour clock rate.
		*/
		void run_for(const HalfCycles cycles);

		/*! Sets a register value. */
		void write(int address, uint8_t value);

		/*! Gets a register value. */
		uint8_t read(int address);

		/*! Gets the current scan line; provided by the Master System only. */
		uint8_t get_current_line();

		/*! Gets the current latched horizontal counter; provided by the Master System only. */
		uint8_t get_latched_horizontal_counter();

		/*! Latches the current horizontal counter. */
		void latch_horizontal_counter();

		/*!
			Returns the amount of time until @c get_interrupt_line would next return true if
			there are no interceding calls to @c write or to @c read.

			If get_interrupt_line is true now, returns zero. If get_interrupt_line would
			never return true, returns -1.
		*/
		HalfCycles get_time_until_interrupt();

		/*!
			Returns the amount of time until the nominated line interrupt position is
			reached on line @c line. If no line interrupt position is defined for
			this VDP, returns the time until the 'beginning' of that line, whatever
			that may mean.

			@line is relative to the first pixel line of the display and may be negative.
		*/
		HalfCycles get_time_until_line(int line);

		/*!
			@returns @c true if the interrupt line is currently active; @c false otherwise.
		*/
		bool get_interrupt_line();
};

}
}

#endif /* TMS9918_hpp */
