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

		static constexpr uint32_t BaseAddress = 0xb'0000;
		static constexpr auto FontROM = ROM::Name::PCCompatibleMDAFont;

		void set_source(const uint8_t *ram, std::vector<uint8_t> font) {
			outputter_.ram = ram;
			outputter_.font = font;
		}

		void run_for(Cycles cycles) {
			// Input rate is the PIT rate of 1,193,182 Hz.
			// MDA is actually clocked at 16.257 MHz; which is treated internally as 1,806,333.333333333333333... Hz
			//
			// The GCD of those two numbers is... 2. Oh well.

			// I _think_ the MDA's CRTC is clocked at 14/9ths the PIT clock.
			// Do that conversion here.
			full_clock_ += 8'128'500 * cycles.as<int>();
			crtc_.run_for(Cycles(full_clock_ / (596'591 * 9)));
			full_clock_ %= (596'591 * 9);
		}

		template <int address>
		void write(uint8_t value) {
			if constexpr (address & 0x8) {
				outputter_.set_control(value);
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
			if constexpr (address & 0x8) {
				return outputter_.control();
			} else {
				return crtc_.get_register();
			}
		}

		// MARK: - Call-ins for ScanProducer.

		void set_scan_target(Outputs::Display::ScanTarget *scan_target) {
			outputter_.crt.set_scan_target(scan_target);
		}

		Outputs::Display::ScanStatus get_scaled_scan_status() const {
			return outputter_.crt.get_scaled_scan_status() * 596591.0f / 8128500.0f;
		}

	private:
		struct CRTCOutputter {
			CRTCOutputter() :
				crt(882, 9, 370, 3, Outputs::Display::InputDataType::Red2Green2Blue2)
				// TODO: really this should be a Luminance8 and set an appropriate modal tint colour;
				// consider whether that's worth building into the scan target.
			{
//				crt.set_visible_area(Outputs::Display::Rect(0.1072f, 0.1f, 0.842105263157895f, 0.842105263157895f));
				crt.set_display_type(Outputs::Display::DisplayType::RGB);
			}

			void set_control(uint8_t control) {
				// b0: 'high resolution' (probably breaks if not 1)
				// b3: video enable
				// b5: enable blink
				control_ = control;
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
				if(new_state != output_state || count > 882) {
					// (1) flush preexisting state.
					if(count) {
						switch(output_state) {
							case OutputState::Sync:		crt.output_sync(count);		break;
							case OutputState::Border: 	crt.output_blank(count);	break;
							case OutputState::Pixels:
								crt.output_data(count);
								pixels = pixel_pointer = nullptr;
							break;
						}
					}

					// (2) adopt new state.
					output_state = new_state;
					count = 0;
				}

				// Collect pixels if applicable.
				if(output_state == OutputState::Pixels) {
					if(!pixels) {
						pixel_pointer = pixels = crt.begin_data(DefaultAllocationSize);

						// Flush any period where pixels weren't recorded due to back pressure.
						if(pixels && count) {
							crt.output_blank(count);
							count = 0;
						}
					}

					if(pixels) {
						static constexpr uint8_t high_intensity = 0x0d;
						static constexpr uint8_t low_intensity = 0x09;
						static constexpr uint8_t off = 0x00;

						if(state.cursor) {
							pixel_pointer[0] =	pixel_pointer[1] =	pixel_pointer[2] =	pixel_pointer[3] =
							pixel_pointer[4] =	pixel_pointer[5] =	pixel_pointer[6] =	pixel_pointer[7] =
							pixel_pointer[8] =	low_intensity;
						} else {
							const uint8_t attributes = ram[((state.refresh_address << 1) + 1) & 0xfff];
							const uint8_t glyph = ram[((state.refresh_address << 1) + 0) & 0xfff];
							uint8_t row = font[(glyph * 14) + state.row_address];

							const uint8_t intensity = (attributes & 0x08) ? high_intensity : low_intensity;
							uint8_t blank = off;

							// Handle irregular attributes.
							// Cf. http://www.seasip.info/VintagePC/mda.html#memmap
							switch(attributes) {
								case 0x00:	case 0x08:	case 0x80:	case 0x88:
									row = 0;
								break;
								case 0x70:	case 0x78:	case 0xf0:	case 0xf8:
									row ^= 0xff;
									blank = intensity;
								break;
							}

							// Apply blink if enabled.
							if((control_ & 0x20) && (attributes & 0x80) && (state.field_count & 16)) {
								row ^= 0xff;
								blank = (blank == off) ? intensity : off;
							}

							if(((attributes & 7) == 1) && state.row_address == 13) {
								// Draw as underline.
								std::fill(pixel_pointer, pixel_pointer + 9, intensity);
							} else {
								// Draw according to ROM contents, possibly duplicating final column.
								pixel_pointer[0] = (row & 0x80) ? intensity : off;
								pixel_pointer[1] = (row & 0x40) ? intensity : off;
								pixel_pointer[2] = (row & 0x20) ? intensity : off;
								pixel_pointer[3] = (row & 0x10) ? intensity : off;
								pixel_pointer[4] = (row & 0x08) ? intensity : off;
								pixel_pointer[5] = (row & 0x04) ? intensity : off;
								pixel_pointer[6] = (row & 0x02) ? intensity : off;
								pixel_pointer[7] = (row & 0x01) ? intensity : off;
								pixel_pointer[8] = (glyph >= 0xc0 && glyph < 0xe0) ? pixel_pointer[7] : blank;
							}
						}
						pixel_pointer += 9;
					}
				}

				// Advance.
				count += 9;

				// Output pixel row prematurely if storage is exhausted.
				if(output_state == OutputState::Pixels && pixel_pointer == pixels + DefaultAllocationSize) {
					crt.output_data(count);
					count = 0;

					pixels = pixel_pointer = nullptr;
				}
			}
			void perform_bus_cycle_phase2(const Motorola::CRTC::BusState &) {}

			Outputs::CRT::CRT crt;

			enum class OutputState {
				Sync, Pixels, Border
			} output_state = OutputState::Sync;
			int count = 0;

			uint8_t *pixels = nullptr;
			uint8_t *pixel_pointer = nullptr;
			static constexpr size_t DefaultAllocationSize = 720;

			const uint8_t *ram = nullptr;
			std::vector<uint8_t> font;

			uint8_t control_ = 0;
		} outputter_;
		Motorola::CRTC::CRTC6845<CRTCOutputter, Motorola::CRTC::CursorType::MDA> crtc_;

		int full_clock_;
};

}

#endif /* CGA_h */
