//
//  PIC.hpp
//  Clock Signal
//
//  Created by Thomas Harte on 21/11/2023.
//  Copyright © 2023 Thomas Harte. All rights reserved.
//

#pragma once

#include "Analyser/Static/PCCompatible/Target.hpp"
#include "Outputs/Log.hpp"

namespace PCCompatible {

// Cf. https://helppc.netcore2k.net/hardware/pic
class PIC {
	using Log = Log::Logger<Log::Source::PIC>;
public:
	template <int address>
	void write(const uint8_t value) {
		if(address) {
			if(config_.word >= 0) {
				switch(config_.word) {
					case 0:
						vector_base_ = value;
					break;
					case 1:
						if(config_.has_fourth_word) {
							// TODO:
							//
							//	(1) slave mask if this is a master;
							//	(2) master interrupt attachment if this is a slave.
						}
						[[fallthrough]];
					case 2:
						auto_eoi_ = value & 2;
					break;
				}

				++config_.word;
				if(config_.word == (config_.has_fourth_word ? 3 : 2)) {
					config_.word = -1;
				}
			} else {
				mask_ = value;
				Log::info().append("Mask set to %02x; requests now %02x", mask_, requests_);
			}
		} else {
			if(value & 0x10) {
				//
				// Initialisation Command Word 1.
				//

				config_.word = 0;
				config_.has_fourth_word = value & 1;

				if(!config_.has_fourth_word) {
					auto_eoi_ = false;
				}

				single_pic_ = value & 2;
				four_byte_vectors_ = value & 4;
				level_triggered_ = value & 8;

				Log::info().append("Level triggered: %d", level_triggered_);
			} else if(value & 0x08) {
				//
				// Operation Control Word 3.
				//

				// b6: 1 => use b5; 0 => ignore.
				// b5: 1 => set special mask; 0 => clear.
				// b2: 1 => poll command issued; 0 => not.
				// b1: 1 => use b0; 0 => ignore.
				// b0: 1 => read IRR on next read; 0 => read ISR.
			} else {
				//
				// Operation Control Word 2.
				//

				// b7, b6, b5:	EOI type.
				// b2, b1, b0:	interrupt level to acknowledge.
				switch(value >> 5) {
					default:
						Log::error().append("PIC: TODO EOI type %d\n", value >> 5);
						[[fallthrough]];
					case 0b010:	// No-op.
					break;

					case 0b001:	// Non-specific EOI.
						awaiting_eoi_ = false;
					break;

					case 0b011: {	// Specific EOI.
						if((value & 3) == eoi_target_) {
							awaiting_eoi_ = false;
						}
					} break;

					// TODO:
					//	0b000 = rotate in auto EOI mode (clear)
					//	0b100 = rotate in auto EOI mode (set)
					//	0b101 = rotate on nonspecific EOI command
					//	0b110 = set primary command
					//	0b111 = rotate on specific EOI command
				}
			}
		}
	}

	template <int address>
	uint8_t read() const {
		if(address) {
			return mask_;
		}

		Log::error().append("Reading address 0");
		return requests_;
	}

	template <int input>
	void apply_edge(const bool final_level) {
		static constexpr uint8_t input_mask = 1 << input;
		const uint8_t new_bit = final_level ? input_mask : 0;

		// Guess: level triggered means the request can be forwarded only so long as the
		// relevant input is actually high. Whereas edge triggered implies capturing state.
		if(level_triggered_) {
			requests_ &= ~input_mask;
		}
		requests_ |= new_bit;

		// TODO: I don't think the above is correct because it defines an edge trigger to be any time that the
		// level is redeclared as 1 but a previous request has been satisfied. So that's not about watching the rising
		// edge of the input as it should be.
		//
		// ... but the code as below causes my XT to fail to boot. So it's possibly incorrect too, in some other way,
		// or else it trigger a latent piece of incorrect behaviour elsewhere.
//		const auto old_levels = levels_;
//		levels_ = (levels_ & ~input_mask) | new_bit;
//
//		if(level_triggered_) {
//			requests_ = (requests_ & ~input_mask) | new_bit;
//		} else {
//			requests_ |= (levels_ ^ old_levels) & new_bit;
//		}

		Log::info().append("%d to %d => requests now %02x", input, final_level, requests_);
	}

	bool pending() const {
		// Per the OSDev Wiki, masking is applied after the fact.
		return (requests_ & ~mask_);	// !awaiting_eoi_ && 
	}

	uint8_t acknowledge() {
		in_service_ = 0x01;
		int id = 0;
		while(!(in_service_ & requests_) && in_service_) {
			in_service_ <<= 1;
			++id;
		}

		if(in_service_) {
			eoi_target_ = id;
			awaiting_eoi_ = !auto_eoi_;
			requests_ &= ~in_service_;
			Log::info().append("Implicitly acknowledging: %d; requests now: %02x", id, requests_);
			return uint8_t(vector_base_ + id);
		}

		// Spurious interrupt.
		return uint8_t(vector_base_ + 7);
	}

private:
	bool single_pic_ = false;
	bool four_byte_vectors_ = false;
	bool level_triggered_ = false;
	bool auto_eoi_ = false;

	uint8_t vector_base_ = 0;
	uint8_t mask_ = 0;
	bool awaiting_eoi_ = false;
	int eoi_target_ = 0;

	uint8_t requests_ = 0;
	uint8_t in_service_ = 0;
	uint8_t levels_ = 0;

	struct ConfgurationState {
		int word;
		bool has_fourth_word;
	} config_;
};

template <Analyser::Static::PCCompatible::Model model>
struct PICs {
	std::array<PIC, is_at(model) ? 2 : 1> pic;
};

}
