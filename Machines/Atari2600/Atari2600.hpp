//
//  Atari2600.hpp
//  CLK
//
//  Created by Thomas Harte on 14/07/2015.
//  Copyright © 2015 Thomas Harte. All rights reserved.
//

#ifndef Atari2600_cpp
#define Atari2600_cpp

#include <stdint.h>

#include "../../Processors/6502/6502.hpp"
#include "../CRTMachine.hpp"
#include "Bus.hpp"
#include "PIA.hpp"
#include "Speaker.hpp"
#include "TIA.hpp"

#include "../ConfigurationTarget.hpp"
#include "Atari2600Inputs.h"

namespace Atari2600 {

class Machine:
	public CRTMachine::Machine,
	public ConfigurationTarget::Machine,
	public Outputs::CRT::Delegate {

	public:
		Machine();
		~Machine();

		void configure_as_target(const StaticAnalyser::Target &target);
		void switch_region();

		void set_digital_input(Atari2600DigitalInput input, bool state);
		void set_switch_is_enabled(Atari2600Switch input, bool state);
		void set_reset_line(bool state) { bus_->set_reset_line(state); }

		// to satisfy CRTMachine::Machine
		virtual void setup_output(float aspect_ratio);
		virtual void close_output();
		virtual std::shared_ptr<Outputs::CRT::CRT> get_crt() { return bus_->tia_->get_crt(); }
		virtual std::shared_ptr<Outputs::Speaker> get_speaker() { return bus_->speaker_; }
		virtual void run_for(const Cycles &cycles) { bus_->run_for(cycles); }

		// to satisfy Outputs::CRT::Delegate
		virtual void crt_did_end_batch_of_frames(Outputs::CRT::CRT *crt, unsigned int number_of_frames, unsigned int number_of_unexpected_vertical_syncs);

	private:
		// the bus
		std::unique_ptr<Bus> bus_;

		// output frame rate tracker
		struct FrameRecord {
			unsigned int number_of_frames;
			unsigned int number_of_unexpected_vertical_syncs;

			FrameRecord() : number_of_frames(0), number_of_unexpected_vertical_syncs(0) {}
		} frame_records_[4];
		unsigned int frame_record_pointer_;
		bool is_ntsc_;
};

}

#endif /* Atari2600_cpp */
