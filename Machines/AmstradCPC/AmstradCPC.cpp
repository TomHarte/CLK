//
//  AmstradCPC.cpp
//  Clock Signal
//
//  Created by Thomas Harte on 30/07/2017.
//  Copyright © 2017 Thomas Harte. All rights reserved.
//

#include "AmstradCPC.hpp"

#include "../../Processors/Z80/Z80.hpp"
#include "../../Components/AY38910/AY38910.hpp"
#include "../../Components/6845/CRTC6845.hpp"

using namespace AmstradCPC;

struct CRTCBusHandler {
	public:
		CRTCBusHandler() : cycles_(0), was_enabled_(false), was_sync_(false) {}

		inline void perform_bus_cycle(const Motorola::CRTC::BusState &state) {
			cycles_++;
			bool is_sync = state.hsync || state.vsync;
			if(state.display_enable != was_enabled_ || is_sync != was_sync_) {
				if(was_sync_) {
					crt_->output_sync((unsigned int)(cycles_ * 2) * 8);
				} else {
					uint8_t *colour_pointer = (uint8_t *)crt_->allocate_write_area(1);
					if(colour_pointer) *colour_pointer = was_enabled_ ? 0xff : 0x00;
					crt_->output_level((unsigned int)(cycles_ * 2) * 8);
				}

				cycles_ = 0;
				was_sync_ = is_sync;
				was_enabled_ = state.display_enable;
			}
		}

		void setup_output(float aspect_ratio) {
			crt_.reset(new Outputs::CRT::CRT(1024, 8, Outputs::CRT::DisplayType::PAL50, 1));
			crt_->set_rgb_sampling_function(
				"vec3 rgb_sample(usampler2D sampler, vec2 coordinate, vec2 icoordinate)"
				"{"
					"return vec3(float(texture(texID, coordinate).r) / 255.0);"
				"}");
		}

		void close_output() {
			crt_.reset();
		}

		std::shared_ptr<Outputs::CRT::CRT> get_crt() {
			return crt_;
		}

	private:
		int cycles_;
		bool was_enabled_, was_sync_;
		std::shared_ptr<Outputs::CRT::CRT> crt_;
};

class ConcreteMachine:
	public CPU::Z80::Processor<ConcreteMachine>,
	public Machine {
	public:
		ConcreteMachine() :
			crtc_counter_(HalfCycles(4)),	// This starts the CRTC exactly out of phase with the memory accesses
			crtc_(crtc_bus_handler_) {
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
							case 0: printf("Select pen %02x\n", *cycle.value & 0x1f);		break;
							case 1: printf("Select colour %02x\n", *cycle.value & 0x1f);	break;
							case 2:
								printf("Set mode %d, other flags %02x\n", *cycle.value & 3, (*cycle.value >> 2)&7);
								read_pointers_[0] = (*cycle.value & 4) ? &ram_[0] : os_.data();
								read_pointers_[3] = (*cycle.value & 8) ? &ram_[49152] : basic_.data();
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
						switch((address >> 8) & 3) {
							case 0:	printf("PSG data: %d\n", *cycle.value);	break;
							case 1:	printf("Vsync, etc: %02x\n", *cycle.value);	break;
							case 2:	printf("Key row, etc: %02x\n", *cycle.value);	break;
							case 3:	printf("PIO control: %02x\n", *cycle.value);	break;
						}
					}
				break;
				case CPU::Z80::PartialMachineCycle::Input:
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
						switch((address >> 8) & 3) {
							case 0:	printf("[In] PSG data\n");		break;
							case 1:	printf("[In] Vsync, etc\n");	break;
							case 2:	printf("[In] Key row, etc\n");	break;
							case 3:	printf("[In] PIO control\n");	break;
						}
					}

					*cycle.value = 0xff;
				break;

				case CPU::Z80::PartialMachineCycle::Interrupt:
					*cycle.value = 0xff;
				break;

				default: break;
			}

			return HalfCycles(0);
		}

		void flush() {}

		void setup_output(float aspect_ratio) {
			crtc_bus_handler_.setup_output(aspect_ratio);
		}

		void close_output() {
			crtc_bus_handler_.close_output();
		}

		std::shared_ptr<Outputs::CRT::CRT> get_crt() {
			return crtc_bus_handler_.get_crt();
		}

		std::shared_ptr<Outputs::Speaker> get_speaker() {
			return nullptr;
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

	private:
		CRTCBusHandler crtc_bus_handler_;
		Motorola::CRTC::CRTC6845<CRTCBusHandler> crtc_;

		HalfCycles clock_offset_;
		HalfCycles crtc_counter_;

		uint8_t ram_[65536];
		std::vector<uint8_t> os_, basic_;

		uint8_t *read_pointers_[4];
		uint8_t *write_pointers_[4];
};

Machine *Machine::AmstradCPC() {
	return new ConcreteMachine;
}
