//
//  AppleII.cpp
//  Clock Signal
//
//  Created by Thomas Harte on 14/04/2018.
//  Copyright © 2018 Thomas Harte. All rights reserved.
//

#include "AppleII.hpp"

#include "../CRTMachine.hpp"

#include "Video.hpp"

#include <memory>

namespace {

class ConcreteMachine:
	public CRTMachine::Machine,
	public AppleII::Machine {
	public:

		ConcreteMachine() {
			set_clock_rate(1022727);
		}

		void setup_output(float aspect_ratio) override {
			video_.reset(new AppleII::Video);
		}

		void close_output() override {
			video_.reset();
		}

		Outputs::CRT::CRT *get_crt() override {
			return video_->get_crt();
		}

		Outputs::Speaker::Speaker *get_speaker() override {
			return nullptr;
		}

		void run_for(const Cycles cycles) override {
		}

	private:
		std::unique_ptr<AppleII::Video> video_;
};

}

using namespace AppleII;

Machine *Machine::AppleII() {
	return new ConcreteMachine;
}

Machine::~Machine() {}
