//
//  AmstradCPC.cpp
//  Clock Signal
//
//  Created by Thomas Harte on 30/07/2017.
//  Copyright © 2017 Thomas Harte. All rights reserved.
//

#include "AmstradCPC.hpp"

#include "../../Processors/Z80/Z80.hpp"

#include "../../Components/8255/i8255.hpp"
#include "../../Components/AY38910/AY38910.hpp"
#include "../../Components/6845/CRTC6845.hpp"

using namespace AmstradCPC;

class CRTCBusHandler {
	public:
		CRTCBusHandler(uint8_t *ram) :
			cycles_(0),
			was_enabled_(false),
			was_sync_(false),
			pixel_data_(nullptr),
			pixel_pointer_(nullptr),
			was_hsync_(false),
			ram_(ram),
			interrupt_counter_(0),
			interrupt_request_(false),
			pixel_divider_(1) {}

		inline void perform_bus_cycle(const Motorola::CRTC::BusState &state) {
			bool is_sync = state.hsync || state.vsync;

			// if a transition between sync/border/pixels just occurred, announce it
			if(state.display_enable != was_enabled_ || is_sync != was_sync_) {
				if(was_sync_) {
					crt_->output_sync(cycles_ * 16);
				} else {
					if(was_enabled_) {
						if(cycles_) {
							crt_->output_data(cycles_ * 16, pixel_divider_);
							pixel_pointer_ = pixel_data_ = nullptr;
						}
					} else {
						uint8_t *colour_pointer = (uint8_t *)crt_->allocate_write_area(1);
						if(colour_pointer) *colour_pointer = border_;
						crt_->output_level(cycles_ * 16);
					}
				}

				cycles_ = 0;
				was_sync_ = is_sync;
				was_enabled_ = state.display_enable;
			}

			// increment cycles since state changed
			cycles_++;

			// collect some more pixels if output is ongoing
			if(!is_sync && state.display_enable) {
				if(!pixel_data_) {
					pixel_pointer_ = pixel_data_ = crt_->allocate_write_area(320);
				}
				if(pixel_pointer_) {
					// the CPC shuffles output lines as:
					//	MA13 MA12	RA2 RA1 RA0		MA9 MA8 MA7 MA6 MA5 MA4 MA3 MA2 MA1 MA0		CCLK
					uint16_t address =
						(uint16_t)(
							((state.refresh_address & 0x3ff) << 1) |
							((state.row_address & 0x7) << 11) |
							((state.refresh_address & 0x3000) << 2)
						);

					switch(mode_) {
						case 0:
							((uint16_t *)pixel_pointer_)[0] = mode0_output_[ram_[address]];
							((uint16_t *)pixel_pointer_)[1] = mode0_output_[ram_[address+1]];
							pixel_pointer_ += 4;
						break;

						case 1:
							((uint32_t *)pixel_pointer_)[0] = mode1_output_[ram_[address]];
							((uint32_t *)pixel_pointer_)[1] = mode1_output_[ram_[address+1]];
							pixel_pointer_ += 8;
						break;

						case 2:
							((uint64_t *)pixel_pointer_)[0] = mode2_output_[ram_[address]];
							((uint64_t *)pixel_pointer_)[1] = mode2_output_[ram_[address+1]];
							pixel_pointer_ += 16;
						break;

						case 3:
							((uint32_t *)pixel_pointer_)[0] = mode3_output_[ram_[address]];
							((uint32_t *)pixel_pointer_)[1] = mode3_output_[ram_[address+1]];
							pixel_pointer_ += 8;
						break;

					}

					// flush the current buffer if full
					if(pixel_pointer_ == pixel_data_ + 320) {
						crt_->output_data(cycles_ * 16, pixel_divider_);
						pixel_pointer_ = pixel_data_ = nullptr;
						cycles_ = 0;
					}
				}
			}

			// check for a trailing hsync
			if(was_hsync_ && !state.hsync) {
				if(mode_ != next_mode_) {
					mode_ = next_mode_;
					switch(mode_) {
						default:
						case 0:		pixel_divider_ = 4;	break;
						case 1:		pixel_divider_ = 2;	break;
						case 2:		pixel_divider_ = 1;	break;
					}
				}

				interrupt_counter_++;
				if(interrupt_counter_ == 52) {
					interrupt_request_ = true;
					interrupt_counter_ = false;
				}

				if(interrupt_reset_counter_) {
					interrupt_reset_counter_--;
					if(!interrupt_reset_counter_) {
						if(interrupt_counter_ < 32) {
							interrupt_request_ = true;
						}
						interrupt_counter_ = 0;
					}
				}
			}

			if(!was_vsync_ && state.vsync) {
				interrupt_reset_counter_ = 2;
			}

			was_vsync_ = state.vsync;
			was_hsync_ = state.hsync;
		}

