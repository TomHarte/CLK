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
#include "../AllRAMProcessor.hpp"

namespace CPU {
namespace Z80 {

class AllRAMProcessor:
	public ::CPU::AllRAMProcessor,
	public Processor<AllRAMProcessor> {

	public:
		AllRAMProcessor();

		int perform_machine_cycle(const MachineCycle *cycle);

		struct MemoryAccessDelegate {
			virtual void z80_all_ram_processor_did_perform_bus_operation(AllRAMProcessor &processor, BusOperation operation, uint16_t address, uint8_t value, int time_stamp) = 0;
		};
		void set_memory_access_delegate(MemoryAccessDelegate *delegate) {
			delegate_ = delegate;
		}

	private:
		MemoryAccessDelegate *delegate_;
};

}
}

#endif /* Z80AllRAM_hpp */
