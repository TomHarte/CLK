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
	Pulse, Gap, LongGap, Unrecognised
};

enum class SymbolType {
	One, Zero, FileGap, Unrecognised
};

class Parser: public Storage::Tape::Parser<WaveType, SymbolType> {
	public:
		Parser();

		/*!
			Reads and combines the next eight bits. Returns -1 if any errors are encountered.
		*/
		int get_next_byte(const std::shared_ptr<Storage::Tape::Tape> &tape);

		/*!
			Waits for a long gap, reads all the bytes between that and the next long gap, then
			attempts to parse those as a valid ZX80 or ZX81 file. If no file is found,
			returns nullptr.
		*/
		std::shared_ptr<File> get_next_file(const std::shared_ptr<Storage::Tape::Tape> &tape);

	private:
		bool pulse_was_high_;
		Time pulse_time_;
		void process_pulse(Storage::Tape::Tape::Pulse pulse);
		void inspect_waves(const std::vector<WaveType> &waves);

		std::shared_ptr<File> get_next_file_data(const std::shared_ptr<Storage::Tape::Tape> &tape);
};

}
}
}

#endif /* ZX8081_hpp */
