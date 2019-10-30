//
//  1770.hpp
//  Clock Signal
//
//  Created by Thomas Harte on 17/09/2016.
//  Copyright 2016 Thomas Harte. All rights reserved.
//

#ifndef _770_hpp
#define _770_hpp

#include "../../Storage/Disk/Controller/MFMDiskController.hpp"

namespace WD {

/*!
	Provides an emulation of various Western Digital drive controllers, including the
	WD1770, WD1772, FDC1773 and FDC1793.
*/
class WD1770: public Storage::Disk::MFMController {
	public:
		enum Personality {
			P1770,	// implies automatic motor-on management, with Type 2 commands offering a spin-up disable
			P1772,	// as per the 1770, with different stepping rates
			P1773,	// implements the side number-testing logic of the 1793; omits spin-up/loading logic
			P1793	// implies Type 2 commands use side number testing logic; spin-up/loading is by HLD and HLT
		};

		/*!
			Constructs an instance of the drive controller that behaves according to personality @c p.
			@param p The type of controller to emulate.
		*/
		WD1770(Personality p);

		/// Sets the value of the double-density input; when @c is_double_density is @c true, reads and writes double-density format data.
		using Storage::Disk::MFMController::set_is_double_density;

		/// Writes @c value to the register at @c address. Only the low two bits of the address are decoded.
		void set_register(int address, uint8_t value);

		/// Fetches the value of the register @c address. Only the low two bits of the address are decoded.
		uint8_t get_register(int address);

		/// Runs the controller for @c number_of_cycles cycles.
		void run_for(const Cycles cycles);

		enum Flag: uint8_t {
			NotReady		= 0x80,
			MotorOn			= 0x80,
			WriteProtect	= 0x40,
			RecordType		= 0x20,
			SpinUp			= 0x20,
			HeadLoaded		= 0x20,
			RecordNotFound	= 0x10,
			SeekError		= 0x10,
			CRCError		= 0x08,
			LostData		= 0x04,
			TrackZero		= 0x04,
			DataRequest		= 0x02,
			Index			= 0x02,
			Busy			= 0x01
		};

		/// @returns The current value of the IRQ line output.
		inline bool get_interrupt_request_line()		{	return status_.interrupt_request;	}

		/// @returns The current value of the DRQ line output.
		inline bool get_data_request_line()				{	return status_.data_request;		}

		class Delegate {
			public:
				virtual void wd1770_did_change_output(WD1770 *wd1770) = 0;
		};
		inline void set_delegate(Delegate *delegate)	{	delegate_ = delegate;			}

	protected:
		virtual void set_head_load_request(bool head_load);
		virtual void set_motor_on(bool motor_on);
		void set_head_loaded(bool head_loaded);

	private:
		Personality personality_;
		inline bool has_motor_on_line() { return (personality_ != P1793 ) && (personality_ != P1773); }
		inline bool has_head_load_line() { return (personality_ == P1793 ); }

		struct Status {
			bool write_protect = false;
			bool record_type = false;
			bool spin_up = false;
			bool record_not_found = false;
			bool crc_error = false;
			bool seek_error = false;
			bool lost_data = false;
			bool data_request = false;
			bool interrupt_request = false;
			bool busy = false;
			enum {
				One, Two, Three
			} type = One;
		} status_;
		uint8_t track_;
		uint8_t sector_;
		uint8_t data_;
		uint8_t command_;

		int index_hole_count_;
		int index_hole_count_target_ = -1;
		int distance_into_section_;

		int step_direction_;
		void update_status(std::function<void(Status &)> updater);

		// Events
		enum Event1770: int {
			Command			= (1 << 3),	// Indicates receipt of a new command.
			HeadLoad		= (1 << 4),	// Indicates the head has been loaded (1973 only).
			Timer			= (1 << 5),	// Indicates that the delay_time_-powered timer has timed out.
			IndexHoleTarget	= (1 << 6),	// Indicates that index_hole_count_ has reached index_hole_count_target_.
			ForceInterrupt	= (1 << 7)	// Indicates a forced interrupt.
		};
		void posit_event(int type);
		int interesting_event_mask_;
		int resume_point_ = 0;
		Cycles::IntType delay_time_ = 0;

		// ID buffer
		uint8_t header_[6];

		// 1793 head-loading logic
		bool head_is_loaded_ = false;

		// delegate
		Delegate *delegate_ = nullptr;
};

}

#endif /* _770_hpp */
