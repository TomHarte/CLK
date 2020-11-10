//
//  AuxiliaryMemorySwitches.hpp
//  Clock Signal
//
//  Created by Thomas Harte on 22/10/2020.
//  Copyright © 2020 Thomas Harte. All rights reserved.
//

#ifndef AuxiliaryMemorySwitches_h
#define AuxiliaryMemorySwitches_h

namespace Apple {
namespace II {

/*!
	Models the auxiliary memory soft switches, added as of the Apple IIe, which allow access to the auxiliary 64kb of RAM and to
	the additional almost-4kb of ROM.

	Relevant memory accesses should be fed to this class; it'll call:
		* machine.set_main_paging() if anything in the 'main' state changes, i.e. the lower 48kb excluding the zero and stack pages;
		* machine.set_card_state() if anything changes with where ROM should appear rather than cards in the $Cxxx range; and
		* machine.set_zero_page_paging() if the selection of the lowest two pages of RAM changes.

	Implementation observation: as implemented on the IIe, the zero page setting also affects what happens in the language card area.
*/
template <typename Machine> class AuxiliaryMemorySwitches {
	public:
		static constexpr bool Auxiliary = true;
		static constexpr bool Main = false;
		static constexpr bool ROM = true;
		static constexpr bool Card = false;

		/// Describes banking state between $0200 and $BFFF.
		struct MainState {
			struct Region {
				/// @c true indicates auxiliary memory should be read from in this region; @c false indicates that main memory should be used.
				bool read = false;
				/// @c true indicates auxiliary memory should be written to in this region; @c false indicates that main memory should be used.
				bool write = false;
			};

			/// Describes banking state in the ranges $0200–$3FFF, $0800–$1FFF and $4000–$BFFF.
			Region base;
			/// Describes banking state in the range $0400–$7FFF.
			Region region_04_08;
			/// Describes banking state in the range $2000–$3FFF.
			Region region_20_40;

			bool operator != (const MainState &rhs) const {
				return
					base.read != rhs.base.read || base.write != rhs.base.write ||
					region_04_08.read != rhs.region_04_08.read || region_04_08.write != rhs.region_04_08.write ||
					region_20_40.read != rhs.region_20_40.read || region_20_40.write != rhs.region_20_40.write;
			}
		};

		/// Describes banking state between $C100 and $Cfff.
		struct CardState {
			/// @c true indicates that the built-in ROM should appear from $C100 to $C2FF @c false indicates that cards should service those accesses.
			bool region_C1_C3 = false;
			/// @c true indicates that the built-in ROM should appear from $C300 to $C3FF; @c false indicates that cards should service those accesses.
			bool region_C3 = false;
			/// @c true indicates that the built-in ROM should appear from $C400 to $C7FF; @c false indicates that cards should service those accesses.
			bool region_C4_C8 = false;
			/// @c true indicates that the built-in ROM should appear from $C800 to $CFFF; @c false indicates that cards should service those accesses.
			bool region_C8_D0 = false;

			bool operator != (const CardState &rhs) const {
				return
					region_C1_C3 != rhs.region_C1_C3 ||
					region_C3 != rhs.region_C3 ||
					region_C4_C8 != rhs.region_C4_C8 ||
					region_C8_D0 != rhs.region_C8_D0;
			}
		};

		/// Descibes banking state between $0000 and $01ff; @c true indicates that auxiliary memory should be used; @c false indicates main memory.
		using ZeroState = bool;

		/// Returns raw switch state for all switches that affect banking, even if they're logically video switches.
		struct SwitchState {
			bool read_auxiliary_memory = false;
			bool write_auxiliary_memory = false;

			bool internal_CX_rom = false;
			bool slot_C3_rom = false;
			bool internal_C8_rom = false;

			bool store_80 = false;
			bool alternative_zero_page = false;
			bool video_page_2 = false;
			bool high_resolution = false;
		};

		AuxiliaryMemorySwitches(Machine &machine) : machine_(machine) {}

		/// Used by an owner to forward, at least, any access in the range $C000 to $C00B,
		/// in $C054 to $C058, or in the range $C300 to $CFFF. Safe to call for any [16-bit] address.
		void access(uint16_t address, bool is_read) {
			if(address >= 0xc300 && address < 0xd000) {
				switches_.internal_C8_rom |= ((address >> 8) == 0xc3) && !switches_.slot_C3_rom;
				switches_.internal_C8_rom &= (address != 0xcfff);
				set_card_paging();
				return;
			}

			if(address < 0xc000 || address >= 0xc058) return;

			switch(address) {
				default: break;

				case 0xc000: case 0xc001:
					if(!is_read) {
						switches_.store_80 = address & 1;
						set_main_paging();
					}
				break;

				case 0xc002: case 0xc003:
					if(!is_read) {
						switches_.read_auxiliary_memory = address & 1;
						set_main_paging();
					}
				break;

				case 0xc004: case 0xc005:
					if(!is_read) {
						switches_.write_auxiliary_memory = address & 1;
						set_main_paging();
					}
				break;

				case 0xc006: case 0xc007:
					if(!is_read) {
						switches_.internal_CX_rom = address & 1;
						set_card_paging();
					}
				break;

				case 0xc008: case 0xc009:
					if(!is_read && switches_.alternative_zero_page != bool(address & 1)) {
						switches_.alternative_zero_page = address & 1;
						set_zero_page_paging();
					}
				break;

				case 0xc00a: case 0xc00b:
					if(!is_read) {
						switches_.slot_C3_rom = address & 1;
						set_card_paging();
					}
				break;

				case 0xc054: case 0xc055:
					switches_.video_page_2 = address & 1;
					set_main_paging();
				break;

				case 0xc056: case 0xc057:
					switches_.high_resolution = address & 1;
					set_main_paging();
				break;
			}
		}

		/// Provides part of the IIgs interface.
		void set_state(uint8_t value) {
			switches_.alternative_zero_page = value & 0x80;
			switches_.video_page_2 = value & 0x40;
			switches_.read_auxiliary_memory = value & 0x20;
			switches_.write_auxiliary_memory = value & 0x10;
			switches_.internal_CX_rom = value & 0x01;

			set_main_paging();
			set_zero_page_paging();
			set_card_paging();
		}

		uint8_t get_state() const {
			return
				(switches_.alternative_zero_page ? 0x80 : 0x00) |
				(switches_.video_page_2 ? 0x40 : 0x00) |
				(switches_.read_auxiliary_memory ? 0x20 : 0x00) |
				(switches_.write_auxiliary_memory ? 0x10 : 0x00) |
				(switches_.internal_CX_rom ? 0x01 : 0x00);
		}

		const MainState &main_state() const {
			return main_state_;
		}

		const CardState &card_state() const {
			return card_state_;
		}

		const ZeroState zero_state() const {
			return switches_.alternative_zero_page;
		}

		const SwitchState switches() const {
			return switches_;
		}

	private:
		Machine &machine_;
		SwitchState switches_;

		MainState main_state_;
		void set_main_paging() {
			const auto previous_state = main_state_;

			// The two appropriately named switches provide the base case.
			main_state_.base.read = switches_.read_auxiliary_memory;
			main_state_.base.write = switches_.write_auxiliary_memory;

			if(switches_.store_80) {
				// If store 80 is set, use the page 2 flag for the lower carve out;
				// if both store 80 and high resolution are set, use the page 2 flag for both carve outs.
				main_state_.region_04_08.read = main_state_.region_04_08.write = switches_.video_page_2;

				if(switches_.high_resolution) {
					main_state_.region_20_40.read = main_state_.region_20_40.write = switches_.video_page_2;
				} else {
					main_state_.region_20_40 = main_state_.base;
				}
			} else {
				main_state_.region_04_08 = main_state_.region_20_40 = main_state_.base;
			}

			if(previous_state != main_state_) {
				machine_.set_main_paging();
			}
		}

		CardState card_state_;
		void set_card_paging() {
			const auto previous_state = card_state_;

			// By default apply the CX switch through to $C7FF.
			card_state_.region_C1_C3 = card_state_.region_C4_C8 = switches_.internal_CX_rom;

			// Allow the C3 region to be switched to internal ROM in isolation even if the rest of the
			// first half of the CX region is diabled, if its specific switch is also disabled.
			if(!switches_.internal_CX_rom && !switches_.slot_C3_rom) {
				card_state_.region_C3 = true;
			} else {
				card_state_.region_C3 = card_state_.region_C1_C3;
			}

			// Apply the CX switch to $C800+, but also allow the C8 switch to select that region in isolation.
			card_state_.region_C8_D0 = switches_.internal_CX_rom || switches_.internal_C8_rom;

			if(previous_state != card_state_) {
				machine_.set_card_paging();
			}
		}

		void set_zero_page_paging() {
			// Believe it or not, the zero page is just set or cleared by a single flag.
			// As though life were rational.
			machine_.set_zero_page_paging();
		}
};

}
}

#endif /* AuxiliaryMemorySwitches_h */
