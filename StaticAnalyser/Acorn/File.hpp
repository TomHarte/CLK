//
//  File.hpp
//  Clock Signal
//
//  Created by Thomas Harte on 18/09/2016.
//  Copyright © 2016 Thomas Harte. All rights reserved.
//

#ifndef StaticAnalyser_Acorn_File_hpp
#define StaticAnalyser_Acorn_File_hpp

#include <memory>
#include <string>
#include <vector>

namespace StaticAnalyser {
namespace Acorn {

struct File {
	std::string name;
	uint32_t load_address;
	uint32_t execution_address;
	bool is_protected;
	std::vector<uint8_t> data;

	struct Chunk {
		std::string name;
		uint32_t load_address;
		uint32_t execution_address;
		uint16_t block_number;
		uint16_t block_length;
		uint8_t block_flag;
		uint32_t next_address;

		bool header_crc_matched;
		bool data_crc_matched;
		std::vector<uint8_t> data;
	};

	std::vector<Chunk> chunks;
};

}
}

#endif /* File_hpp */
