//
//  SpectrumTAP.hpp
//  Clock Signal
//
//  Created by Thomas Harte on 19/03/2021.
//  Copyright © 2021 Thomas Harte. All rights reserved.
//

#ifndef SpectrumTAP_hpp
#define SpectrumTAP_hpp

#include "../Tape.hpp"
#include "../../FileHolder.hpp"

#include <cstdint>
#include <string>

namespace Storage {
namespace Tape {

/*!
	Provides a @c Tape containing an Spectrum-format tape image, which contains a series of
	header and data blocks.
*/
class ZXSpectrumTAP: public Tape {
	public:
		/*!
			Constructs a @c ZXSpectrumTAP containing content from the file with name @c file_name.

			@throws ErrorNotZXSpectrumTAP if this file could not be opened and recognised as a valid Spectrum-format TAP.
		*/
		ZXSpectrumTAP(const std::string &file_name);

		enum {
			ErrorNotZXSpectrumTAP
		};

	private:
		Storage::FileHolder file_;

		uint16_t block_length_ = 0;

		// Implemented to satisfy @c Tape.
		bool is_at_end() override;
		void virtual_reset() override;
		Pulse virtual_get_next_pulse() override;
};


}
}

#endif /* SpectrumTAP_hpp */
