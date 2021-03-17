//
//  TapeParser.hpp
//  Clock Signal
//
//  Created by Thomas Harte on 05/09/2016.
//  Copyright 2016 Thomas Harte. All rights reserved.
//

#ifndef TapeParser_hpp
#define TapeParser_hpp

#include "../Tape.hpp"

#include <cassert>
#include <memory>
#include <vector>

namespace Storage {
namespace Tape {

template <typename SymbolType> class Parser {
	public:
		/// Resets the error flag.
		void reset_error_flag()		{	error_flag_ = false;		}
		/// @returns @c true if an error has occurred since the error flag was last reset; @c false otherwise.
		bool get_error_flag()		{	return error_flag_;			}

		/*!
			Asks the parser to continue taking pulses from the tape until either the subclass next declares a symbol
			or the tape runs out, returning the most-recently declared symbol.
		*/
		SymbolType get_next_symbol(const std::shared_ptr<Storage::Tape::Tape> &tape) {
			while(!has_next_symbol_ && !tape->is_at_end()) {
				process_pulse(tape->get_next_pulse());
			}
			if(!has_next_symbol_ && tape->is_at_end()) mark_end();
			has_next_symbol_ = false;
			return next_symbol_;
		}

		/*!
			This class provides a single token of lookahead; return_symbol allows the single previous
			token supplied by get_next_symbol to be returned, in which case it will be the thing returned
			by the next call to get_next_symbol.
		*/
		void return_symbol(SymbolType symbol) {
			assert(!has_next_symbol_);
			has_next_symbol_ = true;
			next_symbol_ = symbol;
		}

		/*!
			@returns `true` if there is no data left on the tape and the WaveType queue has been exhausted; `false` otherwise.
		*/
		bool is_at_end(const std::shared_ptr<Storage::Tape::Tape> &tape) {
			return tape->is_at_end() && !has_next_symbol_;
		}

		/*!
			Swallows symbols until it reaches the first instance of the required symbol, swallows that
			and returns.
		*/
		void proceed_to_symbol(const std::shared_ptr<Storage::Tape::Tape> &tape, SymbolType required_symbol) {
			while(!is_at_end(tape)) {
				const SymbolType symbol = get_next_symbol(tape);
				if(symbol == required_symbol) return;
			}
		}

	protected:
		/*!
			Should be implemented by subclasses. Consumes @c pulse.
		*/
		virtual void process_pulse(const Storage::Tape::Tape::Pulse &pulse) = 0;

		/*!
			An optional implementation for subclasses; called to announce that the tape has ended: that
			no more process_pulse calls will occur.
		*/
		virtual void mark_end() {}

		/*!
			Sets @c symbol as the newly-recognised symbol.
		*/
		void push_symbol(SymbolType symbol) {
			has_next_symbol_ = true;
			next_symbol_ = symbol;
		}

		void set_error_flag() {
			error_flag_ = true;
		}

		bool error_flag_ = false;
		SymbolType next_symbol_;
		bool has_next_symbol_ = false;
};

/*!
	A partly-abstract base class to help in the authorship of tape format parsers;
	provides hooks for receipt of pulses, which are intended to be classified into waves,
	and for symbol identification from waves.

	Very optional, not intended to box in the approaches taken for analysis. See also
	the PLLParser.
*/
template <typename WaveType, typename SymbolType> class PulseClassificationParser: public Parser<SymbolType> {
	public:
		virtual void process_pulse(const Storage::Tape::Tape::Pulse &pulse) = 0;

		/*
			process_pulse should either call @c push_wave or to take no action.
		*/

	protected:
		/*!
			Sets @c symbol as the newly-recognised symbol and removes @c nunber_of_waves waves from the front of the list.

			Expected to be called by subclasses from @c process_pulse when it recognises that the first @c number_of_waves
			waves together represent @c symbol.
		*/
		void push_symbol(SymbolType symbol, int number_of_waves) {
			Parser<SymbolType>::push_symbol(symbol);
			remove_waves(number_of_waves);
		}

		/*!
			Adds @c wave to the back of the list of recognised waves and calls @c inspect_waves to check for a new symbol.

			Expected to be called by subclasses from @c process_pulse as and when recognised waves arise.
		*/
		void push_wave(WaveType wave) {
			wave_queue_.push_back(wave);
			inspect_waves(wave_queue_);
		}

		/*!
			Removes @c nunber_of_waves waves from the front of the list.

			Expected to be called by subclasses from @c process_pulse if it is recognised that the first set of waves
			do not form a valid symbol.
		*/
		void remove_waves(int number_of_waves) {
			wave_queue_.erase(wave_queue_.begin(), wave_queue_.begin()+number_of_waves);
		}

	private:
		/*!
			Should be implemented by subclasses. Inspects @c waves for a potential new symbol. If one is
			found should call @c push_symbol. May wish alternatively to call @c remove_waves to have entries
			removed from the start of @c waves that cannot form a valid symbol. Need not do anything while
			the waves at the start of @c waves may end up forming a symbol but the symbol is not yet complete.
		*/
		virtual void inspect_waves(const std::vector<WaveType> &waves) = 0;

		std::vector<WaveType> wave_queue_;
};

}
}

#endif /* TapeParser_hpp */
