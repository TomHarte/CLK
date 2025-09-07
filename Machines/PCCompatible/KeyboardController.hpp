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

#include "Analyser/Static/PCCompatible/Target.hpp"

#include <array>
#include <optional>

extern bool should_log;

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
	KeyboardController(PICs<model> &pics, Speaker &, Analyser::Static::PCCompatible::Target::VideoAdaptor) :
		pics_(pics) {}

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

	auto &keyboard() {
		return *this;
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
	KeyboardController(
		PICs<model> &pics,
		Speaker &speaker,
		const Analyser::Static::PCCompatible::Target::VideoAdaptor adaptor
	) : pics_(pics), speaker_(speaker), keyboard_(*this) {
		if(adaptor == Analyser::Static::PCCompatible::Target::VideoAdaptor::MDA) {
			switches_ |= 0x40;
		}
	}

	void run_for(const Cycles cycles) {
		instruction_count_ += cycles.as<int>();

		if(!perform_delay_) {
			return;
		}

		perform_delay_ -= cycles.as<int>();
		if(perform_delay_ <= 0) {
			perform_delay_ = 0;
			perform_command();
		}
	}

	auto &keyboard() {
		return keyboard_;
	}

	void write(const uint16_t port, const uint8_t value) {
		switch(port) {
			default:
				log_.error().append("Unimplemented AT keyboard write: %04x to %04x", value, port);
			break;

			case 0x0060:
				log_.info().append("Keyboard parameter set to %02x", value);
				phase_ = Phase::Data;
				input_ = value;
				has_input_ = true;
				perform_command();
			break;

			case 0x0061:
				// TODO:
				//	b7: 1 = reset IRQ 0
				//	b3: enable channel check
				//	b2: enable parity check
				speaker_.set_control(value & 0x01, value & 0x02);
			break;

			case 0x0064:
				phase_ = Phase::Command;
				command_ = Command(value);
				has_command_ = true;
				has_input_ = false;
				perform_delay_ = performance_delay(command_);

				perform_command();
			break;
		}
	}

	uint8_t read(const uint16_t port) {
		switch(port) {
			default:
				log_.error().append("Unimplemented AT keyboard read from %04x", port);
			break;

			case 0x0060: {
				if(has_output()) {
					advance_output_queue_pointer(output_read_);

					if(!has_output()) {
						pics_.pic[0].template apply_edge<1>(false);
					}
				}
				log_.info().append("Read from keyboard controller of %02x", output_queue_[output_read_]);
				return output_queue_[output_read_];
			}

			case 0x0061:
				// In a real machine bit 4 toggles as a function of memory refresh and some BIOSes
				// (including IBM's) do a polled loop to test its speed. So that effectively compares
				// PIT counts against CPU cycle counts. Since this emulator does nothing whatsoever
				// to attempt realistic CPU timing, the ratio here is just one I found that passed
				// BIOS inspection. I may have overfitted to IBM's. This counts as an ugliness.
			return ((instruction_count_ * 2) / 25) & 0x10;

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
					(enabled_ 					? 0x10 : 0x00) |
					(phase_ == Phase::Command	? 0x08 : 0x00) |
					(is_tested_					? 0x04 : 0x00) |
					(has_input_					? 0x02 : 0x00) |
					(has_output()				? 0x01 : 0x00);
				log_.info().append("Reading status: %02x", status);
				return status;
			}
		}
		return 0xff;
	}

	void set_cpu_control(CPUControl<model> *const control) {
		cpu_control_ = control;
	}

