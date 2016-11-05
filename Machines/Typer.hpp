//
//  Typer.hpp
//  Clock Signal
//
//  Created by Thomas Harte on 19/06/2016.
//  Copyright © 2016 Thomas Harte. All rights reserved.
//

#ifndef Typer_hpp
#define Typer_hpp

#include <memory>
#include "KeyboardMachine.hpp"

namespace Utility {

class Typer {
	public:
		class Delegate: public KeyboardMachine::Machine {
			public:
				virtual bool typer_set_next_character(Typer *typer, char character, int phase);
				virtual void typer_reset(Typer *typer);

				virtual uint16_t *sequence_for_character(Typer *typer, char character);

				const uint16_t EndSequence = 0xffff;
		};

		Typer(const char *string, int delay, int frequency, Delegate *delegate);
		~Typer();
		void update(int duration);
		bool type_next_character();

		const char BeginString = 0x02;	// i.e. ASCII start of text
		const char EndString = 0x03;	// i.e. ASCII end of text

	private:
		char *_string;
		int _frequency;
		int _counter;
		int _phase;
		Delegate *_delegate;
		size_t _string_pointer;
};

class TypeRecipient: public Typer::Delegate {
	public:
		void set_typer_for_string(const char *string)
		{
			_typer.reset(new Typer(string, get_typer_delay(), get_typer_frequency(), this));
		}

		void typer_reset(Typer *typer)
		{
			_typer.reset();
		}

	protected:
		virtual int get_typer_delay() { return 0; }
		virtual int get_typer_frequency() { return 0; }
		std::unique_ptr<Typer> _typer;
};

}

#endif /* Typer_hpp */