		bool get_interrupt_request() {
			return interrupt_request_;
		}

		void reset_interrupt_request() {
			interrupt_request_ = false;
			interrupt_counter_ &= ~32;
		}

		void setup_output(float aspect_ratio) {
			crt_.reset(new Outputs::CRT::CRT(1024, 16, Outputs::CRT::DisplayType::PAL50, 1));
			crt_->set_rgb_sampling_function(
				"vec3 rgb_sample(usampler2D sampler, vec2 coordinate, vec2 icoordinate)"
				"{"
					"uint sample = texture(texID, coordinate).r;"
					"return vec3(float((sample >> 4) & 3u), float((sample >> 2) & 3u), float(sample & 3u)) / 2.0;"
				"}");
				// TODO: better vectorise the above.
		}

		void close_output() {
			crt_.reset();
		}

		std::shared_ptr<Outputs::CRT::CRT> get_crt() {
			return crt_;
		}

		void set_next_mode(int mode) {
			next_mode_ = mode;
		}

		bool get_vsync() const {
			return was_vsync_;
		}

		void select_pen(int pen) {
			pen_ = pen;
		}

		void set_colour(uint8_t colour) {
			if(pen_ & 16) {
				border_ = mapped_palette_value(colour);
				// TODO: should flush any border currently in progress
			} else {
				palette_[pen_] = mapped_palette_value(colour);

				// TODO: no need for a full regeneration, of every mode, every time
				for(int c = 0; c < 256; c++) {
					// prepare mode 0
					uint8_t *mode0_pixels = (uint8_t *)&mode0_output_[c];
					mode0_pixels[0] = palette_[((c & 0x80) >> 7) | ((c & 0x20) >> 3) | ((c & 0x08) >> 2) | ((c & 0x02) << 2)];
					mode0_pixels[1] = palette_[((c & 0x40) >> 6) | ((c & 0x10) >> 2) | ((c & 0x04) >> 1) | ((c & 0x01) << 3)];

					// prepare mode 1
					uint8_t *mode1_pixels = (uint8_t *)&mode1_output_[c];
					mode1_pixels[0] = palette_[((c & 0x80) >> 7) | ((c & 0x08) >> 2)];
					mode1_pixels[1] = palette_[((c & 0x40) >> 6) | ((c & 0x04) >> 1)];
					mode1_pixels[2] = palette_[((c & 0x20) >> 5) | ((c & 0x02) >> 0)];
					mode1_pixels[3] = palette_[((c & 0x10) >> 4) | ((c & 0x01) << 1)];

					// prepare mode 2
					uint8_t *mode2_pixels = (uint8_t *)&mode2_output_[c];
					mode2_pixels[0] = palette_[((c & 0x80) >> 7)];
					mode2_pixels[1] = palette_[((c & 0x40) >> 6)];
					mode2_pixels[2] = palette_[((c & 0x20) >> 5)];
					mode2_pixels[3] = palette_[((c & 0x10) >> 4)];
					mode2_pixels[4] = palette_[((c & 0x08) >> 3)];
					mode2_pixels[5] = palette_[((c & 0x04) >> 2)];
					mode2_pixels[6] = palette_[((c & 0x03) >> 1)];
					mode2_pixels[7] = palette_[((c & 0x01) >> 0)];
				}
			}
		}

		void reset_interrupt_counter() {
			interrupt_counter_ = 0;
		}

	private:
		uint8_t mapped_palette_value(uint8_t colour) {
#define COL(r, g, b) (r << 4) | (g << 2) | b
			static const uint8_t mapping[32] = {
				COL(1, 1, 1),	COL(1, 1, 1),	COL(0, 2, 1),	COL(2, 2, 1),
				COL(0, 0, 1),	COL(2, 0, 1),	COL(0, 1, 1),	COL(2, 1, 1),
				COL(2, 0, 1),	COL(2, 2, 1),	COL(2, 2, 0),	COL(2, 2, 2),
				COL(2, 0, 0),	COL(2, 0, 2),	COL(2, 1, 0),	COL(2, 1, 1),
				COL(0, 0, 1),	COL(0, 2, 1),	COL(0, 2, 0),	COL(0, 2, 2),
				COL(0, 0, 0),	COL(0, 0, 2),	COL(0, 1, 0),	COL(0, 1, 2),
				COL(1, 0, 1),	COL(1, 2, 1),	COL(1, 2, 0),	COL(1, 2, 2),
				COL(1, 0, 0),	COL(1, 0, 2),	COL(1, 1, 0),	COL(1, 1, 2),
			};
#undef COL
			return mapping[colour];
		}

