//
//  68000Storage.hpp
//  Clock Signal
//
//  Created by Thomas Harte on 08/03/2019.
//  Copyright © 2019 Thomas Harte. All rights reserved.
//

#ifndef MC68000Storage_h
#define MC68000Storage_h

class ProcessorStorage {
	public:
		ProcessorStorage();

	protected:
		uint32_t data_[8];
		uint32_t address_[7];
		uint32_t stack_pointers_[2];
		uint32_t program_counter_;

		enum class State {
			Reset,
			Normal
		};

		/*!
			A step is a microcycle to perform plus an action to occur afterwards, if any.
		*/
		struct Step {
			Microcycle microcycle;
			enum class Action {
				None
			} action_ = Action::None;
		};

		// Special programs.
		std::vector<Step> reset_program_;

	private:
		enum class DataSize {
			Byte, Word, LongWord
		};
		enum class AddressingMode {
		};

		std::vector<Step> assemble_program(const char *access_pattern);
};

#endif /* MC68000Storage_h */
