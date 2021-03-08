//
//  Spectrum.hpp
//  Clock Signal
//
//  Created by Thomas Harte on 07/03/2021.
//  Copyright © 2021 Thomas Harte. All rights reserved.
//

#ifndef Storage_Tape_Parsers_Spectrum_hpp
#define Storage_Tape_Parsers_Spectrum_hpp

#include "TapeParser.hpp"

#include <optional>

namespace Storage {
namespace Tape {
namespace ZXSpectrum {

enum class WaveType {
	// All references to 't-states' below are cycles relative to the
	// ZX Spectrum's 3.5Mhz processor.

	Pilot,	// Nominally 2168 t-states.
	Sync1,	// 667 t-states.
	Sync2,	// 735 t-states.
	Zero,	// 855 t-states.
	One,	// 1710 t-states.
	Gap,
};

enum class SymbolType {
	Pilot,
	Sync,
	Zero,
	One,
	Gap,
};

struct Header {
	uint8_t type = 0;
	char name[11]{};	// 10 bytes on tape; always given a NULL terminator in this code.
	uint16_t data_length = 0;
	uint16_t parameters[2] = {0, 0};

	enum class Type {
		Program = 0,
		NumberArray = 1,
		CharacterArray = 2,
		Code = 3,
		Unknown
	};
	Type decoded_type() {
		if(type > 3) return Type::Unknown;
		return Type(type);
	}

	struct BasicParameters {
		std::optional<uint16_t> autostart_line_number;
		uint16_t start_of_variable_area;
	};
	BasicParameters basic_parameters() {
		const BasicParameters params = {
			.autostart_line_number = parameters[0] < 32768 ? std::make_optional(parameters[0]) : std::nullopt,
			.start_of_variable_area = parameters[1]
		};
		return params;
	}

	struct CodeParameters {
		uint16_t start_address;
	};
	CodeParameters code_parameters() {
		const CodeParameters params = {
			.start_address = parameters[0]
		};
		return params;
	}

	struct DataParameters {
		char name;
		enum class Type {
			Numeric,
			String
		} type;
	};
	DataParameters data_parameters() {
		#if TARGET_RT_BIG_ENDIAN
		const uint8_t data_name = uint8_t(parameters[0]);
		#else
		const uint8_t data_name = uint8_t(parameters[0] >> 8);
		#endif

		using Type = DataParameters::Type;
		const DataParameters params = {
			.name = char((data_name & 0x1f) + 'a'),
			.type = (data_name & 0x40) ? Type::String : Type::Numeric
		};
		return params;
	}
};

class Parser: public Storage::Tape::PulseClassificationParser<WaveType, SymbolType> {
	public:
		/*!
			Finds the next header from the tape, if any.
		*/
		std::optional<Header> find_header(const std::shared_ptr<Storage::Tape::Tape> &tape);

		void reset_checksum();
		std::optional<uint8_t> get_byte(const std::shared_ptr<Storage::Tape::Tape> &tape);

	private:
		void process_pulse(const Storage::Tape::Tape::Pulse &pulse) override;
		void inspect_waves(const std::vector<WaveType> &waves) override;

		uint8_t checksum_ = 0;
};

}
}
}

#endif /* Spectrum_hpp */
