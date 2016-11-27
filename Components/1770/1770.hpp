//
//  1770.hpp
//  Clock Signal
//
//  Created by Thomas Harte on 17/09/2016.
//  Copyright © 2016 Thomas Harte. All rights reserved.
//

#ifndef _770_hpp
#define _770_hpp

#include "../../Storage/Disk/DiskController.hpp"

namespace WD {

class WD1770: public Storage::Disk::Controller {
	public:
		enum Personality {
			P1770,
			P1772,
			P1773
		};
		WD1770(Personality p);

		void set_is_double_density(bool is_double_density);
		void set_register(int address, uint8_t value);
		uint8_t get_register(int address);

		void run_for_cycles(unsigned int number_of_cycles);

		enum Flag: uint8_t {
			MotorOn			= 0x80,
			WriteProtect	= 0x40,
			RecordType		= 0x20,
			SpinUp			= 0x20,
			RecordNotFound	= 0x10,
			SeekError		= 0x10,
			CRCError		= 0x08,
			LostData		= 0x04,
			TrackZero		= 0x04,
			DataRequest		= 0x02,
			Index			= 0x02,
			Busy			= 0x01
		};

		inline bool get_interrupt_request_line()		{	return interrupt_request_line_;	}
		inline bool get_data_request_line()				{	return data_request_line_;		}
		class Delegate {
			public:
				virtual void wd1770_did_change_interrupt_request_status(WD1770 *wd1770) = 0;
				virtual void wd1770_did_change_data_request_status(WD1770 *wd1770) = 0;
		};
		inline void set_delegate(Delegate *delegate)	{	delegate_ = delegate;			}

	private:
		Personality personality_;

		uint8_t status_;	// TODO: to ensure correctness, this probably needs either to be a different value per command type,
							// or — probably more easily — to be an ordinary struct of flags with the value put together upon request.
		uint8_t track_;
		uint8_t sector_;
		uint8_t data_;
		uint8_t command_;

		int index_hole_count_;
		int index_hole_count_target_;
		int bits_since_token_;
		int distance_into_section_;
		bool is_awaiting_marker_value_;

		int step_direction_;
		void set_interrupt_request(bool interrupt_request);
		void set_data_request(bool interrupt_request);

		// Tokeniser
		bool is_reading_data_;
		bool is_double_density_;
		int shift_register_;
		struct Token {
			enum Type {
				Index, ID, Data, DeletedData, Byte
			} type;
			uint8_t byte_value;
		} latest_token_;

		// Events
		enum Event: int {
			Command			= (1 << 0),	// Indicates receipt of a new command.
			Token			= (1 << 1),	// Indicates recognition of a new token in the flux stream. Interrogate latest_token_ for details.
			IndexHole		= (1 << 2),	// Indicates the passing of a physical index hole.

			Timer			= (1 << 3),	// Indicates that the delay_time_-powered timer has timed out.
			IndexHoleTarget	= (1 << 4)	// Indicates that index_hole_count_ has reached index_hole_count_target_.
		};
		void posit_event(Event type);
		int interesting_event_mask_;
		int resume_point_;
		int delay_time_;

		// ID buffer
		uint8_t header_[6];

		// output line statuses
		bool interrupt_request_line_;
		bool data_request_line_;
		Delegate *delegate_;

		// Storage::Disk::Controller
		virtual void process_input_bit(int value, unsigned int cycles_since_index_hole);
		virtual void process_index_hole();
};

}

#endif /* _770_hpp */
