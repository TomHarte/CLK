//
//  File.hpp
//  Clock Signal
//
//  Created by Thomas Harte on 18/09/2016.
//  Copyright 2016 Thomas Harte. All rights reserved.
//

#ifndef StaticAnalyser_Acorn_File_hpp
#define StaticAnalyser_Acorn_File_hpp

#include <memory>
#include <string>
#include <vector>

namespace Analyser {
namespace Static {
namespace Acorn {

struct File {
	std::string name;
	uint32_t load_address = 0;
	uint32_t execution_address = 0;

	enum Flags: uint16_t {
		Readable = 1 << 0,
		Writable = 1 << 1,
		Locked = 1 << 2,
		IsDirectory = 1 << 3,
		ExecuteOnly = 1 << 4,
		PubliclyReadable = 1 << 5,
		PubliclyWritable = 1 << 6,
		PubliclyExecuteOnly = 1 << 7,
		IsPrivate = 1 << 8,
	};
	uint16_t flags = Flags::Readable | Flags::Readable | Flags::PubliclyReadable | Flags::PubliclyWritable;
	uint8_t sequence_number = 0;

	std::vector<uint8_t> data;

	/// Describes a single chunk of file data; these relate to the tape and ROM filing system.
	/// The File-level fields contain a 'definitive' version of the load and execution addresses,
	/// but both of those filing systems also store them per chunk.
	///
	/// Similarly, the file-level data will contain the aggregate data of all chunks.
	struct Chunk {
		std::string name;
		uint32_t load_address = 0;
		uint32_t execution_address = 0;
		uint16_t block_number = 0;
		uint16_t block_length = 0;
		uint32_t next_address = 0;
		uint8_t block_flag = 0;

		bool header_crc_matched;
		bool data_crc_matched;
		std::vector<uint8_t> data;
	};

	std::vector<Chunk> chunks;
};

}
}
}

#endif /* File_hpp */