		unsigned int cycles_;
		bool was_enabled_, was_sync_, was_hsync_, was_vsync_;
		std::shared_ptr<Outputs::CRT::CRT> crt_;
		uint8_t *pixel_data_, *pixel_pointer_;

		uint8_t *ram_;

		int next_mode_, mode_;

		unsigned int pixel_divider_;
		uint16_t mode0_output_[256];
		uint32_t mode1_output_[256];
		uint64_t mode2_output_[256];
		uint32_t mode3_output_[256];

		int pen_;
		uint8_t palette_[16];
		uint8_t border_;

		int interrupt_counter_;
		bool interrupt_request_;
		int interrupt_reset_counter_;
};

struct KeyboardState {
	KeyboardState() : rows{0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff} {}
	uint8_t rows[10];
};

class i8255PortHandler : public Intel::i8255::PortHandler {
	public:
		i8255PortHandler(const KeyboardState &key_state, const CRTCBusHandler &crtc_bus_handler) :
			key_state_(key_state), crtc_bus_handler_(crtc_bus_handler) {}

		void set_value(int port, uint8_t value) {
			switch(port) {
				case 0:
					ay_->set_data_input(value);
				break;
				case 1:
//					printf("Vsync, etc: %02x\n", value);
				break;
				case 2: {
					// TODO: the AY really should allow port communications to be active. Work needed.
					int key_row = value & 15;
					if(key_row < 10) {
						ay_->set_port_input(false, key_state_.rows[key_row]);
					} else {
						ay_->set_port_input(false, 0xff);
					}
					// TODO: set casette motor control: ((value >> 4) & 1)
					// TODO: set casette output: ((value >> 5) & 1)
					ay_->set_control_lines(
						(GI::AY38910::ControlLines)(
							((value & 0x80) ? GI::AY38910::BDIR : 0) |
							((value & 0x40) ? GI::AY38910::BC1 : 0) |
							GI::AY38910::BC2
						));
				} break;
			}
		}

		uint8_t get_value(int port) {
			switch(port) {
				case 0: return ay_->get_data_output();
				case 1:	return (crtc_bus_handler_.get_vsync() ? 1 : 0) | 0xfe;
				case 2:
//					printf("[In] Key row, etc\n");
				break;
			}
			return 0xff;
		}

		void set_ay(std::shared_ptr<GI::AY38910> ay) {
			ay_ = ay;
		}

	private:
		std::shared_ptr<GI::AY38910> ay_;
		const KeyboardState &key_state_;
		const CRTCBusHandler &crtc_bus_handler_;
};

