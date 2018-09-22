//
//  MasterSystem.cpp
//  Clock Signal
//
//  Created by Thomas Harte on 20/09/2018.
//  Copyright © 2018 Thomas Harte. All rights reserved.
//

#include "MasterSystem.hpp"

#include "../../Processors/Z80/Z80.hpp"

#include "../../Components/9918/9918.hpp"
#include "../../Components/SN76489/SN76489.hpp"

#include "../CRTMachine.hpp"
#include "../JoystickMachine.hpp"

#include "../../ClockReceiver/ForceInline.hpp"

#include "../../Outputs/Speaker/Implementation/LowpassSpeaker.hpp"

namespace {
const int sn76489_divider = 2;
}

namespace Sega {
namespace MasterSystem {

class ConcreteMachine:
	public Machine,
	public CPU::Z80::BusHandler,
	public CRTMachine::Machine {

	public:
		ConcreteMachine(const Analyser::Static::Target &target, const ROMMachine::ROMFetcher &rom_fetcher) :
			z80_(*this),
			sn76489_(TI::SN76489::Personality::SN76489, audio_queue_, sn76489_divider),
			speaker_(sn76489_) {
			speaker_.set_input_rate(3579545.0f / static_cast<float>(sn76489_divider));
			set_clock_rate(3579545);
		}

		~ConcreteMachine() {
			audio_queue_.flush();
		}

		void setup_output(float aspect_ratio) override {
			vdp_.reset(new TI::TMS::TMS9918(TI::TMS::TMS9918A));
			get_crt()->set_video_signal(Outputs::CRT::VideoSignal::Composite);
		}

		void close_output() override {
			vdp_.reset();
		}

		Outputs::CRT::CRT *get_crt() override {
			return vdp_->get_crt();
		}

		Outputs::Speaker::Speaker *get_speaker() override {
			return &speaker_;
		}

		void run_for(const Cycles cycles) override {
			z80_.run_for(cycles);
		}

		forceinline HalfCycles perform_machine_cycle(const CPU::Z80::PartialMachineCycle &cycle) {
			time_since_vdp_update_ += cycle.length;
			time_since_sn76489_update_ += cycle.length;

			if(time_until_interrupt_ > 0) {
				time_until_interrupt_ -= cycle.length;
				if(time_until_interrupt_ <= HalfCycles(0)) {
					z80_.set_non_maskable_interrupt_line(true, time_until_interrupt_);
				}
			}

			return HalfCycles(0);
		}

		void flush() {
			update_video();
			update_audio();
			audio_queue_.perform();
		}

	private:
		inline void update_audio() {
			speaker_.run_for(audio_queue_, time_since_sn76489_update_.divide_cycles(Cycles(sn76489_divider)));
		}
		inline void update_video() {
			vdp_->run_for(time_since_vdp_update_.flush());
		}

		CPU::Z80::Processor<ConcreteMachine, false, false> z80_;
		std::unique_ptr<TI::TMS::TMS9918> vdp_;

		Concurrency::DeferringAsyncTaskQueue audio_queue_;
		TI::SN76489 sn76489_;
		Outputs::Speaker::LowpassSpeaker<TI::SN76489> speaker_;

		HalfCycles time_since_vdp_update_;
		HalfCycles time_since_sn76489_update_;
		HalfCycles time_until_interrupt_;
};

}
}

using namespace Sega::MasterSystem;

Machine *Machine::MasterSystem(const Analyser::Static::Target *target, const ROMMachine::ROMFetcher &rom_fetcher) {
	return new ConcreteMachine(*target, rom_fetcher);
}

Machine::~Machine() {}
