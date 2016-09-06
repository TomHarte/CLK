//
//  TapeParser.hpp
//  Clock Signal
//
//  Created by Thomas Harte on 05/09/2016.
//  Copyright © 2016 Thomas Harte. All rights reserved.
//

#ifndef TapeParser_hpp
#define TapeParser_hpp

namespace StaticAnalyer {

/*!
	A partly-abstract base class to help in the authorship of tape format parsers;
	provides hooks for a 
*/
template <typename WaveType, typename SymbolType> class TapeParser {
	public:
		TapeParser(const std::shared_ptr<Storage::Tape::Tape> &tape) : _tape(tape), _has_next_symbol(false) {}

		void reset_error_flag()		{	_error_flag = false;		}
		bool get_error_flag()		{	return _error_flag;			}
		bool is_at_end()			{	return _tape->is_at_end();	}

	protected:
		bool _error_flag;
		void push_wave(WaveType wave)
		{
			_wave_queue.push_back(wave);
			inspect_waves(_wave_queue);
		}

		void remove_waves(int number_of_waves)
		{
			_wave_queue.erase(_wave_queue.begin(), _wave_queue.begin()+number_of_waves);
		}

		void push_symbol(SymbolType symbol, int number_of_waves)
		{
			_has_next_symbol = true;
			_next_symbol = symbol;
			remove_waves(number_of_waves);
		}

		SymbolType get_next_symbol()
		{
			while(!_has_next_symbol && !is_at_end())
			{
				process_pulse(_tape->get_next_pulse());
			}
			_has_next_symbol = false;
			return _next_symbol;
		}

	private:
		virtual void process_pulse(Storage::Tape::Tape::Pulse pulse) = 0;
		virtual void inspect_waves(const std::vector<WaveType> &waves) = 0;

		std::vector<WaveType> _wave_queue;
		SymbolType _next_symbol;
		bool _has_next_symbol;

		std::shared_ptr<Storage::Tape::Tape> _tape;
};

}

#endif /* TapeParser_hpp */
