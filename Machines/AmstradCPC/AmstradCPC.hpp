//
//  AmstradCPC.hpp
//  Clock Signal
//
//  Created by Thomas Harte on 30/07/2017.
//  Copyright © 2017 Thomas Harte. All rights reserved.
//

#ifndef AmstradCPC_hpp
#define AmstradCPC_hpp

#include "../ConfigurationTarget.hpp"
#include "../CRTMachine.hpp"

#include "../../Processors/Z80/Z80.hpp"
#include "../../Components/AY38910/AY38910.hpp"
#include "../../Components/6845/CRTC6845.hpp"

namespace AmstradCPC {

enum ROMType: uint8_t {
	OS464, OS664, OS6128,
	BASIC464, BASIC664, BASIC6128,
	AMSDOS
};

struct CRTCBusHandler {
	public:
		CRTCBusHandler() : cycles_(0), was_enabled_(false), was_sync_(false) {
		}

		inline void perform_bus_cycle(const Motorola::CRTC::BusState &state) {
			cycles_++;
			bool is_sync = state.hsync || state.vsync;
			if(state.display_enable != was_enabled_ || is_sync != was_sync_) {
				if(was_sync_) {
					crt_->output_sync((unsigned int)(cycles_ * 2));
				} else {
					uint8_t *colour_pointer = (uint8_t *)crt_->allocate_write_area(1);
					if(colour_pointer) *colour_pointer = was_enabled_ ? 0xff : 0x00;
					crt_->output_level((unsigned int)(cycles_ * 2));
				}

				cycles_ = 0;
				was_sync_ = is_sync;
				was_enabled_ = state.display_enable;
			}
		}

		std::shared_ptr<Outputs::CRT::CRT> crt_;

	private:
		int cycles_;
		bool was_enabled_, was_sync_;
};

class Machine:
	public CPU::Z80::Processor<Machine>,
	public CRTMachine::Machine,
	public ConfigurationTarget::Machine {
	public:
		Machine();

		HalfCycles perform_machine_cycle(const CPU::Z80::PartialMachineCycle &cycle);
		void flush();

		void setup_output(float aspect_ratio);
		void close_output();

		std::shared_ptr<Outputs::CRT::CRT> get_crt();
		std::shared_ptr<Outputs::Speaker> get_speaker();

		void run_for(const Cycles cycles);

		void configure_as_target(const StaticAnalyser::Target &target);

		void set_rom(ROMType type, std::vector<uint8_t> data);

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

}

#endif /* AmstradCPC_hpp */
