//
//  AllRAMProcessor.hpp
//  Clock Signal
//
//  Created by Thomas Harte on 16/05/2017.
//  Copyright 2017 Thomas Harte. All rights reserved.
//

#ifndef AllRAMProcessor_hpp
#define AllRAMProcessor_hpp

#include <cstdint>
#include <set>
#include <vector>

#include "../ClockReceiver/ClockReceiver.hpp"

namespace CPU {

class AllRAMProcessor {
	public:
		AllRAMProcessor(std::size_t memory_size);
		HalfCycles get_timestamp();
		void set_data_at_address(size_t startAddress, size_t length, const uint8_t *data);
		void get_data_at_address(size_t startAddress, size_t length, uint8_t *data);

		class TrapHandler {
			public:
				virtual void processor_did_trap(AllRAMProcessor &, uint16_t address) = 0;
		};
		void set_trap_handler(TrapHandler *trap_handler);
		void add_trap_address(uint16_t address);

	protected:
		std::vector<uint8_t> memory_;
		HalfCycles timestamp_;

		inline void check_address_for_trap(uint16_t address) {
			if(traps_[address]) {
				trap_handler_->processor_did_trap(*this, address);
			}
		}

	private:
		TrapHandler *trap_handler_;
		std::vector<bool> traps_;
};

}

#endif /* AllRAMProcessor_hpp */