class ConcreteMachine:
	public CPU::Z80::Processor<ConcreteMachine>,
	public Machine {
	public:
		ConcreteMachine() :
			crtc_counter_(HalfCycles(4)),	// This starts the CRTC exactly out of phase with the memory accesses
			crtc_(crtc_bus_handler_),
			crtc_bus_handler_(ram_),
			i8255_(i8255_port_handler_),
			i8255_port_handler_(key_state_, crtc_bus_handler_) {
			// primary clock is 4Mhz
			set_clock_rate(4000000);
		}

		inline HalfCycles perform_machine_cycle(const CPU::Z80::PartialMachineCycle &cycle) {
			// Amstrad CPC timing scheme: assert WAIT for three out of four cycles
			clock_offset_ = (clock_offset_ + cycle.length) & HalfCycles(7);
			set_wait_line(clock_offset_ >= HalfCycles(2));

			// Update the CRTC once every eight half cycles; aiming for half-cycle 4 as
			// per the initial seed to the crtc_counter_, but any time in the final four
			// will do as it's safe to conclude that nobody else has touched video RAM
			// during that whole window
			crtc_counter_ += cycle.length;
			int crtc_cycles = crtc_counter_.divide(HalfCycles(8)).as_int();
			if(crtc_cycles) crtc_.run_for(Cycles(1));
			set_interrupt_line(crtc_bus_handler_.get_interrupt_request());

			// Stop now if no action is strictly required.
			if(!cycle.is_terminal()) return HalfCycles(0);

			uint16_t address = cycle.address ? *cycle.address : 0x0000;
			switch(cycle.operation) {
				case CPU::Z80::PartialMachineCycle::ReadOpcode:
				case CPU::Z80::PartialMachineCycle::Read:
					*cycle.value = read_pointers_[address >> 14][address & 16383];
				break;

				case CPU::Z80::PartialMachineCycle::Write:
					write_pointers_[address >> 14][address & 16383] = *cycle.value;
				break;

				case CPU::Z80::PartialMachineCycle::Output:
					// Check for a gate array access.
					if((address & 0xc000) == 0x4000) {
						switch(*cycle.value >> 6) {
							case 0: crtc_bus_handler_.select_pen(*cycle.value & 0x1f);		break;
							case 1: crtc_bus_handler_.set_colour(*cycle.value & 0x1f);		break;
							case 2:
								read_pointers_[0] = (*cycle.value & 4) ? &ram_[0] : os_.data();
								read_pointers_[3] = (*cycle.value & 8) ? &ram_[49152] : basic_.data();
								if(*cycle.value & 15) crtc_bus_handler_.reset_interrupt_counter();
								crtc_bus_handler_.set_next_mode(*cycle.value & 3);
							break;
							case 3: printf("RAM paging?\n"); break;
						}
					}

					// Check for a CRTC access
					if(!(address & 0x4000)) {
						switch((address >> 8) & 3) {
							case 0:	crtc_.select_register(*cycle.value);	break;
							case 1:	crtc_.set_register(*cycle.value);		break;
							case 2: case 3:	printf("Illegal CRTC write?\n");	break;
						}
					}

					// Check for a PIO access
					if(!(address & 0x800)) {
						i8255_.set_register((address >> 8) & 3, *cycle.value);
					}
				break;
				case CPU::Z80::PartialMachineCycle::Input:
					*cycle.value = 0xff;

					// Check for a CRTC access
					if(!(address & 0x4000)) {
						switch((address >> 8) & 3) {
							case 0:	case 1: printf("Illegal CRTC read?\n");	break;
							case 2: *cycle.value = crtc_.get_status();		break;
							case 3:	*cycle.value = crtc_.get_register();	break;
						}
					}

					// Check for a PIO access
					if(!(address & 0x800)) {
						*cycle.value = i8255_.get_register((address >> 8) & 3);
					}
				break;

				case CPU::Z80::PartialMachineCycle::Interrupt:
					*cycle.value = 0xff;
					crtc_bus_handler_.reset_interrupt_request();
				break;

				default: break;
			}

			return HalfCycles(0);
		}

		void flush() {
//			i8255_port_handler_.flush();
		}

		void setup_output(float aspect_ratio) {
			crtc_bus_handler_.setup_output(aspect_ratio);
			ay_.reset(new GI::AY38910);
			i8255_port_handler_.set_ay(ay_);
		}

		void close_output() {
			crtc_bus_handler_.close_output();
			ay_.reset();
		}

		std::shared_ptr<Outputs::CRT::CRT> get_crt() {
			return crtc_bus_handler_.get_crt();
		}

		std::shared_ptr<Outputs::Speaker> get_speaker() {
			return ay_;
		}

		void run_for(const Cycles cycles) {
			CPU::Z80::Processor<ConcreteMachine>::run_for(cycles);
		}

		void configure_as_target(const StaticAnalyser::Target &target) {
			// Establish reset memory map as per machine model (or, for now, as a hard-wired 464)
			read_pointers_[0] = os_.data();
			read_pointers_[1] = &ram_[16384];
			read_pointers_[2] = &ram_[32768];
			read_pointers_[3] = basic_.data();

			write_pointers_[0] = &ram_[0];
			write_pointers_[1] = &ram_[16384];
			write_pointers_[2] = &ram_[32768];
			write_pointers_[3] = &ram_[49152];
		}

		void set_rom(ROMType type, std::vector<uint8_t> data) {
			// Keep only the two ROMs that are currently of interest.
			switch(type) {
				case ROMType::OS464:		os_ = data;		break;
				case ROMType::BASIC464:		basic_ = data;	break;
				default: break;
			}
		}

		void set_key_state(uint16_t key, bool isPressed) {
			int line = key >> 4;
			uint8_t mask = (uint8_t)(1 << (key & 7));
			if(isPressed) key_state_.rows[line] &= ~mask; else key_state_.rows[line] |= mask;
		}

		void clear_all_keys() {
			memset(key_state_.rows, 0xff, 10);
		}

	private:
		CRTCBusHandler crtc_bus_handler_;
		Motorola::CRTC::CRTC6845<CRTCBusHandler> crtc_;

		std::shared_ptr<GI::AY38910> ay_;
		i8255PortHandler i8255_port_handler_;
		Intel::i8255::i8255<i8255PortHandler> i8255_;

		HalfCycles clock_offset_;
		HalfCycles crtc_counter_;
		HalfCycles half_cycles_since_ay_update_;

		uint8_t ram_[65536];
		std::vector<uint8_t> os_, basic_;

		uint8_t *read_pointers_[4];
		uint8_t *write_pointers_[4];

		KeyboardState key_state_;
};

Machine *Machine::AmstradCPC() {
	return new ConcreteMachine;
}
