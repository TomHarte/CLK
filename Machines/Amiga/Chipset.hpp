//
//  Chipset.hpp
//  Clock Signal
//
//  Created by Thomas Harte on 22/07/2021.
//  Copyright © 2021 Thomas Harte. All rights reserved.
//

#ifndef Chipset_hpp
#define Chipset_hpp

#include <cstddef>
#include <cstdint>

#include "../../Processors/68000/68000.hpp"
#include "../../Outputs/CRT/CRT.hpp"

#include "Blitter.hpp"

namespace Amiga {

class Chipset {
	public:
		Chipset(uint16_t *ram, size_t size);

		/// @returns The duration from now until the beginning of the next
		/// available CPU slot for accessing chip memory.
		HalfCycles time_until_cpu_slot();

		struct Changes {
			int hsyncs = 0;
			int vsyncs = 0;
			int interrupt_level = 0;
			HalfCycles duration;
		};

		/// Advances the stated amount of time.
		Changes run_for(HalfCycles);

		/// Advances to the next available CPU slot.
		Changes run_until_cpu_slot();

		/// Performs the provided microcycle, which the caller guarantees to be a memory access.
		void perform(const CPU::MC68000::Microcycle &);

		/// Provides the chipset's current interrupt level.
		int get_interrupt_level() {
			return interrupt_level_;
		}

		// The standard CRT set.
		void set_scan_target(Outputs::Display::ScanTarget *scan_target);
		Outputs::Display::ScanStatus get_scaled_scan_status() const;
		void set_display_type(Outputs::Display::DisplayType);
		Outputs::Display::DisplayType get_display_type() const;

	private:
		// MARK: - Interrupts.

		uint16_t interrupt_enable_ = 0;
		uint16_t interrupt_requests_ = 0;
		int interrupt_level_ = 0;

		void update_interrupts();

		// MARK: - Scheduler.

		template <bool stop_on_cpu> Changes run(HalfCycles duration = HalfCycles());
		template <bool stop_on_cpu> int advance_slots(int, int);
		template <int cycle, bool stop_if_cpu> bool perform_cycle();
		template <int cycle> void output();

		// MARK: - DMA Control, Scheduler and Blitter.

		uint16_t dma_control_ = 0;
		Blitter blitter_;

		// MARK: - Sprites.

		struct Sprite {
			void set_pointer(int shift, uint16_t value);
			void set_start_position(uint16_t value);
			void set_stop_and_control(uint16_t value);
			void set_image_data(int slot, uint16_t value);
		} sprites_[8];

		// MARK: - Raster.

		int line_cycle_ = 0, y_ = 0;
		int line_length_ = 227;
		int frame_height_ = 312;
		int vertical_blank_height_ = 29;

		uint16_t display_window_start_[2] = {0, 0};
		uint16_t display_window_stop_[2] = {0, 0};
		uint16_t fetch_window_[2] = {0, 0};

		// MARK: - Copper.

		class Copper {
			public:
				Copper(Chipset &chipset, uint16_t *ram, size_t size) : chipset_(chipset), ram_(ram), ram_mask_(uint32_t(size - 1)) {}

				/// Offers a DMA slot to the Copper, specifying the current beam position.
				///
				/// @returns @c true if the slot was used; @c false otherwise.
				bool advance(uint16_t position);

				/// Forces a reload of address @c id (i.e. 0 or 1) and restarts the Copper.
				void reload(int id) {
					address_ = addresses_[id] >> 1;
					state_ = State::FetchFirstWord;
				}

				/// Writes the word @c value to the address register @c id, shifting it by @c shift (0 or 16) first.
				template <int id, int shift> void set_address(uint16_t value) {
					addresses_[id] = (addresses_[id] & (0xffff'0000 >> shift)) | uint32_t(value << shift);
				}

				/// Sets the Copper control word.
				void set_control(uint16_t c) {
					control_ = c;
				}

				/// Forces the Copper into the stopped state.
				void stop() {
					state_ = State::Stopped;
				}

			private:
				Chipset &chipset_;

				uint32_t address_ = 0;
				uint32_t addresses_[2]{};
				uint16_t control_ = 0;

				enum class State {
					FetchFirstWord, FetchSecondWord, Waiting, Stopped,
				} state_ = State::Stopped;
				bool skip_next_ = false;
				uint16_t instruction_[2]{};
				uint16_t position_mask_ = 0xffff;

				uint16_t *ram_ = nullptr;
				uint32_t ram_mask_ = 0;
		} copper_;
		friend Copper;

		// MARK: - Pixel output.

		Outputs::CRT::CRT crt_;
		uint16_t palette_[32]{};
};

}

#endif /* Chipset_hpp */
