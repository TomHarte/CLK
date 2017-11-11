//
//  TapePRG.hpp
//  Clock Signal
//
//  Created by Thomas Harte on 14/08/2016.
//  Copyright © 2016 Thomas Harte. All rights reserved.
//

#ifndef Storage_Tape_PRG_hpp
#define Storage_Tape_PRG_hpp

#include "../Tape.hpp"
#include "../../FileHolder.hpp"
#include <cstdint>

namespace Storage {
namespace Tape {

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
		PRG(const char *file_name);

		enum {
			ErrorBadFormat
		};

		// implemented to satisfy @c Tape
		bool is_at_end();

	private:
		FileHolder file_;
		Pulse virtual_get_next_pulse();
		void virtual_reset();

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
};

}
}

#endif /* T64_hpp */
