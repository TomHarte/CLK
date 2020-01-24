//
//  DiskController.hpp
//  Clock Signal
//
//  Created by Thomas Harte on 14/07/2016.
//  Copyright 2016 Thomas Harte. All rights reserved.
//

#ifndef Storage_Disk_Controller_hpp
#define Storage_Disk_Controller_hpp

#include "../Drive.hpp"
#include "../DPLL/DigitalPhaseLockedLoop.hpp"
#include "../Track/PCMSegment.hpp"

#include "../../../ClockReceiver/ClockReceiver.hpp"
#include "../../../ClockReceiver/ClockingHintSource.hpp"

namespace Storage {
namespace Disk {

/*!
	Provides the shell for emulating a disk controller: something that is connected to a disk drive and uses a
	phase locked loop ('PLL') to decode a bit stream from the surface of the disk.

	Partly abstract; it is expected that subclasses will provide methods to deal with receiving a newly-recognised
	bit from the PLL and with crossing the index hole.

	TODO: communication of head size and permissible stepping extents, appropriate simulation of gain.
*/
class Controller:
	public Drive::EventDelegate,
	public ClockingHint::Source,
	public ClockingHint::Observer {
	protected:
		/*!
			Constructs a @c Controller that will be run at @c clock_rate.
		*/
		Controller(Cycles clock_rate);

		/*!
			Communicates to the PLL the expected length of a bit as a fraction of a second.
		*/
		void set_expected_bit_length(Time bit_length);

		/*!
			Advances the drive by @c number_of_cycles cycles.
		*/
		void run_for(const Cycles cycles);

		/*!
			Sets the current drive. This drive is the one the PLL listens to.
		*/
		void set_drive(std::shared_ptr<Drive> drive);

		/*!
			Should be implemented by subclasses; communicates each bit that the PLL recognises.
		*/
		virtual void process_input_bit(int value) = 0;

		/*!
			Should be implemented by subclasses; communicates that the index hole has been reached.
		*/
		virtual void process_index_hole() = 0;

		/*!
			Should be implemented by subclasses if they implement writing; communicates that
			all bits supplied to write_bit have now been written.
		*/
		virtual void process_write_completed() override;

		/*!
			Puts the controller and the drive returned by get_drive() into write mode, supplying to
			the drive the current bit length.

			While the controller is in write mode it disconnects the PLL. So subclasses will not
			receive any calls to @c process_input_bit.

			@param clamp_to_index_hole If @c true then writing will automatically be truncated by
			the index hole. Writing will continue over the index hole otherwise.
		*/
		void begin_writing(bool clamp_to_index_hole);

		/*!
			Puts the drive returned by get_drive() out of write mode, and marks the controller
			as no longer being in write mode.
		*/
		void end_writing();

		/*!
			@returns @c true if the controller is in reading mode; @c false otherwise.
		*/
		bool is_reading();

		/*!
			Returns the connected drive or, if none is connected, an invented one. No guarantees are
			made about the lifetime or the exclusivity of the invented drive.
		*/
		Drive &get_drive();

		/*!
			As per ClockingHint::Source.
		*/
		ClockingHint::Preference preferred_clocking() override;

	private:
		Time bit_length_;
		Cycles::IntType clock_rate_multiplier_ = 1;
		Cycles::IntType clock_rate_ = 1;

		bool is_reading_ = true;

		DigitalPhaseLockedLoop<Controller> pll_;
		friend DigitalPhaseLockedLoop<Controller>;

		std::shared_ptr<Drive> drive_;

		std::shared_ptr<Drive> empty_drive_;

		// ClockingHint::Observer.
		void set_component_prefers_clocking(ClockingHint::Source *component, ClockingHint::Preference clocking) final;

		// for Drive::EventDelegate
		void process_event(const Drive::Event &event) final;
		void advance(const Cycles cycles) final;

		// to satisfy DigitalPhaseLockedLoop::Delegate
		void digital_phase_locked_loop_output_bit(int value);
};

}
}

#endif /* DiskDrive_hpp */
