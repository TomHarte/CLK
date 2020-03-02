//
//  Typer.hpp
//  Clock Signal
//
//  Created by Thomas Harte on 19/06/2016.
//  Copyright 2016 Thomas Harte. All rights reserved.
//

#ifndef Typer_hpp
#define Typer_hpp

#include <memory>
#include <string>

#include "../KeyboardMachine.hpp"
#include "../../ClockReceiver/ClockReceiver.hpp"

namespace Utility {

/*!
	An interface that provides a mapping from logical characters to the sequence of keys
	necessary to type that character on a given machine.
*/
class CharacterMapper {
	public:
		virtual ~CharacterMapper() {}

		/// @returns The EndSequence-terminated sequence of keys that would cause @c character to be typed.
		virtual uint16_t *sequence_for_character(char character) = 0;

		/// The typer will automatically reset all keys in between each sequence that it types.
		/// By default it will pause for one key's duration when doing so. Character mappers
		/// can eliminate that pause by overriding this method.
		/// @returns @c true if the typer should pause after performing a reset; @c false otherwise.
		virtual bool needs_pause_after_reset_all_keys()	{ return true; }

		/// The typer will pause between every entry in a keyboard sequence. On some machines
		/// that may not be necessary — it'll often depends on whether the machine needs time to
		/// observe a modifier like shift before it sees the actual keypress.
		/// @returns @c true if the typer should pause after forwarding @c key; @c false otherwise.
		virtual bool needs_pause_after_key(uint16_t key)	{ return true; }

	protected:
		typedef uint16_t KeySequence[16];

		/*!
			Provided in the base class as a convenience: given the lookup table of key sequences @c sequences,
			with @c length entries, returns the sequence for character @c character if it exists; otherwise
			returns @c nullptr.
		*/
		uint16_t *table_lookup_sequence_for_character(KeySequence *sequences, std::size_t length, char character);
};

/*!
	Provides a stateful mechanism for typing a sequence of characters. Each character is mapped to a key sequence
	by a character mapper. That key sequence is then replayed to a delegate.

	Being given a delay and frequency at construction, the run_for interface can be used to produce time-based
	typing. Alternatively, an owner may decline to use run_for and simply call type_next_character each time a
	fresh key transition is ready to be consumed.
*/
class Typer {
	public:
		class Delegate: public KeyboardMachine::KeyActions {
			public:
				/// Informs the delegate that this typer has reached the end of its content.
				virtual void typer_reset(Typer *typer) = 0;
		};

		Typer(const std::string &string, HalfCycles delay, HalfCycles frequency, CharacterMapper &character_mapper, Delegate *delegate);

		/// Advances for @c duration.
		void run_for(const HalfCycles duration);

		/// Types the next character now, if there is one.
		/// @returns @c true if there was anything left to type; @c false otherwise.
		bool type_next_character();

		/// Adds the contents of @c str to the end of the current string.
		void append(const std::string &str);

		const char BeginString = 0x02;	// i.e. ASCII start of text
		const char EndString = 0x03;	// i.e. ASCII end of text

	private:
		std::string string_;
		std::size_t string_pointer_ = 0;

		HalfCycles frequency_;
		HalfCycles counter_;
		int phase_ = 0;

		Delegate *delegate_;
		CharacterMapper &character_mapper_;

		uint16_t try_type_next_character();
		const uint16_t *sequence_for_character(char) const;
};

/*!
	Provides a default base class for type recipients: classes that want to attach a single typer at a time and
	which may or may not want to nominate an initial delay and typing frequency.
*/
template <typename CMApper>
class TypeRecipient: public Typer::Delegate {
	protected:
		template <typename... Args> TypeRecipient(Args&&... args) : character_mapper(std::forward<Args>(args)...) {}

		/// Attaches a typer to this class that will type @c string using @c character_mapper as a source.
		void add_typer(const std::string &string) {
			if(!typer_) {
				typer_ = std::make_unique<Typer>(string, get_typer_delay(), get_typer_frequency(), character_mapper, this);
			} else {
				typer_->append(string);
			}
		}

		/*!
			@returns @c true if the character mapper provides a mapping for @c c; @c false otherwise.
		*/
		bool can_type(char c) {
			const auto sequence = character_mapper.sequence_for_character(c);
			return sequence && sequence[0] != KeyboardMachine::MappedMachine::KeyNotMapped;
		}

		/*!
			Provided in order to conform to that part of the Typer::Delegate interface that goes above and
			beyond KeyboardMachine::Machine; responds to the end of typing by clearing all keys.
		*/
		void typer_reset(Typer *typer) {
			clear_all_keys();

			// It's unsafe to deallocate typer right now, since it is the caller, but also it has a small
			// memory footprint and it's desireable not to imply that the subclass need call it any more.
			// So shuffle it off into a siding.
			previous_typer_ = std::move(typer_);
			typer_ = nullptr;
		}

		virtual HalfCycles get_typer_delay() { return HalfCycles(0); }
		virtual HalfCycles get_typer_frequency() { return HalfCycles(0); }
		std::unique_ptr<Typer> typer_;

	private:
		std::unique_ptr<Typer> previous_typer_;
		CMApper character_mapper;
};

}

#endif /* Typer_hpp */
