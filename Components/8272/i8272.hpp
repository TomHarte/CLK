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

#include <cstdint>
#include <memory>
#include <vector>

namespace Intel {
namespace i8272 {

class BusHandler {
	public:
		virtual void set_dma_data_request(bool drq) {}
		virtual void set_interrupt(bool irq) {}
};

class i8272: public Storage::Disk::MFMController {
	public:
		i8272(BusHandler &bus_handler, Cycles clock_rate, int clock_rate_multiplier, int revolutions_per_minute);

		void run_for(Cycles);

		void set_data_input(uint8_t value);
		uint8_t get_data_output();

		void set_register(int address, uint8_t value);
		uint8_t get_register(int address);

		void set_dma_acknowledge(bool dack);
		void set_terminal_count(bool tc);

		void set_disk(std::shared_ptr<Storage::Disk::Disk> disk, int drive);

	private:
		// The bus handler, for interrupt and DMA-driven usage.
		BusHandler &bus_handler_;
		std::unique_ptr<BusHandler> allocated_bus_handler_;

		// Status registers.
		uint8_t main_status_;
		uint8_t status_[3];

		// A buffer for accumulating the incoming command, and one for accumulating the result.
		std::vector<uint8_t> command_;
		std::vector<uint8_t> result_stack_;
		uint8_t input_;
		bool has_input_;
		bool expects_input_;

		// Event stream: the 8272-specific events, plus the current event state.
		enum class Event8272: int {
			CommandByte	= (1 << 3),
			Timer = (1 << 4),
			ResultEmpty = (1 << 5),
		};
		void posit_event(int type);
		int interesting_event_mask_;
		int resume_point_;
		bool is_access_command_;

		// The counter used for ::Timer events.
		int delay_time_;

		// The connected drives.
		struct Drive {
			uint8_t head_position;

			// Seeking: persistent state.
			enum Phase {
				NotSeeking,
				Seeking,
				CompletedSeeking
			} phase;
			bool did_seek;
			bool seek_failed;

			// Seeking: transient state.
			int step_rate_counter;
			int steps_taken;
			int target_head_position;	// either an actual number, or -1 to indicate to step until track zero

			/// @returns @c true if the currently queued-up seek or recalibrate has reached where it should be.
			bool seek_is_satisfied();

			// Head state.
			int head_unload_delay[2];
			bool head_is_loaded[2];

			// The connected drive.
			std::shared_ptr<Storage::Disk::Drive> drive;

			Drive() :
				head_position(0), phase(NotSeeking),
				drive(new Storage::Disk::Drive),
				head_is_loaded{false, false} {};
		} drives_[4];
		int drives_seeking_;

		// User-supplied parameters; as per the specify command.
		int step_rate_time_;
		int head_unload_time_;
		int head_load_time_;
		bool dma_mode_;

		// A count of head unload timers currently running.
		int head_timers_running_;

		// Transient storage and counters used while reading the disk.
		uint8_t header_[6];
		int distance_into_section_;
		int index_hole_count_, index_hole_limit_;

		// Keeps track of the drive and head in use during commands.
		int active_drive_;
		int active_head_;

		// Internal registers.
		uint8_t cylinder_, head_, sector_, size_;

};

}
}

#endif /* i8272_hpp */
