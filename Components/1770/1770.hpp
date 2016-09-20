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

class WD1770 {
	public:
		WD1770();

		void set_drive(std::shared_ptr<Storage::Disk::Drive> drive);
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
			CRCError		= 0x08,
			LostData		= 0x04,
			TrackZero		= 0x04,
			DataRequest		= 0x02,
			Index			= 0x02,
			Busy			= 0x01
		};

	private:
		unsigned int cycles;

		enum class State {
			Waiting,
			BeginType1, BeginType2, BeginType3
		} state_;

		uint8_t status_;
		uint8_t track_;
		uint8_t sector_;
		uint8_t data_;
		uint8_t command_;
		bool has_command_;
};

}

#endif /* _770_hpp */
