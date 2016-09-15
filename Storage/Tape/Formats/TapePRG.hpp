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
#include <stdint.h>

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
			@param type The type of data the file should contain.
			@throws ErrorBadFormat if this file could not be opened and recognised as the specified type.
		*/
		PRG(const char *file_name);
		~PRG();

		enum {
			ErrorBadFormat
		};

		// implemented to satisfy @c Tape
		bool is_at_end();

	private:
		Pulse virtual_get_next_pulse();
		void virtual_reset();

		FILE *_file;
		uint16_t _load_address;
		uint16_t _length;

		enum FilePhase {
			FilePhaseLeadIn,
			FilePhaseHeader,
			FilePhaseHeaderDataGap,
			FilePhaseData,
			FilePhaseAtEnd
		} _filePhase;
		int _phaseOffset;

		int _bitPhase;
		enum OutputToken {
			Leader,
			Zero,
			One,
			WordMarker,
			EndOfBlock,
			Silence
		} _outputToken;
		void get_next_output_token();
		uint8_t _output_byte;
		uint8_t _check_digit;
		uint8_t _copy_mask;
};

}
}

#endif /* T64_hpp */
