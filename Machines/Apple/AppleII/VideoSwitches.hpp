//
//  VideoSwitches.hpp
//  Clock Signal
//
//  Created by Thomas Harte on 31/10/2020.
//  Copyright © 2020 Thomas Harte. All rights reserved.
//

#ifndef VideoSwitches_h
#define VideoSwitches_h

#include "../../../ClockReceiver/ClockReceiver.hpp"
#include "../../../ClockReceiver/DeferredQueue.hpp"
#include "../../ROMMachine.hpp"

namespace Apple {
namespace II {

// Enumerates all Apple II and IIe display modes.
enum class GraphicsMode {
	Text = 0,
	DoubleText,
	HighRes,
	DoubleHighRes,
	LowRes,
	DoubleLowRes,
	FatLowRes
};
constexpr bool is_text_mode(GraphicsMode m) { return m <= GraphicsMode::DoubleText; }
constexpr bool is_double_mode(GraphicsMode m) { return int(m) & 1; }

template <typename TimeUnit> class VideoSwitches {
	public:
		/*!
			Constructs a new instance of VideoSwitches in which changes to relevant switches
			affect the video mode only after @c delay cycles.

			If @c is_iie is true, these switches will set up the character zones for an IIe-esque
			set of potential flashing characters and alternate video modes.
		*/
		VideoSwitches(bool is_iie, TimeUnit delay, std::function<void(TimeUnit)> &&target) : delay_(delay), deferrer_(std::move(target)) {
			character_zones_[0].xor_mask = 0;
			character_zones_[0].address_mask = 0x3f;
			character_zones_[1].xor_mask = 0;
			character_zones_[1].address_mask = 0x3f;
			character_zones_[2].xor_mask = 0;
			character_zones_[2].address_mask = 0x3f;
			character_zones_[3].xor_mask = 0;
			character_zones_[3].address_mask = 0x3f;

			if(is_iie) {
				character_zones_[0].xor_mask =
				character_zones_[2].xor_mask =
				character_zones_[3].xor_mask = 0xff;
				character_zones_[2].address_mask =
				character_zones_[3].address_mask = 0xff;
			}
		}

		/*!
			Advances @c cycles.
		*/
		void run_for(TimeUnit cycles) {
			deferrer_.run_for(cycles);
		}

		/*
			Descriptions for the setters below are taken verbatim from
			the Apple IIe Technical Reference. Addresses are the conventional
			locations within the Apple II memory map. Only those which affect
			video output are implemented here.

			Those registers which don't exist on a II/II+ are marked.
		*/

		/*!
			Setter for ALTCHAR ($C00E/$C00F; triggers on write only):

			* Off: display text using primary character set.
			* On: display text using alternate character set.

			Doesn't exist on a II/II+.
		*/
		void set_alternative_character_set(bool alternative_character_set) {
			external_.alternative_character_set = alternative_character_set;
			deferrer_.defer(delay_, [this, alternative_character_set] {
				internal_.alternative_character_set = alternative_character_set;

				if(alternative_character_set) {
					character_zones_[1].address_mask = 0xff;
					character_zones_[1].xor_mask = 0;
				} else {
					character_zones_[1].address_mask = 0x3f;
					character_zones_[1].xor_mask = flash_mask();
					// The XOR mask is seeded here; it's dynamic, so updated elsewhere.
				}
			});
		}
		bool get_alternative_character_set() const {
			return external_.alternative_character_set;
		}

		/*!
			Setter for 80COL ($C00C/$C00D; triggers on write only).

			* Off: display 40 columns.
			* On: display 80 columns.

			Doesn't exist on a II/II+.
		*/
		void set_80_columns(bool columns_80) {
			external_.columns_80 = columns_80;
			deferrer_.defer(delay_, [this, columns_80] {
				internal_.columns_80 = columns_80;
			});
		}
		bool get_80_columns() const {
			return external_.columns_80;
		}

		/*!
			Setter for 80STORE ($C000/$C001; triggers on write only).

			* Off: cause PAGE2 to select auxiliary RAM.
			* On: cause PAGE2 to switch main RAM areas.

			Doesn't exist on a II/II+.
		*/
		void set_80_store(bool store_80) {
			external_.store_80 = internal_.store_80 = store_80;
		}
		bool get_80_store() const {
			return external_.store_80;
		}

		/*!
			Setter for PAGE2 ($C054/$C055; triggers on read or write).

			* Off: select Page 1.
			* On: select Page 2 or, if 80STORE on, Page 1 in auxiliary memory.

			80STORE doesn't exist on a II/II+; therefore this always selects
			either Page 1 or Page 2 on those machines.
		*/
		void set_page2(bool page2) {
			external_.page2 = internal_.page2 = page2;
		}
		bool get_page2() const {
			return external_.page2;
		}

		/*!
			Setter for TEXT ($C050/$C051; triggers on read or write).

			* Off: display graphics or, if MIXED on, mixed.
			* On: display text.
		*/
		void set_text(bool text) {
			external_.text = text;
			deferrer_.defer(delay_, [this, text] {
				internal_.text = text;
			});
		}
		bool get_text() const {
			return external_.text;
		}

		/*!
			Setter for MIXED ($C052/$C053; triggers on read or write).

			* Off: display only text or only graphics.
			* On: if TEXT off, display text and graphics.
		*/
		void set_mixed(bool mixed) {
			external_.mixed = mixed;
			deferrer_.defer(delay_, [this, mixed] {
				internal_.mixed = mixed;
			});
		}
		bool get_mixed() const {
			return external_.mixed;
		}

		/*!
			Setter for HIRES ($C056/$C057; triggers on read or write).

			* Off: if TEXT off, display low-resolution graphics.
			* On: if TEXT off, display high-resolution or, if DHIRES on, double high-resolution graphics.

			DHIRES doesn't exist on a II/II+; therefore this always selects
			either high- or low-resolution graphics on those machines.

			Despite Apple's documentation, the IIe also supports double low-resolution
			graphics, which are the 80-column analogue to ordinary low-resolution 40-column
			low-resolution graphics.
		*/
		void set_high_resolution(bool high_resolution) {
			external_.high_resolution = high_resolution;
			deferrer_.defer(delay_, [this, high_resolution] {
				internal_.high_resolution = high_resolution;
			});
		}
		bool get_high_resolution() const {
			return external_.high_resolution;
		}

		/*!
			Setter for annunciator 3.

			* On: turn on annunciator 3.
			* Off: turn off annunciator 3.

			This exists on both the II/II+ and the IIe, but has no effect on
			video on the older machines. It's intended to be used on the IIe
			to confirm double-high resolution mode but has side effects in
			selecting mixed mode output and discarding high-resolution
			delay bits.
		*/
		void set_annunciator_3(bool annunciator_3) {
			external_.annunciator_3 = annunciator_3;
			deferrer_.defer(delay_, [this, annunciator_3] {
				internal_.annunciator_3 = annunciator_3;
				high_resolution_mask_ = annunciator_3 ? 0x7f : 0xff;
			});
		}
		bool get_annunciator_3() const {
			return external_.annunciator_3;
		}

		enum class CharacterROM {
			/// The ROM that shipped with both the Apple II and the II+.
			II,
			/// The ROM that shipped with the original IIe.
			IIe,
			/// The ROM that shipped with the Enhanced IIe.
			EnhancedIIe,
			/// The ROM that shipped with the IIgs.
			IIgs
		};

		/// @returns A file-level description of @c rom.
		static ROMMachine::ROM rom_description(CharacterROM rom) {
			const std::string machine_name = "AppleII";
			switch(rom) {
				case CharacterROM::II:
					return ROMMachine::ROM(machine_name, "the basic Apple II character ROM", "apple2-character.rom", 2*1024, 0x64f415c6);

				case CharacterROM::IIe:
					return ROMMachine::ROM(machine_name, "the Apple IIe character ROM", "apple2eu-character.rom", 4*1024, 0x816a86f1);

				default:	// To appease GCC.
				case CharacterROM::EnhancedIIe:
					return ROMMachine::ROM(machine_name, "the Enhanced Apple IIe character ROM", "apple2e-character.rom", 4*1024, 0x2651014d);

				case CharacterROM::IIgs:
					return ROMMachine::ROM(machine_name, "the Apple IIgs character ROM", "apple2gs.chr", 4*1024, 0x91e53cd8);
			}
		}

		/// Set the character ROM for this video output.
		void set_character_rom(const std::vector<uint8_t> &rom) {
			character_rom_ = rom;

			// There's some inconsistency in bit ordering amongst the common ROM dumps;
			// detect that based arbitrarily on the second line of the $ graphic and
			// ensure consistency.
			if(character_rom_[0x121] == 0x3c || character_rom_[0x122] == 0x3c) {
				for(auto &graphic : character_rom_) {
					graphic =
						((graphic & 0x01) ? 0x40 : 0x00) |
						((graphic & 0x02) ? 0x20 : 0x00) |
						((graphic & 0x04) ? 0x10 : 0x00) |
						((graphic & 0x08) ? 0x08 : 0x00) |
						((graphic & 0x10) ? 0x04 : 0x00) |
						((graphic & 0x20) ? 0x02 : 0x00) |
						((graphic & 0x40) ? 0x01 : 0x00);
				}
			}
		}

	protected:
		GraphicsMode graphics_mode(int row) const {
			if(
				internal_.text ||
				(internal_.mixed && row >= 160 && row < 192)
			) return internal_.columns_80 ? GraphicsMode::DoubleText : GraphicsMode::Text;
			if(internal_.high_resolution) {
				return (internal_.annunciator_3 && internal_.columns_80) ? GraphicsMode::DoubleHighRes : GraphicsMode::HighRes;
			} else {
				if(internal_.columns_80) return GraphicsMode::DoubleLowRes;
				if(internal_.annunciator_3) return GraphicsMode::FatLowRes;
				return GraphicsMode::LowRes;
			}
		}

		int video_page() const {
			return (internal_.store_80 || !internal_.page2) ? 0 : 1;
		}

		uint16_t get_row_address(int row) const {
			const int character_row = row >> 3;
			const int pixel_row = row & 7;
			const uint16_t row_address = uint16_t((character_row >> 3) * 40 + ((character_row&7) << 7));

			const GraphicsMode pixel_mode = graphics_mode(row);
			return ((pixel_mode == GraphicsMode::HighRes) || (pixel_mode == GraphicsMode::DoubleHighRes)) ?
				uint16_t(((video_page()+1) * 0x2000) + row_address + ((pixel_row&7) << 10)) :
				uint16_t(((video_page()+1) * 0x400) + row_address);
		}

		/*!
			Should be called by subclasses at the end of each line of the display;
			this gives the base class a peg on which to hang flashing-character updates.
		*/
		void did_end_line() {
			// Update character set flashing; flashing is applied only when the alternative
			// character set is not selected.
			flash_ = (flash_ + 1) % (2 * flash_length);
			character_zones_[1].xor_mask = flash_mask() * !internal_.alternative_character_set;
		}

	private:
		// Maintain a DeferredQueue for delayed mode switches.
		const TimeUnit delay_;
		DeferredQueuePerformer<TimeUnit> deferrer_;

		struct Switches {
			bool alternative_character_set = false;
			bool columns_80 = false;
			bool store_80 = false;
			bool page2 = false;
			bool text = true;
			bool mixed = false;
			bool high_resolution = false;
			bool annunciator_3 = false;
		} external_, internal_;

		int flash_length = 8406;
		int flash_ = 0;
		uint8_t flash_mask() const {
			return uint8_t((flash_ / flash_length) * 0xff);
		}

	protected:

		// Describes the current text mode mapping from in-memory character index
		// to output character; subclasses should:
		//
		//	(i)		use the top two-bits of the character code to index character_zones_;
		//	(ii)	apply the address_mask to the character code in order to get a character
		//			offset into the character ROM; and
		//	(iii)	apply the XOR mask to the output of the character ROM.
		//
		// By this means they will properly handle the limited character sets of Apple IIs
		// prior to the IIe as well as the IIe and onward's alternative character set toggle.
		struct CharacterMapping {
			uint8_t address_mask;
			uint8_t xor_mask;
		};
		CharacterMapping character_zones_[4];

		// A mask that should be applied to high-resolution graphics bytes before output;
		// it acts to retain or remove the top bit, affecting whether the half-pixel delay
		// bit is effective. On a IIe it's toggleable, on early Apple IIs it doesn't exist.
		uint8_t high_resolution_mask_ = 0xff;

		// This holds a copy of the character ROM. The regular character
		// set is assumed to be in the first 64*8 bytes; the alternative
		// is in the 128*8 bytes after that.
		std::vector<uint8_t> character_rom_;
};

}
}

#endif /* VideoSwitches_h */
