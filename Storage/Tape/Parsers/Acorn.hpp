//
//  Acorn.hpp
//  Clock Signal
//
//  Created by Thomas Harte on 06/11/2016.
//  Copyright © 2016 Thomas Harte. All rights reserved.
//

#ifndef Storage_Tape_Parsers_Acorn_hpp
#define Storage_Tape_Parsers_Acorn_hpp

#include "TapeParser.hpp"
#include "../../../NumberTheory/CRC.hpp"

namespace Storage {
namespace Tape {
namespace Acorn {

enum class WaveType {
	Short, Long, Unrecognised
};

enum class SymbolType {
	One, Zero
};

class Parser: public Storage::Tape::Parser<WaveType, SymbolType> {
	public:
		Parser();

		int get_next_bit(const std::shared_ptr<Storage::Tape::Tape> &tape);
		int get_next_byte(const std::shared_ptr<Storage::Tape::Tape> &tape);
		int get_next_short(const std::shared_ptr<Storage::Tape::Tape> &tape);
		int get_next_word(const std::shared_ptr<Storage::Tape::Tape> &tape);
		void reset_crc();
		uint16_t get_crc();

	private:
		void process_pulse(Storage::Tape::Tape::Pulse pulse);
		void inspect_waves(const std::vector<WaveType> &waves);
		NumberTheory::CRC16 crc_;
};

}
}
}

#endif /* Acorn_hpp */
