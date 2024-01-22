//
//  Oric.hpp
//  Clock Signal
//
//  Created by Thomas Harte on 06/11/2016.
//  Copyright 2016 Thomas Harte. All rights reserved.
//

#pragma once

#include "TapeParser.hpp"

namespace Storage::Tape::Oric {

enum class WaveType {
	Short,	// i.e. 416 microseconds
	Medium,	// i.e. 624 microseconds
	Long,	// i.e. 832 microseconds
	Unrecognised
};

enum class SymbolType {
	One, Zero, FoundFast, FoundSlow
};

class Parser: public Storage::Tape::PulseClassificationParser<WaveType, SymbolType> {
	public:
		int get_next_byte(const std::shared_ptr<Storage::Tape::Tape> &tape, bool use_fast_encoding);
		bool sync_and_get_encoding_speed(const std::shared_ptr<Storage::Tape::Tape> &tape);

	private:
		void process_pulse(const Storage::Tape::Tape::Pulse &pulse) override;
		void inspect_waves(const std::vector<WaveType> &waves) override;

		enum DetectionMode {
			FastData,
			SlowData,
			FastZero,
			SlowZero,
			Sync
		} detection_mode_;
		bool wave_was_high_;
		float cycle_length_;

		struct Pattern {
			WaveType type;
			int count = 0;
		};
		std::size_t pattern_matching_depth(const std::vector<WaveType> &waves, const Pattern *pattern);
};

}
