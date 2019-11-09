//
//  i8272.hpp
//  Clock Signal
//
//  Created by Thomas Harte on 05/08/2017.
//  Copyright 2017 Thomas Harte. All rights reserved.
//

#ifndef i8272_hpp
#define i8272_hpp

#include "../../Storage/Disk/Controller/MFMDiskController.hpp"

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

class i8272 : public Storage::Disk::MFMController {
	public:
		i8272(BusHandler &bus_handler, Cycles clock_rate);

		void run_for(Cycles);

		void set_data_input(uint8_t value);
		uint8_t get_data_output();

		void set_register(int address, uint8_t value);
		uint8_t get_register(int address);

		void set_dma_acknowledge(bool dack);
		void set_terminal_count(bool tc);

		ClockingHint::Preference preferred_clocking() final;

	protected:
		virtual void select_drive(int number) = 0;

	private:
		// The bus handler, for interrupt and DMA-driven usage.
		BusHandler &bus_handler_;
		std::unique_ptr<BusHandler> allocated_bus_handler_;

		// Status registers.
		uint8_t main_status_ = 0;
		uint8_t status_[3] = {0, 0, 0};

		// A buffer for accumulating the incoming command, and one for accumulating the result.
		std::vector<uint8_t> command_;
		std::vector<uint8_t> result_stack_;
		uint8_t input_ = 0;
		bool has_input_ = false;
		bool expects_input_ = false;

		// Event stream: the 8272-specific events, plus the current event state.
		enum class Event8272: int {
			CommandByte	= (1 << 3),
			Timer = (1 << 4),
			ResultEmpty = (1 << 5),
			NoLongerReady = (1 << 6)
		};
		void posit_event(int type) override;
		int interesting_event_mask_ = static_cast<int>(Event8272::CommandByte);
		int resume_point_ = 0;
		bool is_access_command_ = false;

		// The counter used for ::Timer events.
		Cycles::IntType delay_time_ = 0;

		// The connected drives.
		struct Drive {
			uint8_t head_position = 0;

			// Seeking: persistent state.
			enum Phase {
				NotSeeking,
				Seeking,
				CompletedSeeking
			} phase = NotSeeking;
			bool did_seek = false;
			bool seek_failed = false;

			// Seeking: transient state.
			Cycles::IntType step_rate_counter = 0;
			int steps_taken = 0;
			int target_head_position = 0;	// either an actual number, or -1 to indicate to step until track zero

			// Head state.
			Cycles::IntType head_unload_delay[2] = {0, 0};
			bool head_is_loaded[2] = {false, false};

		} drives_[4];
		int drives_seeking_ = 0;

		/// @returns @c true if the selected drive, which is number @c drive, can stop seeking.
		bool seek_is_satisfied(int drive);

		// User-supplied parameters; as per the specify command.
		int step_rate_time_ = 1;
		int head_unload_time_ = 1;
		int head_load_time_ = 1;
		bool dma_mode_ = false;
		bool is_executing_ = false;

		// A count of head unload timers currently running.
		int head_timers_running_ = 0;

		// Transient storage and counters used while reading the disk.
		uint8_t header_[6] = {0, 0, 0, 0, 0, 0};
		int distance_into_section_ = 0;
		int index_hole_count_ = 0, index_hole_limit_ = 0;

		// Keeps track of the drive and head in use during commands.
		int active_drive_ = 0;
		int active_head_ = 0;

		// Internal registers.
		uint8_t cylinder_ = 0, head_ = 0, sector_ = 0, size_ = 0;

		// Master switch on not performing any work.
		bool is_sleeping_ = false;
};

}
}

#endif /* i8272_hpp */
