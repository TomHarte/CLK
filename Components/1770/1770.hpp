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

//		void set_disk(std::shared_ptr<Storage::Disk::Disk> disk);
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
			BeginType1, BeginType2, BeginType3,
			BeginType1PostSpin,
			WaitForSixIndexPulses,
			TestTrack, TestDirection, TestHead,
			TestVerify, VerifyTrack,
			StepDelay, TestPause, TestWrite
		} state_;

		union {
			struct {
				State next_state;
			} wait_six_index_pulses_;
			struct {
				int count;
			} step_delay_;
		};

		int index_hole_count_;

		uint8_t status_;
		uint8_t track_;
		uint8_t sector_;
		uint8_t data_;
		uint8_t command_;
		bool has_command_;

		void set_interrupt_request(bool interrupt_request) {}
		bool is_step_in_;
		uint8_t data_shift_register_;

		virtual void process_input_bit(int value, unsigned int cycles_since_index_hole);
		virtual void process_index_hole();
};

}

#endif /* _770_hpp */
