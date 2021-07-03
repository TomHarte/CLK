//
//  Nick.hpp
//  Clock Signal
//
//  Created by Thomas Harte on 14/06/2021.
//  Copyright © 2021 Thomas Harte. All rights reserved.
//

#ifndef Nick_hpp
#define Nick_hpp

#include <cstdint>
#include "../../ClockReceiver/ClockReceiver.hpp"
#include "../../Outputs/CRT/CRT.hpp"

namespace Enterprise {

class Nick {
	public:
		Nick(const uint8_t *ram);

		void write(uint16_t address, uint8_t value);
		uint8_t read(uint16_t address);

		void run_for(Cycles);
		Cycles get_time_until_z80_slot(Cycles after_period) const;

		void set_scan_target(Outputs::Display::ScanTarget *scan_target);
		Outputs::Display::ScanStatus get_scaled_scan_status() const;

		/// @returns The amount of time until the next potential change in interrupt output.
		Cycles get_next_sequence_point() const;

		/*!
			@returns The current state of the interrupt line — @c true for active;
				@c false for inactive.
		*/
		inline bool get_interrupt_line() const {
			return interrupt_line_;
		}

		/// Sets the type of output.
		void set_display_type(Outputs::Display::DisplayType);

		/// Gets the type of output.
		Outputs::Display::DisplayType get_display_type() const;

	private:
		Outputs::CRT::CRT crt_;
		const uint8_t *const ram_;

		// CPU-provided state.
		uint8_t line_parameter_control_ = 0xc0;
		uint16_t line_parameter_base_ = 0x0000;
		uint16_t border_colour_ = 0;

		// Ephemerals, related to current video position.
		int horizontal_counter_ = 0;
		uint16_t line_parameter_pointer_ = 0x0000;
		bool should_reload_line_parameters_ = true;
		uint16_t line_data_pointer_[2];
		uint16_t start_line_data_pointer_[2];

		// Current mode line parameters.
		uint8_t lines_remaining_ = 0x00;
		uint8_t two_colour_mask_ = 0xff;
		int left_margin_ = 0, right_margin_ = 0;
		const uint16_t *alt_ind_palettes[4];
		enum class Mode {
			Vsync,
			Pixel,
			Attr,
			CH256,
			CH128,
			CH64,
			Unused,
			LPixel,
		} mode_ = Mode::Vsync;
		bool is_sync_or_pixels_ = false;
		int bpp_ = 0;
		int column_size_ = 0;
		bool interrupt_line_ = true;
		int line_data_per_column_increments_[2] = {0, 0};
		bool vres_ = false;
		bool reload_line_parameter_pointer_ = false;

		// An accumulator for border output regions.
		int border_duration_ = 0;

		// The destination for new pixels.
		static constexpr int allocation_size = 336;
		static_assert((allocation_size % 16) == 0, "Allocation size must be a multiple of 16");
		uint16_t *pixel_pointer_ = nullptr, *allocated_pointer_ = nullptr;

		// Output transitions.
		enum class OutputType {
			Sync, Blank, Pixels, Border, ColourBurst
		};
		void set_output_type(OutputType, bool force_flush = false);
		int output_duration_ = 0;
		OutputType output_type_ = OutputType::Sync;

		// Current palette.
		uint16_t palette_[16]{};

		// The first column with pixels on it; will be either 8 or 10 depending
		// on whether the colour burst is meaningful to the current display type.
		int first_pixel_window_ = 10;

		// Specific outputters.
		template <int bpp, bool is_lpixel> void output_pixel(uint16_t *target, int columns) const;
		template <int bpp, int index_bits> void output_character(uint16_t *target, int columns) const;
		template <int bpp> void output_attributed(uint16_t *target, int columns) const;
};


}

#endif /* Nick_hpp */
