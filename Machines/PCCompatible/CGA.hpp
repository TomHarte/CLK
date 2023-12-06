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
			switch(address) {
				case 0:	case 2:	case 4:	case 6:
					crtc_.select_register(value);
				break;
				case 1:	case 3:	case 5:	case 7:
					crtc_.set_register(value);
				break;

				case 0x8:	outputter_.set_mode(value);		break;
				case 0x9:	outputter_.set_colours(value);	break;
			}
		}

		template <int address>
		uint8_t read() {
			switch(address) {
				case 1:	case 3:	case 5:	case 7:
					return crtc_.get_register();

				case 0xa:
					return
						// b3: 1 => in vsync; 0 => not;
						// b2: 1 => light pen switch is off;
						// b1: 1 => positive edge from light pen has set trigger;
						// b0: 1 => safe to write to VRAM now without causing snow.
						(crtc_.get_bus_state().vsync ? 0b1001 : 0b0000) |
						(crtc_.get_bus_state().hsync ? 0b0001 : 0b0000) |
						0b0100;

				default: return 0xff;
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
			enum class OutputState {
				Sync, Pixels, Border, ColourBurst
			};

			CRTCOutputter() :
				crt(910, 8, Outputs::Display::Type::NTSC60, Outputs::Display::InputDataType::Red2Green2Blue2)
			{
//				crt.set_visible_area(Outputs::Display::Rect(0.1072f, 0.1f, 0.842105263157895f, 0.842105263157895f));
				crt.set_display_type(Outputs::Display::DisplayType::CompositeColour);	// TODO: needs to be a user option.
			}

			void set_mode(uint8_t control) {
				// b5: enable blink
				// b4: 1 => 640x200 graphics
				// b3: video enable
				// b2: 1 => monochrome
				// b1: 1 => 320x200 graphics; 0 => text
				// b0: 1 => 80-column text; 0 => 40

				control_ = control;	// To capture blink, monochrome and video enable bits.

				if(control & 0x2) {
					mode_ = (control & 0x10) ? Mode::Pixels640 : Mode::Pixels320;
					pixels_per_tick = (mode_ == Mode::Pixels640) ? 16 : 8;
				} else {
					mode_ = Mode::Text;
					pixels_per_tick = 8;
				}
				clock_divider = 1 + !(control & 0x01);
			}

			void set_colours(uint8_t value) {
				if(value & 0x20) {
					palette[0] = 0b00'00'00;
					palette[1] = 0b00'10'10;
					palette[2] = 0b10'00'10;
					palette[3] = 0b10'10'10;
				} else {
					palette[0] = 0b00'00'00;
					palette[1] = 0b00'10'00;
					palette[2] = 0b10'00'00;
					palette[3] = 0b10'10'00;	// TODO: brown, here and elsewhere.
				}
			}

			uint8_t control() {
				return control_;
			}

			void update_hsync(bool new_hsync) {
				if(new_hsync == previous_hsync) {
					cycles_since_hsync += clock_divider;
				} else {
					cycles_since_hsync = 0;
					previous_hsync = new_hsync;
				}
			}

			OutputState implied_state(const Motorola::CRTC::BusState &state) const {
				OutputState new_state;

				if(state.hsync || state.vsync) {
					new_state = OutputState::Sync;
				} else if(!state.display_enable || !(control_&0x08)) {
					new_state = OutputState::Border;

					// TODO: I'm pretty sure this isn't correct for colour burst positioning, though
					// it happens to fool the particular CRT I've implemented.
					if(!(control_&4) && cycles_since_hsync <= 4) {
						new_state = OutputState::ColourBurst;
					}
				} else {
					new_state = OutputState::Pixels;
				}

				return new_state;
			}

			void perform_bus_cycle_phase1(const Motorola::CRTC::BusState &state) {
				// Determine new output state.
				update_hsync(state.hsync);
				const OutputState new_state = implied_state(state);

				// Upon either a state change or just having accumulated too much local time...
				if(new_state != output_state || active_pixels_per_tick != pixels_per_tick || active_clock_divider != clock_divider || count > 912) {
					// (1) flush preexisting state.
					if(count) {
						switch(output_state) {
							case OutputState::Sync:			crt.output_sync(count * active_clock_divider);				break;
							case OutputState::Border: 		crt.output_blank(count * active_clock_divider);				break;
							case OutputState::ColourBurst:	crt.output_colour_burst(count * active_clock_divider, 0);	break;
							case OutputState::Pixels:		flush_pixels();												break;
						}
					}

					// (2) adopt new state.
					output_state = new_state;
					active_pixels_per_tick = pixels_per_tick;
					active_clock_divider = clock_divider;
					count = 0;
				}

				// Collect pixels if applicable.
				if(output_state == OutputState::Pixels) {
					if(!pixels) {
						pixel_pointer = pixels = crt.begin_data(DefaultAllocationSize);

						// Flush any period where pixels weren't recorded due to back pressure.
						if(pixels && count) {
							crt.output_blank(count * active_clock_divider);
							count = 0;
						}
					}

					if(pixels) {
						if(state.cursor) {
							std::fill(pixel_pointer, pixel_pointer + pixels_per_tick, 0x3f); // i.e. white.
						} else {
							if(mode_ == Mode::Text) {
								serialise_text(state);
							} else {
								serialise_pixels(state);
							}
						}
						pixel_pointer += active_pixels_per_tick;
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
				crt.output_data(count * active_clock_divider, size_t((count * active_pixels_per_tick) / 8));
				pixels = pixel_pointer = nullptr;
			}

			void serialise_pixels(const Motorola::CRTC::BusState &state) {
				// This is what I think is happenings:
				//
				// Refresh address is still shifted left one and two bytes are fetched, just as if it were
				// character code + attributes except that these are two bytes worth of graphics.
				//
				// Meanwhile, row address is used to invent a 15th address line.
				const auto base_address = ((state.refresh_address << 1) + (state.row_address << 13)) & 0x3fff;
				const uint8_t bitmap[] = {
					ram[base_address],
					ram[base_address + 1],
				};

				if(mode_ == Mode::Pixels320) {
					pixel_pointer[0] = palette[(bitmap[0] & 0xc0) >> 6];
					pixel_pointer[1] = palette[(bitmap[0] & 0x30) >> 4];
					pixel_pointer[2] = palette[(bitmap[0] & 0x0c) >> 2];
					pixel_pointer[3] = palette[(bitmap[0] & 0x03) >> 0];
					pixel_pointer[4] = palette[(bitmap[1] & 0xc0) >> 6];
					pixel_pointer[5] = palette[(bitmap[1] & 0x30) >> 4];
					pixel_pointer[6] = palette[(bitmap[1] & 0x0c) >> 2];
					pixel_pointer[7] = palette[(bitmap[1] & 0x03) >> 0];
				} else {
					pixel_pointer[0x0] = palette[(bitmap[0] & 0x80) >> 7];
					pixel_pointer[0x1] = palette[(bitmap[0] & 0x40) >> 6];
					pixel_pointer[0x2] = palette[(bitmap[0] & 0x20) >> 5];
					pixel_pointer[0x3] = palette[(bitmap[0] & 0x10) >> 4];
					pixel_pointer[0x4] = palette[(bitmap[0] & 0x08) >> 3];
					pixel_pointer[0x5] = palette[(bitmap[0] & 0x04) >> 2];
					pixel_pointer[0x6] = palette[(bitmap[0] & 0x02) >> 1];
					pixel_pointer[0x7] = palette[(bitmap[0] & 0x01) >> 0];
					pixel_pointer[0x8] = palette[(bitmap[1] & 0x80) >> 7];
					pixel_pointer[0x9] = palette[(bitmap[1] & 0x40) >> 6];
					pixel_pointer[0xa] = palette[(bitmap[1] & 0x20) >> 5];
					pixel_pointer[0xb] = palette[(bitmap[1] & 0x10) >> 4];
					pixel_pointer[0xc] = palette[(bitmap[1] & 0x08) >> 3];
					pixel_pointer[0xd] = palette[(bitmap[1] & 0x04) >> 2];
					pixel_pointer[0xe] = palette[(bitmap[1] & 0x02) >> 1];
					pixel_pointer[0xf] = palette[(bitmap[1] & 0x01) >> 0];
				}
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
			static constexpr size_t DefaultAllocationSize = 320;

			// Current output stream.
			uint8_t *pixels = nullptr;
			uint8_t *pixel_pointer = nullptr;
			int active_pixels_per_tick = 8;
			int active_clock_divider = 1;

			// Source data.
			const uint8_t *ram = nullptr;
			std::vector<uint8_t> font;

			// CRTC state tracking, for CRT serialisation.
			OutputState output_state = OutputState::Sync;
			int count = 0;

			bool previous_hsync = false;
			int cycles_since_hsync = 0;

			// Current Programmer-set parameters.
			int clock_divider = 1;
			int pixels_per_tick = 8;
			uint8_t control_ = 0;
			enum class Mode {
				Pixels640, Pixels320, Text,
			} mode_ = Mode::Text;

			uint8_t palette[4];

		} outputter_;
		Motorola::CRTC::CRTC6845<CRTCOutputter, Motorola::CRTC::CursorType::MDA> crtc_;

		int full_clock_;
};

}

#endif /* CGA_h */
