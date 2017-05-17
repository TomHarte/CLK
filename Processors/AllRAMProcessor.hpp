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
#include <vector>

namespace CPU {

class AllRAMProcessor {
	public:
		AllRAMProcessor(size_t memory_size);
		uint32_t get_timestamp();
		void set_data_at_address(uint16_t startAddress, size_t length, const uint8_t *data);

	protected:
		std::vector<uint8_t> memory_;
		uint32_t timestamp_;
};

}

#endif /* AllRAMProcessor_hpp */
