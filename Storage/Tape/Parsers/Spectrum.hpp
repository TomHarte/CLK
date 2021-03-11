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

#include <array>
#include <optional>
#include <vector>

namespace Storage {
namespace Tape {
namespace ZXSpectrum {

enum class WaveType {
	// All references to 't-states' below are cycles relative to the
	// ZX Spectrum's 3.5Mhz processor.

	Pilot,	// Nominally 2168 t-states.
	Zero,	// 855 t-states.
	One,	// 1710 t-states.
	Gap,
};

// Formally, there are two other types of wave:
//
//	Sync1,	// 667 t-states.
//	Sync2,	// 735 t-states.
//
// Non-Spectrum machines often just output a plain zero symbol instead of
// a two-step sync; this parser treats anything close enough to a zero
// as a sync.

enum class SymbolType {
	Zero,
	One,
	Pilot,
	Gap,
};

/// A block is anything that follows a period of pilot tone; on a Spectrum that might be a
/// file header or the file contents; on a CPC it might be a file header or a single chunk providing
/// partial file contents. The Enterprise seems broadly to follow the Spectrum but the internal
/// byte structure differs.
struct Block {
	uint8_t type = 0;
};

class Parser: public Storage::Tape::PulseClassificationParser<WaveType, SymbolType> {
	public:
		enum class MachineType {
			ZXSpectrum,
			Enterprise,
			SAMCoupe,
			AmstradCPC
		};
		Parser(MachineType);

		/*!
			Finds the next block from the tape, if any.

			Following this call the tape will be positioned immediately after the byte that indicated the block type —
			in Spectrum-world this seems to be called the flag byte. This call can therefore be followed up with one
			of the get_ methods.
		*/
		std::optional<Block> find_block(const std::shared_ptr<Storage::Tape::Tape> &tape);

		/*!
			Reads the contents of the rest of this block, until the next gap.
		*/
		std::vector<uint8_t> get_block_body(const std::shared_ptr<Storage::Tape::Tape> &tape);

		/*!
			Reads a single byte from the tape, if there is one left, updating the internal checksum.

			The checksum is computed as an exclusive OR of all bytes read.
		*/
		std::optional<uint8_t> get_byte(const std::shared_ptr<Storage::Tape::Tape> &tape);

		/*!
			Seeds the internal checksum.
		*/
		void seed_checksum(uint8_t value = 0x00);

	private:
		const MachineType machine_type_;
		constexpr bool should_flip_bytes() {
			return machine_type_ == MachineType::Enterprise;
		}
		constexpr bool should_detect_speed() {
			return machine_type_ != MachineType::ZXSpectrum;
		}

		void process_pulse(const Storage::Tape::Tape::Pulse &pulse) override;
		void inspect_waves(const std::vector<WaveType> &waves) override;

		uint8_t checksum_ = 0;

		enum class SpeedDetectionPhase {
			WaitingForGap,
			WaitingForPilot,
			CalibratingPilot,
			Done
		} speed_phase_ = SpeedDetectionPhase::Done;

		float too_long_ = 2600.0f;
		float too_short_ = 600.0f;
		float is_pilot_ = 1939.0f;
		float is_one_ = 1282.0f;

		std::array<float, 8> calibration_pulses_;
		size_t calibration_pulse_pointer_ = 0;
};

}
}
}

#endif /* Spectrum_hpp */
