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
		};

		/// Advances the stated amount of time.
		Changes run_for(HalfCycles);

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

		// MARK: - DMA Control and Blitter.

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

		int x_ = 0, y_ = 0;
		int line_length_ = 227;
		int frame_height_ = 312;
		int vertical_blank_height_ = 29;

		uint16_t display_window_start_[2] = {0, 0};
		uint16_t display_window_stop_[2] = {0, 0};
		uint16_t fetch_window_[2] = {0, 0};

		// MARK: - Copper.

		uint16_t copper_address_ = 0;

		// MARK: - Pixel output.

		Outputs::CRT::CRT crt_;
};

}

#endif /* Chipset_hpp */
