//
//  Spectrum.hpp
//  Clock Signal
//
//  Created by Thomas Harte on 07/03/2021.
//  Copyright © 2021 Thomas Harte. All rights reserved.
//

#ifndef Storage_Tape_Parsers_Spectrum_hpp
#define Storage_Tape_Parsers_Spectrum_hpp

#include "TapeParser.hpp"

namespace Storage {
namespace Tape {
namespace ZXSpectrum {

enum class WaveType {
	// All references to 't-states' below are cycles relative to the
	// ZX Spectrum's 3.5Mhz processor.

	Pilot,	// Nominally 2168 t-states.
	Sync1,	// 667 t-states.
	Sync2,	// 735 t-states.
	Zero,	// 855 t-states.
	One,	// 1710 t-states.
	Gap,
};

enum class SymbolType {
	Pilot,
	Sync,
	Zero,
	One,
	Gap,
};

class Parser: public Storage::Tape::PulseClassificationParser<WaveType, SymbolType> {
	private:
		void process_pulse(const Storage::Tape::Tape::Pulse &pulse) override;
		void inspect_waves(const std::vector<WaveType> &waves) override;
};

}
}
}

#endif /* Spectrum_hpp */
