//
//  ZX8081.hpp
//  Clock Signal
//
//  Created by Thomas Harte on 07/06/2017.
//  Copyright © 2017 Thomas Harte. All rights reserved.
//

#ifndef Storage_Tape_Parsers_ZX8081_hpp
#define Storage_Tape_Parsers_ZX8081_hpp

#include "TapeParser.hpp"
#include <string>
#include <vector>
#include <cstdint>

namespace Storage {
namespace Tape {
namespace ZX8081 {

struct File {
	std::vector<uint8_t> data;
	std::string name;
	bool isZX81;
};

enum class WaveType {
	Pulse, Gap, LongGap
};

enum class SymbolType {
	One, Zero, Gap
};

class Parser: public Storage::Tape::Parser<WaveType, SymbolType> {
	public:
		Parser();

		/*!
			Reads and combines the next eight bits.
		*/
		uint8_t get_next_byte(const std::shared_ptr<Storage::Tape::Tape> &tape);

		/*!
			Waits for a long gap, reads all the bytes between that and the next long gap, then
			attempts to parse those as a valid ZX80 or ZX81 file. If no file is found,
			returns nullptr.
		*/
		std::shared_ptr<File> get_next_file();

	private:
		/*!
			Proceeds beyond the current or next gap then counts pulses until the gap afterwards, and returns the resulting bit.
		*/
		uint8_t get_next_bit(const std::shared_ptr<Storage::Tape::Tape> &tape);
};

}
}
}

#endif /* ZX8081_hpp */
