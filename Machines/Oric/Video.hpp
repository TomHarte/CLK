//
//  Video.hpp
//  Clock Signal
//
//  Created by Thomas Harte on 12/10/2016.
//  Copyright 2016 Thomas Harte. All rights reserved.
//

#ifndef Machines_Oric_Video_hpp
#define Machines_Oric_Video_hpp

#include "../../Outputs/CRT/CRT.hpp"
#include "../../ClockReceiver/ClockReceiver.hpp"

#include <cstdint>
#include <memory>
#include <vector>

namespace Oric {

class VideoOutput {
	public:
		VideoOutput(uint8_t *memory);
		void set_colour_rom(const std::vector<uint8_t> &colour_rom);

		void run_for(const Cycles cycles);

		void set_scan_target(Outputs::Display::ScanTarget *scan_target);
		void set_display_type(Outputs::Display::DisplayType display_type);
		Outputs::Display::DisplayType get_display_type() const;
		Outputs::Display::ScanStatus get_scaled_scan_status() const;

		void register_crt_frequency_mismatch();

	private:
		uint8_t *ram_;
		Outputs::CRT::CRT crt_;
		Outputs::CRT::CRTFrequencyMismatchWarner<VideoOutput> frequency_mismatch_warner_;
		bool crt_is_60Hz_ = false;

		void update_crt_frequency();

		// Counters and limits.
		int counter_ = 0, frame_counter_ = 0;
		int v_sync_start_position_, v_sync_end_position_, counter_period_;

		// Output target and device.
		uint8_t *rgb_pixel_target_ = nullptr;
		uint32_t *composite_pixel_target_ = nullptr;
		uint32_t colour_forms_[8];
		Outputs::Display::InputDataType data_type_;

		// Registers.
		uint8_t ink_, paper_;

		int character_set_base_address_ = 0xb400;
		inline void set_character_set_base_address();

		bool is_graphics_mode_ = false;
		bool next_frame_is_sixty_hertz_ = false;
		bool use_alternative_character_set_;
		bool use_double_height_characters_;
		bool blink_text_;
};

}

#endif /* Video_hpp */
