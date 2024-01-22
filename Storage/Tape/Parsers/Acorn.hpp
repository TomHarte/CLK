//
//  Acorn.hpp
//  Clock Signal
//
//  Created by Thomas Harte on 06/11/2016.
//  Copyright 2016 Thomas Harte. All rights reserved.
//

#pragma once

#include "TapeParser.hpp"
#include "../../../Numeric/CRC.hpp"
#include "../../Disk/DPLL/DigitalPhaseLockedLoop.hpp"

namespace Storage::Tape::Acorn {

class Shifter {
	public:
		Shifter();

		void process_pulse(const Storage::Tape::Tape::Pulse &pulse);

		class Delegate {
			public:
				virtual void acorn_shifter_output_bit(int value) = 0;
		};
		void set_delegate(Delegate *delegate) {
			delegate_ = delegate;
		}

		void digital_phase_locked_loop_output_bit(int value);

	private:
		Storage::DigitalPhaseLockedLoop<Shifter, 15> pll_;
		bool was_high_;

		unsigned int input_pattern_;

		Delegate *delegate_;
};

enum class SymbolType {
	One, Zero
};

class Parser: public Storage::Tape::Parser<SymbolType>, public Shifter::Delegate {
	public:
		Parser();

		int get_next_bit(const std::shared_ptr<Storage::Tape::Tape> &tape);
		int get_next_byte(const std::shared_ptr<Storage::Tape::Tape> &tape);
		unsigned int get_next_short(const std::shared_ptr<Storage::Tape::Tape> &tape);
		unsigned int get_next_word(const std::shared_ptr<Storage::Tape::Tape> &tape);
		void reset_crc();
		uint16_t get_crc() const;

	private:
		void acorn_shifter_output_bit(int value) override;
		void process_pulse(const Storage::Tape::Tape::Pulse &pulse) override;

		bool did_update_shifter(int new_value, int length);
		CRC::Generator<uint16_t, 0x0000, 0x0000, false, false> crc_;
		Shifter shifter_;
};

}
