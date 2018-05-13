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

/*!
	Provides emulation of the TMS9918a, TMS9928 and TMS9929. Likely in the future to be the
	vessel for emulation of sufficiently close derivatives, such as the Master System VDP.

	The TMS9918 and descendants are video display generators that own their own RAM, making it
	accessible through an implicitly-timed register interface, and (depending on model) can generate
	PAL and NTSC component and composite video.

	These chips have only one non-on-demand interaction with the outside world: an interrupt line.
	See get_time_until_interrupt and get_interrupt_line for asynchronous operation options.
*/
class TMS9918: public TMS9918Base {
	public:
		enum Personality {
			TMS9918A,	// includes the 9928 and 9929; set TV standard and output device as desired.
		};

		/*!
			Constructs an instance of the drive controller that behaves according to personality @c p.
			@param p The type of controller to emulate.
		*/
		TMS9918(Personality p);

		enum TVStandard {
			/*! i.e. 50Hz output at around 312.5 lines/field */
			PAL,
			/*! i.e. 60Hz output at around 262.5 lines/field */
			NTSC
		};

		/*! Sets the TV standard for this TMS, if that is hard-coded in hardware. */
		void set_tv_standard(TVStandard standard);

		/*! Provides the CRT this TMS is connected to. */
		Outputs::CRT::CRT *get_crt();

		/*!
			Runs the VCP for the number of cycles indicate; it is an implicit assumption of the code
			that the input clock rate is 3579545 Hz — the NTSC colour clock rate.
		*/
		void run_for(const HalfCycles cycles);

		/*! Sets a register value. */
		void set_register(int address, uint8_t value);

		/*! Gets a register value. */
		uint8_t get_register(int address);

		/*!
			Returns the amount of time until get_interrupt_line would next return true if
			there are no interceding calls to set_register or get_register.

			If get_interrupt_line is true now, returns zero. If get_interrupt_line would
			never return true, returns -1.
		*/
		HalfCycles get_time_until_interrupt();

		/*!
			@returns @c true if the interrupt line is currently active; @c false otherwise.
		*/
		bool get_interrupt_line();
};

};

#endif /* TMS9918_hpp */