private:
	enum Command: uint8_t {
		ReadCommandByte = 0x20,		// TODO.
		WriteCommandByte = 0x60,

		SelfTest = 0xaa,
		InterfaceTest = 0xab,

		DisableKeyboard = 0xad,
		EnableKeyboard = 0xae,

		ReadSwitches = 0xc0,
		GetOutputByte = 0xd0,		// TODO.
		SetOutputByte = 0xd1,

		ReadTestInputs = 0xe0,

		ResetBlockBegin = 0xf0,
	};

	static constexpr bool requires_parameter(const Command command) {
		return
			(command >= 0x60 && command < 0x80) ||
			(command == 0xc1) || (command == 0xc2) ||
			(command >= 0xd1 && command < 0xd5);
	}

	static constexpr int performance_delay(const Command command) {
		if(requires_parameter(command)) {
			return 3;
		}

		switch(command) {
			case Command::SelfTest:	return 15;
			default: 				return 0;
		}
	}

	void transmit(const uint8_t value) {
		log_.info().append("Enquing %02x", value);
		advance_output_queue_pointer(output_write_);
		output_queue_[output_write_] = value;
		pics_.pic[0].template apply_edge<1>(true);	// TODO: verify.
	}

	void perform_command() {
		// Don't do anything until perform_delay_ is 0 and a command and/or other input is ready.
		if(perform_delay_ || (!has_input_ && !has_command_)) {
			return;
		}

		// No command => input only, which is a direct-to-device communication.
		if(!has_command_) {
			if(!enabled_) {
				log_.info().append("Storing device command for later: %02x", input_);
				keyboard_command_ = input_;
				has_input_ = false;
				return;
			}
			log_.info().append("Device command: %02x", input_);
			keyboard_.perform(input_);
			// TODO: mouse?
			has_input_ = false;
			return;
		}

		// There is a command, but stop anyway if it requires a parameter and doesn't yet have one.
		if(requires_parameter(command_) && !has_input_) {
			return;
		}

		log_.info().append("Controller command: %02x", command_).append_if(has_input_, " / %02x", input_);

		// Consume command and parameter, and execute.
		has_command_ = false;
		if(requires_parameter(command_)) has_input_ = false;

		if(command_ >= Command::ResetBlockBegin) {
			log_.info().append("Should reset: %x", command_ & 0x0f);

			if(!(command_ & 1)) {
				cpu_control_->reset();
			}
		} else switch(command_) {
			default:
				log_.info().append("Unimplemented keyboard controller command: %02x", command_);
			break;

			case Command::WriteCommandByte:
				is_tested_ = input_ & 0x4;
				// TODO:
				//	b0: 1 = enable first PS/2 port interrupt;
				//	b1: 1 = enable second port interrupt;
				//	b2: 1 = system has passed POST
				//	b3: should be 0
				//	b4: 1 = disable first port clock;
				//	b5: 1 = disable second port clock;
				//	b6: 1 = enable translation
				//	b7: should be 0
			break;

			case Command::SelfTest:
				is_tested_ = true;
				transmit(0x55);	// 0x55 => no issues found.
			break;
			case Command::InterfaceTest:
				transmit(0);	// i.e. no issues uncovered.
			break;
			case Command::ReadTestInputs:
				transmit(enabled_ ? 0x01 : 0x00);
			break;

			case Command::DisableKeyboard:	enabled_ = false;	break;
			case Command::EnableKeyboard:
				enabled_ = true;

				// If a keybaord command was enqueued, post it now.
				if(keyboard_command_.has_value()) {
					input_ = *keyboard_command_;
					keyboard_command_ = std::nullopt;
					has_input_ = true;
					perform_command();
				}
			break;

			case Command::SetOutputByte:
				// b1 = the A20 gate, 1 => A20 enabled.
				cpu_control_->set_a20_enabled(input_ & 0x02);
			break;

			case Command::ReadSwitches:
				transmit(switches_);
			break;
		}
	}

	[[no_unique_address]] Log::Logger<Log::Source::Keyboard> log_;

	PICs<model> &pics_;
	Speaker &speaker_;
	CPUControl<model> *cpu_control_ = nullptr;

	// Strongly coupled to specific code in the 5170 BIOS, this provides a grossly-inaccurate
	// linkage between execution speed (-ish) and DRAM refresh. An unambguous nonsense.
	int instruction_count_ = 0;

	uint8_t input_;
	Command command_;
	std::optional<uint8_t> keyboard_command_;

	bool has_input_ = false;
	bool has_command_ = false;

	std::array<uint8_t, 8> output_queue_;
	void advance_output_queue_pointer(size_t &pointer) {
		pointer = (pointer + 1) % output_queue_.size();
	}

	size_t output_read_ = 0, output_write_ = 0;
	bool has_output() const {
		return output_read_ != output_write_;
	}

	//	bit 7	  = 0  keyboard inhibited
	//	bit 6	  = 0  CGA, else MDA
	//	bit 5	  = 0  manufacturing jumper installed
	//	bit 4	  = 0  system RAM 512K, else 640K
	//	bit 3-0      reserved
	uint8_t switches_ = 0b1011'0000;

	int perform_delay_ = 0;

	bool is_tested_ = false;
	bool enabled_ = false;

	enum class Phase {
		Command,
		Data,
	} phase_ = Phase::Command;

	struct Keyboard {
		Keyboard(KeyboardController<model> &controller) : controller_(controller) {}

		void post(const uint8_t value) {
			controller_.post_keyboard({value});
		}

		void perform(const uint8_t command) {
			switch(command) {
				default:
					log_.error().append("Unimplemented keyboard command: %02x", command);
				break;

				case 0xf2:
					controller_.post_keyboard({0xfa});
				break;

				case 0xff:
					controller_.post_keyboard({0xfa, 0xaa});
				break;
			}
		}

	private:
		KeyboardController<model> &controller_;
		[[no_unique_address]] Log::Logger<Log::Source::Keyboard> log_;
	} keyboard_;

	friend Keyboard;
	void post_keyboard(const std::initializer_list<uint8_t> values) {
		if(!enabled_) return;
		for(const auto value : values) {
			transmit(value);
		}
	}
};

}
