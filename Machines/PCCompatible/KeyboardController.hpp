//
//  KeyboardController.hpp
//  Clock Signal
//
//  Created by Thomas Harte on 07/03/2025.
//  Copyright © 2025 Thomas Harte. All rights reserved.
//

#pragma once

#include "PIC.hpp"
#include "Speaker.hpp"

namespace PCCompatible {

template <Analyser::Static::PCCompatible::Model, typename Enable = void>
class KeyboardController;

template <Analyser::Static::PCCompatible::Model model>
class KeyboardController<model, typename std::enable_if_t<is_xt(model)>> {
public:
	KeyboardController(PICs<model> &pics, PCSpeaker &) : pics_(pics) {}

	// KB Status Port 61h high bits:
	//; 01 - normal operation. wait for keypress, when one comes in,
	//;		force data line low (forcing keyboard to buffer additional
	//;		keypresses) and raise IRQ1 high
	//; 11 - stop forcing data line low. lower IRQ1 and don't raise it again.
	//;		drop all incoming keypresses on the floor.
	//; 10 - lower IRQ1 and force clock line low, resetting keyboard
	//; 00 - force clock line low, resetting keyboard, but on a 01->00 transition,
	//;		IRQ1 would remain high
	void set_mode(const uint8_t mode) {
		const auto last_mode = mode_;
		mode_ = Mode(mode);
		switch(mode_) {
			case Mode::NormalOperation:		break;
			case Mode::NoIRQsIgnoreInput:
				pics_.pic[0].template apply_edge<1>(false);
			break;
			case Mode::Reset:
				input_.clear();
				[[fallthrough]];
			case Mode::ClearIRQReset:
				pics_.pic[0].template apply_edge<1>(false);
			break;
		}

		// If the reset condition ends, start a counter through until reset is complete.
		if(last_mode == Mode::Reset && mode_ != Mode::Reset) {
			reset_delay_ = 15;      // Arbitrarily.
		}
	}

	void run_for(const Cycles cycles) {
		if(reset_delay_ <= 0) {
			return;
		}
		reset_delay_ -= cycles.as<int>();
		if(reset_delay_ <= 0) {
			input_.clear();
			post(0xaa);
		}
	}

	uint8_t read() {
		pics_.pic[0].template apply_edge<1>(false);
		if(input_.empty()) {
			return 0;
		}

		const uint8_t key = input_.front();
		input_.erase(input_.begin());
		if(!input_.empty()) {
			pics_.pic[0].template apply_edge<1>(true);
		}
		return key;
	}

	void post(const uint8_t value) {
		if(mode_ != Mode::NormalOperation || reset_delay_) {
			return;
		}
		input_.push_back(value);
		pics_.pic[0].template apply_edge<1>(true);
	}

private:
	enum class Mode {
		NormalOperation = 0b01,
		NoIRQsIgnoreInput = 0b11,
		ClearIRQReset = 0b10,
		Reset = 0b00,
	} mode_;

	std::vector<uint8_t> input_;
	PICs<model> &pics_;

	int reset_delay_ = 0;
};

template <Analyser::Static::PCCompatible::Model model>
class KeyboardController<model, typename std::enable_if_t<is_at(model)>> {
public:
	KeyboardController(PICs<model> &pics, PCSpeaker &speaker) : pics_(pics), speaker_(speaker) {}

	void run_for([[maybe_unused]] const Cycles cycles) {
	}

	void post([[maybe_unused]] const uint8_t value) {
	}

	template <typename IntT>
	void write([[maybe_unused]] const uint16_t port, [[maybe_unused]] const IntT value) {
		switch(port) {
			default:
				log_.error().append("Unimplemented AT keyboard write: %04x to %04x", value, port);
			break;

			case 0x0061:
				// TODO:
				//	b7: 1 = reset IRQ 0
				//	b3: enable channel check
				//	b2: enable parity check
				speaker_.set_control(value & 0x01, value & 0x02);
			break;
		}
	}

	template <typename IntT>
	IntT read([[maybe_unused]] const uint16_t port) {
		switch(port) {
			default:
				log_.error().append("Unimplemented AT keyboard read from %04x", port);
			break;

			case 0x0061:
				refresh_toggle_ ^= 0x10;
				log_.info().append("AT keyboard: %02x from %04x", refresh_toggle_, port);
			return refresh_toggle_;
		}
		return IntT(~0);
	}

private:
	Log::Logger<Log::Source::PCCompatible> log_;

	PICs<model> &pics_;
	PCSpeaker &speaker_;
	uint8_t refresh_toggle_ = 0;
};

}
