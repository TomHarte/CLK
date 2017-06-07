//
//  TapeParser.hpp
//  Clock Signal
//
//  Created by Thomas Harte on 05/09/2016.
//  Copyright © 2016 Thomas Harte. All rights reserved.
//

#ifndef TapeParser_hpp
#define TapeParser_hpp

#include "../Tape.hpp"

#include <memory>
#include <vector>

namespace Storage {
namespace Tape {

/*!
	A partly-abstract base class to help in the authorship of tape format parsers;
	provides hooks for pulse classification from pulses to waves and for symbol identification from
	waves.
	
	Very optional, not intended to box in the approaches taken for analysis.
*/
template <typename WaveType, typename SymbolType> class Parser {
	public:
		/// Instantiates a new parser with the supplied @c tape.
		Parser() : _has_next_symbol(false), _error_flag(false) {}

		/// Resets the error flag.
		void reset_error_flag()		{	_error_flag = false;		}
		/// @returns @c true if an error has occurred since the error flag was last reset; @c false otherwise.
		bool get_error_flag()		{	return _error_flag;			}

		/*!
			Asks the parser to continue taking pulses from the tape until either the subclass next declares a symbol
			or the tape runs out, returning the most-recently declared symbol.
		*/
		SymbolType get_next_symbol(const std::shared_ptr<Storage::Tape::Tape> &tape) {
			while(!_has_next_symbol && !tape->is_at_end()) {
				process_pulse(tape->get_next_pulse());
			}
			_has_next_symbol = false;
			return _next_symbol;
		}

		/*!
			Should be implemented by subclasses. Consumes @c pulse. Is likely either to call @c push_wave
			or to take no action.
		*/
		virtual void process_pulse(Storage::Tape::Tape::Pulse pulse) = 0;

	protected:

		/*!
			Adds @c wave to the back of the list of recognised waves and calls @c inspect_waves to check for a new symbol.
			
			Expected to be called by subclasses from @c process_pulse as and when recognised waves arise.
		*/
		void push_wave(WaveType wave) {
			_wave_queue.push_back(wave);
			inspect_waves(_wave_queue);
		}

		/*!
			Removes @c nunber_of_waves waves from the front of the list.
			
			Expected to be called by subclasses from @c process_pulse if it is recognised that the first set of waves
			do not form a valid symbol.
		*/
		void remove_waves(int number_of_waves) {
			_wave_queue.erase(_wave_queue.begin(), _wave_queue.begin()+number_of_waves);
		}

		/*!
			Sets @c symbol as the newly-recognised symbol and removes @c nunber_of_waves waves from the front of the list.
			
			Expected to be called by subclasses from @c process_pulse when it recognises that the first @c number_of_waves
			waves together represent @c symbol.
		*/
		void push_symbol(SymbolType symbol, int number_of_waves) {
			_has_next_symbol = true;
			_next_symbol = symbol;
			remove_waves(number_of_waves);
		}

		void set_error_flag() {
			_error_flag = true;
		}

	private:
		bool _error_flag;

		/*!
			Should be implemented by subclasses. Inspects @c waves for a potential new symbol. If one is
			found should call @c push_symbol. May wish alternatively to call @c remove_waves to have entries
			removed from the start of @c waves that cannot form a valid symbol. Need not do anything while
			the waves at the start of @c waves may end up forming a symbol but the symbol is not yet complete.
		*/
		virtual void inspect_waves(const std::vector<WaveType> &waves) = 0;

		std::vector<WaveType> _wave_queue;
		SymbolType _next_symbol;
		bool _has_next_symbol;
};

}
}

#endif /* TapeParser_hpp */
