//
//  Z80AllRAM.hpp
//  Clock Signal
//
//  Created by Thomas Harte on 16/05/2017.
//  Copyright © 2017 Thomas Harte. All rights reserved.
//

#ifndef Z80AllRAM_hpp
#define Z80AllRAM_hpp

#include "Z80.hpp"

namespace CPU {
namespace Z80 {

class AllRAMProcessor: public Processor<AllRAMProcessor> {
	public:
		AllRAMProcessor();

		int perform_machine_cycle(const MachineCycle *cycle);

		void set_data_at_address(uint16_t startAddress, size_t length, const uint8_t *data);
		uint32_t get_timestamp();

	private:
		uint8_t memory_[65536];
		uint32_t timestamp_;
};

}
}

#endif /* Z80AllRAM_hpp */
