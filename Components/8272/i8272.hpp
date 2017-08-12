//
//  i8272.hpp
//  Clock Signal
//
//  Created by Thomas Harte on 05/08/2017.
//  Copyright © 2017 Thomas Harte. All rights reserved.
//

#ifndef i8272_hpp
#define i8272_hpp

#include "../../Storage/Disk/MFMDiskController.hpp"
#include "../../Storage/Disk/Drive.hpp"

#include <cstdint>
#include <vector>

namespace Intel {

class i8272: public Storage::Disk::MFMController {
	public:
		i8272(Cycles clock_rate, int clock_rate_multiplier, int revolutions_per_minute);

		void run_for(Cycles);

		void set_register(int address, uint8_t value);
		uint8_t get_register(int address);

		void set_disk(std::shared_ptr<Storage::Disk::Disk> disk, int drive);

	private:
		void posit_event(int type);
		uint8_t main_status_;

		std::vector<uint8_t> command_;
		std::vector<uint8_t> result_stack_;

		enum class Event8272: int {
			CommandByte	= (1 << 3),
			Timer = (1 << 4),
			ResultEmpty = (1 << 5),
		};

		int interesting_event_mask_;
		int resume_point_;
		int delay_time_;

		int step_rate_time_;
		int head_unload_time_;
		int head_load_time_;
		bool dma_mode_;

		struct Drive {
			uint8_t head_position;
			uint8_t status[3];

			// Seeking state.
			enum Phase {
				NotSeeking,
				Seeking,
				CompletedSeeking
			} phase;
			int step_rate_counter;
			int steps_taken;
			int target_head_position;	// either an actual number, or -1 to indicate to step until track zero

			// Head state.
			int head_unload_delay[2];
			bool head_is_loaded[2];

			std::shared_ptr<Storage::Disk::Drive> drive;

			Drive() :
				head_position(0), phase(NotSeeking),
				drive(new Storage::Disk::Drive),
				status{0, 0, 0}, head_is_loaded{false, false} {};
			void clear_status() {
				status[0] = status[1] = status[2] = 0;
			}
		} drives_[4];
		int head_timers_running_;

		uint8_t header_[6];
		int distance_into_section_;
		int index_hole_limit_;

		int active_drive_;
		int active_head_;

		uint8_t cylinder_, head_, sector_, size_;

		bool seek_is_satisfied(int drive);
};

}


#endif /* i8272_hpp */
