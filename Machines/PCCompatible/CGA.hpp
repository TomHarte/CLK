//
//  CGA.hpp
//  Clock Signal
//
//  Created by Thomas Harte on 05/12/2023.
//  Copyright © 2023 Thomas Harte. All rights reserved.
//

#ifndef CGA_h
#define CGA_h

#include "../../Components/6845/CRTC6845.hpp"
#include "../../Outputs/CRT/CRT.hpp"
#include "../../Machines/Utility/ROMCatalogue.hpp"

namespace PCCompatible {

class CGA {
	public:
		CGA() : crtc_(Motorola::CRTC::Personality::HD6845S, outputter_) {}

		static constexpr uint32_t BaseAddress = 0xb'8000;
		static constexpr auto FontROM = ROM::Name::PCCompatibleCGAFont;

		void set_source(const uint8_t *ram, std::vector<uint8_t> font) {
			outputter_.ram = ram;
			outputter_.font = font;
		}

		void run_for(Cycles cycles) {
			// Input rate is the PIT rate of 1,193,182 Hz.
			// CGA is clocked at the real oscillator rate of 14 times that.
			// But there's also an internal divide by 8 to align to the fetch clock.
			full_clock_ += 7 * cycles.as<int>();

			const int modulo = 4 * outputter_.clock_divider;
			crtc_.run_for(Cycles(full_clock_ / modulo));
			full_clock_ %= modulo;
		}

		template <int address>
		void write(uint8_t value) {
			if constexpr (address & 0x8) {
				outputter_.set_mode(value);
			} else {
				if constexpr (address & 0x1) {
					crtc_.set_register(value);
				} else {
					crtc_.select_register(value);
				}
			}
		}

		template <int address>
		uint8_t read() {
			switch(address) {
				default: 	return crtc_.get_register();
				case 0xa:
					return
						// b3: 1 => in vsync; 0 => not;
						// b2: 1 => light pen switch is off;
						// b1: 1 => positive edge from light pen has set trigger;
						// b0: 1 => safe to write to VRAM now without causing snow.
						(crtc_.get_bus_state().vsync ? 0b1001 : 0b0000) |
						(crtc_.get_bus_state().hsync ? 0b0001 : 0b0000) |
						0b0100;
			}
		}

		// MARK: - Call-ins for ScanProducer.

		void set_scan_target(Outputs::Display::ScanTarget *scan_target) {
			outputter_.crt.set_scan_target(scan_target);
		}

		Outputs::Display::ScanStatus get_scaled_scan_status() const {
			return outputter_.crt.get_scaled_scan_status() * 4.0f / (7.0f * 8.0f);
		}

	private:
		struct CRTCOutputter {
			CRTCOutputter() :
				crt(912, 8, 262, 3, Outputs::Display::InputDataType::Red2Green2Blue2)
			{
//				crt.set_visible_area(Outputs::Display::Rect(0.1072f, 0.1f, 0.842105263157895f, 0.842105263157895f));
				crt.set_display_type(Outputs::Display::DisplayType::RGB);
			}


			void set_mode(uint8_t control) {
				// b5: enable blink
				// b4: 1 => 640x200 graphics
				// b3: video enable
				// b2: 1 => monochrome
				// b1: 1 => 320x200 graphics; 0 => text
				// b0: 1 => 80-column text; 0 => 40
				control_ = control;

				if(control & 0x2) {
					mode_ = (control & 0x10) ? Mode::Pixels640 : Mode::Pixels320;
				} else {
					mode_ = Mode::Text;
				}

				// TODO: I think I need a separate clock divider, which affects the input to the CRTC,
				// and an output multiplier, to set how many pixels are generated per CRTC tick.
				//
				// 640px mode generates 16 pixels per tick, all the other modes generate 8.
				// The clock is divided by 1 in all 80-column text mode, 2 in all other modes.
				clock_divider = 1 + !(control & 0x01);
			}

			uint8_t control() {
				return control_;
			}

