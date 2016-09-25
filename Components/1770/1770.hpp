//
//  1770.hpp
//  Clock Signal
//
//  Created by Thomas Harte on 17/09/2016.
//  Copyright © 2016 Thomas Harte. All rights reserved.
//

#ifndef _770_hpp
#define _770_hpp

#include "../../Storage/Disk/DiskDrive.hpp"

namespace WD {

class WD1770: public Storage::Disk::Drive {
	public:
		WD1770();

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

	private:
		uint8_t status_;
		uint8_t track_;
		uint8_t sector_;
		uint8_t data_;
		uint8_t command_;

		int index_hole_count_;
		int bits_since_token_;
		int distance_into_section_;

		int step_direction_;
		void set_interrupt_request(bool interrupt_request) {}

		// Tokeniser
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
			Command		= (1 << 0),
			Token		= (1 << 1),
			IndexHole	= (1 << 2),
			Timer		= (1 << 3)
		};
		void posit_event(Event type);
		int interesting_event_mask_;
		int resume_point_;
		int delay_time_;

		// ID buffer
		uint8_t header[6];

		//
		virtual void process_input_bit(int value, unsigned int cycles_since_index_hole);
		virtual void process_index_hole();
};

}

#endif /* _770_hpp */
