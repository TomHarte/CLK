//
//  KeyboardController.hpp
//  Clock Signal
//
//  Created by Thomas Harte on 07/03/2025.
//  Copyright © 2025 Thomas Harte. All rights reserved.
//

#pragma once

#include "CPUControl.hpp"
#include "PIC.hpp"
#include "Speaker.hpp"

namespace PCCompatible {

/*!
	Provides an implementation of either an XT- or AT-style keyboard controller,
	as determined by the model template parameter.
*/
template <Analyser::Static::PCCompatible::Model, typename Enable = void>
class KeyboardController;

/*!
	Models the XT keyboard controller.
*/
template <Analyser::Static::PCCompatible::Model model>
class KeyboardController<model, typename std::enable_if_t<is_xt(model)>> {
public:
	KeyboardController(PICs<model> &pics, Speaker &) : pics_(pics) {}

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

	void set_cpu_control(CPUControl<model> *) {}

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

/*!
	Models the AT keyboard controller.
*/
template <Analyser::Static::PCCompatible::Model model>
class KeyboardController<model, typename std::enable_if_t<is_at(model)>> {
public:
	KeyboardController(PICs<model> &pics, Speaker &speaker) : pics_(pics), speaker_(speaker) {}

	void run_for([[maybe_unused]] const Cycles cycles) {
	}

	void post(const uint8_t value) {
		parameter_ = value;
		has_input_ = true;
		is_command_ = false;
		pics_.pic[0].template apply_edge<1>(true);
	}

	void write(const uint16_t port, const uint8_t value) {
		switch(port) {
			default:
				log_.error().append("Unimplemented AT keyboard write: %04x to %04x", value, port);
			break;

			case 0x0060:
				log_.error().append("Keyboard parameter set to: ", value);
				parameter_ = value;
			break;

			case 0x0061:
				// TODO:
				//	b7: 1 = reset IRQ 0
				//	b3: enable channel check
				//	b2: enable parity check
				speaker_.set_control(value & 0x01, value & 0x02);
			break;

			case 0x0064:
				is_command_ = true;

				auto info = log_.info();
				info.append("AT keyboard command %04x", value);
				switch(value) {
					default:
						info.append("; unimplemented");
					break;

					case 0xaa:	// Self-test; 0x55 => no issues found.
						log_.error().append("Keyboard self-test");
						post(0x55);
					break;

					case 0xd1:	// Set output byte. b1 = the A20 gate.
						log_.error().append("Should set A20 gate: %d", value & 0x02);
						cpu_control_->set_a20_enabled(value & 0x02);
					break;

					case 0xf0:	case 0xf1:	case 0xf2:	case 0xf3:
					case 0xf4:	case 0xf5:	case 0xf6:	case 0xf7:
					case 0xf8:	case 0xf9:	case 0xfa:	case 0xfb:
					case 0xfc:	case 0xfd:	case 0xfe:	case 0xff:
						log_.error().append("Should reset: %x", value & 0x0f);

						if(!(value & 1)) {
							cpu_control_->reset();
						}
					break;
				}
			break;
		}
	}

	template <typename IntT>
	IntT read(const uint16_t port) {
		switch(port) {
			default:
				log_.error().append("Unimplemented AT keyboard read from %04x", port);
			break;

			case 0x0060:
				log_.error().append("Read keyboard parameter of %02x", parameter_);

				// TODO: disabled in response to BIOS expectations but possibly a false
				// positive because commands are responded to instantly?
//				has_input_ = false;
			return parameter_;

			case 0x0061:
				// In a real machine bit 4 toggles as a function of memory refresh; it is often
				// used by BIOSes to check that refresh is happening, with no greater inspection
				// than that it is toggling. So toggle on read.
				refresh_toggle_ ^= 0x10;

				log_.info().append("AT keyboard: %02x from %04x", refresh_toggle_, port);
			return refresh_toggle_;

			case 0x0064: {
				// Status:
				//	b7 = 1 => parity error on transmission;
				//	b6 = 1 => receive timeout;
				// 	b5 = 1 => transmit timeout;
				//	b4 = 1 => keyboard active;
				//	b3 = 1 = data at 0060 is command, 0 = data;
				//	b2 = 1 = selftest OK; 0 = just powered up or reset;
				//	b1 = 1 => 'input' buffer full (i.e. don't write 0x60 or 0x64 now — this is input to the controller);
				//	b0 = 1 => 'output' data is full (i.e. reading from 0x60 now makes sense — output is to PC).
				const uint8_t status =
					0x10 |
					(is_command_	? 0x08 : 0x00) |
					(has_output_	? 0x02 : 0x00) |
					(has_input_		? 0x01 : 0x00);
				log_.error().append("Reading status: %02x", status);
				return status;
			}
		}
		return IntT(~0);
	}

	void set_cpu_control(CPUControl<model> *const control) {
		cpu_control_ = control;
	}

private:
	Log::Logger<Log::Source::PCCompatible> log_;

	PICs<model> &pics_;
	Speaker &speaker_;
	CPUControl<model> *cpu_control_ = nullptr;

	uint8_t refresh_toggle_ = 0;

	bool has_input_ = false;
	bool has_output_ = false;

	bool is_command_ = false;
	uint8_t parameter_;
};

}