			void perform_bus_cycle_phase1(const Motorola::CRTC::BusState &state) {
				// Determine new output state.
				const OutputState new_state =
					(state.hsync | state.vsync) ? OutputState::Sync :
						((state.display_enable && control_&0x08) ? OutputState::Pixels : OutputState::Border);

				// Upon either a state change or just having accumulated too much local time...
				if(new_state != output_state || pixels_divider != clock_divider || count > 912) {
					// (1) flush preexisting state.
					if(count) {
						switch(output_state) {
							case OutputState::Sync:		crt.output_sync(count * clock_divider);		break;
							case OutputState::Border: 	crt.output_blank(count * clock_divider);	break;
							case OutputState::Pixels:	flush_pixels();								break;
						}
					}

					// (2) adopt new state.
					output_state = new_state;
					pixels_divider = clock_divider;
					count = 0;
				}

				// Collect pixels if applicable.
				if(output_state == OutputState::Pixels) {
					if(!pixels) {
						pixel_pointer = pixels = crt.begin_data(DefaultAllocationSize);

						// Flush any period where pixels weren't recorded due to back pressure.
						if(pixels && count) {
							crt.output_blank(count * clock_divider);
							count = 0;
						}
					}

					if(pixels) {
						if(state.cursor) {
							pixel_pointer[0] =	pixel_pointer[1] =	pixel_pointer[2] =	pixel_pointer[3] =
							pixel_pointer[4] =	pixel_pointer[5] =	pixel_pointer[6] =	pixel_pointer[7] =
							pixel_pointer[8] =	0x3f;	// i.e. white.
						} else {
							if(mode_ == Mode::Text) {
								serialise_text(state);
							} else {
								serialise_pixels(state);
							}
						}
						pixel_pointer += 8;
					}
				}

				// Advance.
				count += 8;

				// Output pixel row prematurely if storage is exhausted.
				if(output_state == OutputState::Pixels && pixel_pointer == pixels + DefaultAllocationSize) {
					flush_pixels();
					count = 0;
				}
			}
			void perform_bus_cycle_phase2(const Motorola::CRTC::BusState &) {}

			void flush_pixels() {
				crt.output_data(count * pixels_divider, size_t(count));
				pixels = pixel_pointer = nullptr;
			}

			void serialise_pixels(const Motorola::CRTC::BusState &state) {
				// This is what I think is happenings:
				//
				// Refresh address is still shifted left one and two bytes are fetched, just as if it were
				// character code + attributes except that these are two bytes worth of graphics.
				//
				// Meanwhile, row address is used to invent a 15th address line.
				const uint8_t bitmap = ram[((state.refresh_address << 1) + (state.row_address << 13)) & 0x3fff];
//				const uint8_t bitmap = ram[(state.refresh_address + (state.row_address << 13)) & 0x3fff];

				// Better than nothing...
				pixel_pointer[0] =
				pixel_pointer[1] = (bitmap & 0xc0) >> 6;
				pixel_pointer[2] =
				pixel_pointer[3] = (bitmap & 0x30) >> 4;
				pixel_pointer[4] =
				pixel_pointer[5] = (bitmap & 0x0c) >> 2;
				pixel_pointer[6] =
				pixel_pointer[7] = (bitmap & 0x03) >> 0;
			}

			void serialise_text(const Motorola::CRTC::BusState &state) {
				const uint8_t attributes = ram[((state.refresh_address << 1) + 1) & 0xfff];
				const uint8_t glyph = ram[((state.refresh_address << 1) + 0) & 0xfff];
				const uint8_t row = font[(glyph * 8) + state.row_address];

				uint8_t colours[2] = {
					uint8_t(((attributes & 0x40) >> 1) | ((attributes & 0x20) >> 2) | ((attributes & 0x10) >> 3)),
					uint8_t(((attributes & 0x04) << 3) | ((attributes & 0x02) << 2) | ((attributes & 0x01) << 1)),
				};

				// Apply foreground intensity.
				if(attributes & 0x08) {
					colours[1] |= colours[1] >> 1;
				}

				// Apply blink or background intensity.
				if(control_ & 0x20) {
					if((attributes & 0x80) && (state.field_count & 16)) {
						std::swap(colours[0], colours[1]);
					}
				} else {
					if(attributes & 0x80) {
						colours[0] |= colours[0] >> 1;
					}
				}

				// Draw according to ROM contents.
				pixel_pointer[0] = (row & 0x80) ? colours[1] : colours[0];
				pixel_pointer[1] = (row & 0x40) ? colours[1] : colours[0];
				pixel_pointer[2] = (row & 0x20) ? colours[1] : colours[0];
				pixel_pointer[3] = (row & 0x10) ? colours[1] : colours[0];
				pixel_pointer[4] = (row & 0x08) ? colours[1] : colours[0];
				pixel_pointer[5] = (row & 0x04) ? colours[1] : colours[0];
				pixel_pointer[6] = (row & 0x02) ? colours[1] : colours[0];
				pixel_pointer[7] = (row & 0x01) ? colours[1] : colours[0];
			}

			Outputs::CRT::CRT crt;

			enum class OutputState {
				Sync, Pixels, Border
			} output_state = OutputState::Sync;
			int count = 0;

			uint8_t *pixels = nullptr;
			uint8_t *pixel_pointer = nullptr;
			int pixels_divider = 1;
			static constexpr size_t DefaultAllocationSize = 320;

			const uint8_t *ram = nullptr;
			std::vector<uint8_t> font;

			uint8_t control_ = 0;

			enum class Mode {
				Pixels640, Pixels320, Text,
			} mode_ = Mode::Text;
			int clock_divider = 1;
		} outputter_;
		Motorola::CRTC::CRTC6845<CRTCOutputter, Motorola::CRTC::CursorType::MDA> crtc_;

		int full_clock_;
};

}

#endif /* CGA_h */
