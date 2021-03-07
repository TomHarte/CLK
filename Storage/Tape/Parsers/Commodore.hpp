//
//  Commodore.hpp
//  Clock Signal
//
//  Created by Thomas Harte on 06/11/2016.
//  Copyright 2016 Thomas Harte. All rights reserved.
//

#ifndef Storage_Tape_Parsers_Commodore_hpp
#define Storage_Tape_Parsers_Commodore_hpp

#include "TapeParser.hpp"
#include <memory>
#include <string>

namespace Storage {
namespace Tape {
namespace Commodore {

enum class WaveType {
	Short, Medium, Long, Unrecognised
};

enum class SymbolType {
	One, Zero, Word, EndOfBlock, LeadIn
};

struct Header {
	enum {
		RelocatableProgram,
		NonRelocatableProgram,
		DataSequenceHeader,
		DataBlock,
		EndOfTape,
		Unknown
	} type;

	std::vector<uint8_t> data;
	std::wstring name;
	std::vector<uint8_t> raw_name;
	uint16_t starting_address;
	uint16_t ending_address;
	bool parity_was_valid;
	bool duplicate_matched;

	/*!
		Writes a byte serialised version of this header to @c target, writing at most
		@c length bytes.
	*/
	void serialise(uint8_t *target, uint16_t length);
};

struct Data {
	std::vector<uint8_t> data;
	bool parity_was_valid;
	bool duplicate_matched;
};

class Parser: public Storage::Tape::PulseClassificationParser<WaveType, SymbolType> {
	public:
		Parser();

		/*!
			Advances to the next block on the tape, treating it as a header, then consumes, parses, and returns it.
			Returns @c nullptr if any wave-encoding level errors are encountered.
		*/
		std::unique_ptr<Header> get_next_header(const std::shared_ptr<Storage::Tape::Tape> &tape);

		/*!
			Advances to the next block on the tape, treating it as data, then consumes, parses, and returns it.
			Returns @c nullptr if any wave-encoding level errors are encountered.
		*/
		std::unique_ptr<Data> get_next_data(const std::shared_ptr<Storage::Tape::Tape> &tape);

	private:
		/*!
			Template for the logic in selecting which of two copies of something to consider authoritative,
			including setting the duplicate_matched flag.
		*/
		template<class ObjectType>
			std::unique_ptr<ObjectType> duplicate_match(std::unique_ptr<ObjectType> first_copy, std::unique_ptr<ObjectType> second_copy);

		std::unique_ptr<Header> get_next_header_body(const std::shared_ptr<Storage::Tape::Tape> &tape, bool is_original);
		std::unique_ptr<Data> get_next_data_body(const std::shared_ptr<Storage::Tape::Tape> &tape, bool is_original);

		/*!
			Finds and completes the next landing zone.
		*/
		void proceed_to_landing_zone(const std::shared_ptr<Storage::Tape::Tape> &tape, bool is_original);

		/*!
			Swallows symbols until it reaches the first instance of the required symbol, swallows that
			and returns.
		*/
		void proceed_to_symbol(const std::shared_ptr<Storage::Tape::Tape> &tape, SymbolType required_symbol);

		/*!
			Swallows the next byte; sets the error flag if it is not equal to @c value.
		*/
		void expect_byte(const std::shared_ptr<Storage::Tape::Tape> &tape, uint8_t value);

		uint8_t parity_byte_ = 0;
		void reset_parity_byte();
		uint8_t get_parity_byte();
		void add_parity_byte(uint8_t byte);

		/*!
			Proceeds to the next word marker then returns the result of @c get_next_byte_contents.
		*/
		uint8_t get_next_byte(const std::shared_ptr<Storage::Tape::Tape> &tape);

		/*!
			Reads the next nine symbols and applies a binary test to each to differentiate between ::One and not-::One.
			Returns a byte composed of the first eight of those as bits; sets the error flag if any symbol is not
			::One and not ::Zero, or if the ninth bit is not equal to the odd parity of the other eight.
		*/
		uint8_t get_next_byte_contents(const std::shared_ptr<Storage::Tape::Tape> &tape);

		/*!
			Returns the result of two consecutive @c get_next_byte calls, arranged in little-endian format.
		*/
		uint16_t get_next_short(const std::shared_ptr<Storage::Tape::Tape> &tape);

		/*!
			Per the contract with Analyser::Static::TapeParser; sums time across pulses. If this pulse
			indicates a high to low transition, inspects the time since the last transition, to produce
			a long, medium, short or unrecognised wave period.
		*/
		void process_pulse(const Storage::Tape::Tape::Pulse &pulse) override;
		bool previous_was_high_ = false;
		float wave_period_ = 0.0f;

		/*!
			Per the contract with Analyser::Static::TapeParser; produces any of a word marker, an end-of-block marker,
			a zero, a one or a lead-in symbol based on the currently captured waves.
		*/
		void inspect_waves(const std::vector<WaveType> &waves) override;
};

}
}
}

#endif /* Commodore_hpp */
