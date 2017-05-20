//
//  AllRAMProcessor.hpp
//  Clock Signal
//
//  Created by Thomas Harte on 16/05/2017.
//  Copyright © 2017 Thomas Harte. All rights reserved.
//

#ifndef AllRAMProcessor_hpp
#define AllRAMProcessor_hpp

#include <cstdint>
#include <set>
#include <vector>

namespace CPU {

class AllRAMProcessor {
	public:
		AllRAMProcessor(size_t memory_size);
		uint32_t get_timestamp();
		void set_data_at_address(uint16_t startAddress, size_t length, const uint8_t *data);

		class TrapHandler {
			public:
				virtual void processor_did_trap(AllRAMProcessor &, uint16_t address) = 0;
		};
		void set_trap_handler(TrapHandler *trap_handler);
		void add_trap_address(uint16_t address);

	protected:
		std::vector<uint8_t> memory_;
		uint32_t timestamp_;

		void check_address_for_trap(uint16_t address);

	private:
		std::set<uint16_t> trap_addresses_;
		TrapHandler *trap_handler_;
};

}

#endif /* AllRAMProcessor_hpp */
