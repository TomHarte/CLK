//
//  Tape.cpp
//  Clock Signal
//
//  Created by Thomas Harte on 24/08/2016.
//  Copyright © 2016 Thomas Harte. All rights reserved.
//

#include "Tape.hpp"

#include <deque>
#include "../TapeParser.hpp"

using namespace StaticAnalyser::Commodore;

enum class WaveType {
	Short, Medium, Long, Unrecognised
};

enum class SymbolType {
	One, Zero, Word, EndOfBlock, LeadIn
};

class CommodoreROMTapeParser: public StaticAnalyer::TapeParser<WaveType, SymbolType> {
	public:
		CommodoreROMTapeParser(const std::shared_ptr<Storage::Tape::Tape> &tape) :
			TapeParser(tape),
			_wave_period(0.0f),
			_previous_was_high(false) {}

		void spin()
		{
			while(!is_at_end())
			{
				SymbolType symbol = get_next_symbol();
				switch(symbol)
				{
					case SymbolType::One:			printf("1");	break;
					case SymbolType::Zero:			printf("0");	break;
					case SymbolType::Word:			printf(" ");	break;
					case SymbolType::EndOfBlock:	printf("\n");	break;
					case SymbolType::LeadIn:		printf("-");	break;
				}
			}
		}

	private:
		void process_pulse(Storage::Tape::Tape::Pulse pulse)
		{
			bool is_high = pulse.type == Storage::Tape::Tape::Pulse::High;
			_wave_period += pulse.length.get_float();

			if(!is_high && _previous_was_high)
			{
				if(_wave_period >= 0.000592 && _wave_period < 0.000752)			push_wave(WaveType::Long);
				else if(_wave_period >= 0.000432 && _wave_period < 0.000592)	push_wave(WaveType::Medium);
				else if(_wave_period >= 0.000272 && _wave_period < 0.000432)	push_wave(WaveType::Short);
				else push_wave(WaveType::Unrecognised);

				_wave_period = 0.0f;
			}

			_previous_was_high = is_high;
		}
		bool _previous_was_high;
		float _wave_period;

		void inspect_waves(const std::vector<WaveType> &waves)
		{
			if(waves.size() < 2) return;

			if(waves[0] == WaveType::Long && waves[1] == WaveType::Medium)
			{
				push_symbol(SymbolType::Word, 2);
				return;
			}

			if(waves[0] == WaveType::Long && waves[1] == WaveType::Short)
			{
				push_symbol(SymbolType::EndOfBlock, 2);
				return;
			}

			if(waves[0] == WaveType::Short && waves[1] == WaveType::Medium)
			{
				push_symbol(SymbolType::Zero, 2);
				return;
			}

			if(waves[0] == WaveType::Medium && waves[1] == WaveType::Short)
			{
				push_symbol(SymbolType::One, 2);
				return;
			}

			if(waves[0] == WaveType::Short)
			{
				push_symbol(SymbolType::LeadIn, 1);
				return;
			}

			// Otherwise, eject at least one wave as all options are exhausted.
			remove_waves(1);
		}
};

std::list<File> StaticAnalyser::Commodore::GetFiles(const std::shared_ptr<Storage::Tape::Tape> &tape)
{
	CommodoreROMTapeParser parser(tape);
	parser.spin();

	std::list<File> file_list;
	return file_list;
}
