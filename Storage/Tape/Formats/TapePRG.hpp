//
//  TapePRG.hpp
//  Clock Signal
//
//  Created by Thomas Harte on 14/08/2016.
//  Copyright 2016 Thomas Harte. All rights reserved.
//

#pragma once

#include "Storage/Tape/Tape.hpp"
#include "Storage/FileHolder.hpp"
#include "Storage/TargetPlatforms.hpp"

#include <cstdint>
#include <string_view>

namespace Storage::Tape {

/*!
	Provides a @c Tape containing a .PRG, which is a direct local file.
*/
class PRG: public Tape {
public:
	/*!
		Constructs a @c T64 containing content from the file with name @c file_name, of type @c type.

		@param file_name The name of the file to load.
		@throws ErrorBadFormat if this file could not be opened and recognised as the specified type.
	*/
	PRG(std::string_view file_name);

	enum {
		ErrorBadFormat
	};

private:
	std::unique_ptr<FormatSerialiser> format_serialiser() const override;

	struct Serialiser: public FormatSerialiser, public TargetPlatform::Recipient {
		Serialiser(std::string_view file_name, uint16_t load_address, uint16_t length);
		void set_target_platforms(TargetPlatform::Type) override;

	private:
		bool is_at_end() const override;
		Pulse next_pulse() override;
		void reset() override;

		FileHolder file_;
		uint16_t load_address_;
		uint16_t length_;

		enum FilePhase {
			FilePhaseLeadIn,
			FilePhaseHeader,
			FilePhaseHeaderDataGap,
			FilePhaseData,
			FilePhaseAtEnd
		} file_phase_ = FilePhaseLeadIn;
		int phase_offset_ = 0;

		int bit_phase_ = 3;
		enum OutputToken {
			Leader,
			Zero,
			One,
			WordMarker,
			EndOfBlock,
			Silence
		} output_token_;
		void get_next_output_token();
		uint8_t output_byte_;
		uint8_t check_digit_;
		uint8_t copy_mask_ = 0x80;

		struct Timings {
			Timings(bool is_plus4) :
				leader_zero_length(	is_plus4 ? 240 : 179),
				zero_length(		is_plus4 ? 240 : 169),
				one_length(			is_plus4 ? 480 : 247),
				marker_length(		is_plus4 ? 960 : 328) {}

			// The below are in microseconds per pole.
			unsigned int leader_zero_length;
			unsigned int zero_length;
			unsigned int one_length;
			unsigned int marker_length;
		} timings_;
	};
	std::string file_name_;
	uint16_t load_address_;
	uint16_t length_;
};

}
