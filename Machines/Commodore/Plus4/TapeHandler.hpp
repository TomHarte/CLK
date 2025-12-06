//
//  TapeHandler.hpp
//  Clock Signal
//
//  Created by Thomas Harte on 03/12/2025.
//  Copyright © 2025 Thomas Harte. All rights reserved.
//

#pragma once

#include "Processors/6502Mk2/6502Mk2.hpp"
#include "Storage/Data/Commodore.hpp"
#include "Storage/Tape/Tape.hpp"
#include "Storage/Tape/Parsers/Commodore.hpp"

#include <algorithm>
#include <memory>
#include <optional>

namespace Commodore::Plus4 {

/*!
	Describes a continuous block of memory that the tape handler asserts should be completed as quickly as possible, regardless
	of wall-clock time, and that depends upon only timers and tape hardware running at the correct rate relative to one another.

	i.e. this is used to indicate where the machine can apply accelerated loading, running the machine without video or disk drives as
	quickly as possible until the program counter exists the nominated range.
*/
struct AcceleratedRange {
	uint16_t low, high;
};

/*!
	All tape assistance, bundled into a single place, including:

		(i) automatic motor control; and
		(ii) fast loading, including loader detection.
*/
struct TapeHandler: public ClockingHint::Observer {
	static constexpr uint16_t ROMTrapAddress = 0xf0f0;

	// MARK: - Getters.

	Storage::Tape::BinaryTapePlayer &tape_player() {
		return *tape_player_;
	}

	const Storage::Tape::BinaryTapePlayer &tape_player() const {
		return *tape_player_;
	}

	bool test_rom_trap() const {
		return use_fast_tape_hack_;
	}

	bool apply_accelerated_range() const {
		return allow_fast_tape_hack_ && !tape_player_->is_at_end();
	}

	bool play_button() const {
		return play_button_;
	}

	// MARK: - Rote Setters.

	void set_allow_accelerated_tape_loading(const bool allow) {
		allow_fast_tape_hack_ = allow;
		set_use_fast_tape();
	}

	bool allow_accelerated_tape_loading() const {
		return allow_fast_tape_hack_;
	}

	void set_rom_is_paged(const bool is_paged) {
		rom_is_paged_ = is_paged;
		set_use_fast_tape();
	}

	void set_io(const uint8_t output, const uint8_t direction) {
		io_output_ = output;
		io_direction_ = direction;
		update_tape_motor();
	}

	void set_tape(std::shared_ptr<Storage::Tape::Tape> tape) {
		tape_player_->set_tape(tape, TargetPlatform::Plus4);
	}

	// MARK: - Clocking.

	void set_clock_rate(const int rate) {
		clock_rate_ = rate;
		tape_player_ = std::make_unique<Storage::Tape::BinaryTapePlayer>(rate);
		tape_player_->set_clocking_hint_observer(this);
	}

	void run_for(const Cycles length) {
		tape_player_->run_for(length);
	}

	// MARK: - Automatic play button detection.

	void read_parallel_port(const std::function<std::array<uint8_t, 4>(void)> &test_memory) {
		// 6529 parallel port, about which I know only what I've found in kernel ROM disassemblies.

		// Intended logic: if play button is not currently pressed and this read is immediately followed by
		// an AND 4, press it. The kernel will deal with motor control subsequently. This seems to be how
		// the ROM tests whether the user has yet responded to its invitation to press play.
		if(play_button_) return;

		// TODO: boil this down to a PC check. It's currently in this form as I'm unclear what
		// diversity of kernels exist.
		const auto next = test_memory();

		if(next[0] == 0x29 && next[1] == 0x04 && next[2] == 0xd0 && next[3] == 0xf4) {
			play_button_ = true;
			update_tape_motor();
		}
	}

	// MARK: - Loading accelerators.

	template <typename M6502T>
	bool perform_ldcass(M6502T &m6502, std::array<uint8_t, 65536> &ram, const Cycles timer_cycle_length) {
		// Magic constants.
		static constexpr uint16_t FileNameLength = 0xab;
		static constexpr uint16_t FileNameAddress = 0xaf;
		static constexpr uint16_t TapeBlockType = 0xf8;
		static constexpr uint16_t SecondAddressFlag = 0xad;
		static constexpr uint16_t HeaderBuffer = 0x0333;

		// Imply an automatic motor start.
		play_button_ = true;
		update_tape_motor();

		// Input:
		//	A: 0 = Load, 1-255 = Verify;
		//	X/Y = Load address (if secondary address = 0).
		// Output:
		//	Carry: 0 = No errors, 1 = Error;
		//	A = KERNAL error code (if Carry = 1);
		//	X/Y = Address of last byte loaded/verified (if Carry = 0).
		// Used registers: A, X, Y. Real address: $F49E.

		auto registers = m6502.registers();

		// Check for a filename.
		std::vector<uint8_t> raw_name;
		const uint8_t name_length = ram[FileNameLength];
		if(name_length) {
			const uint16_t address = uint16_t(ram[FileNameAddress] | (ram[FileNameAddress + 1] << 8));
			for(uint16_t c = 0; c < name_length; c++) {
				raw_name.push_back(ram[address + c]);
			}
		}

		const auto start_offset = tape_player_->serialiser()->offset();

		// Search for first thing that matches the file name.
		Storage::Tape::Commodore::Parser parser(TargetPlatform::Plus4);
		auto &serialiser = *tape_player_->serialiser();
		std::unique_ptr<Storage::Tape::Commodore::Header> header;
		while(!parser.is_at_end(serialiser)) {
			header = parser.get_next_header(serialiser);
			if(!header || !header->parity_was_valid) {
				continue;
			}
			if(!raw_name.empty() && raw_name != header->raw_name) {
				continue;
			}

			const auto body = parser.get_next_data(serialiser);
			if(!body || !body->parity_was_valid) {
				continue;
			}

			// Copy header into place.
			header->serialise(&ram[HeaderBuffer], uint16_t(ram.size() - HeaderBuffer));

			// Set block type; 0x00 = data body.
			ram[TapeBlockType] = 0;

			// TODO: F5 = checksum.

			auto load_address =
				ram[SecondAddressFlag] ? header->starting_address : uint16_t((registers.y << 8) | registers.x);

			// Set 'load ram base', 'sta' and 'tapebs'.
			ram[0xb2] = ram[0xb4] = ram[0xb6] = load_address & 0xff;
			ram[0xb3] = ram[0xb5] = ram[0xb7] = load_address >> 8;

			if(load_address + body->data.size() < 65536) {
				std::copy(body->data.begin(), body->data.end(), &ram[load_address]);
			} else {
				const auto split_point = body->data.begin() + 65536 - load_address;
				std::copy(body->data.begin(), split_point, &ram[load_address]);
				std::copy(split_point, body->data.end(), &ram[0]);
			}
			load_address += body->data.size();

			// Set final tape byte.
			ram[0xa7] = body->data.back();

			// Set 'ea' pointer.
			ram[0x9d] = load_address & 0xff;
			ram[0x9e] = load_address >> 8;

			registers.a = 0xa2;
			registers.x = load_address & 0xff;
			registers.y = load_address >> 8;
			registers.flags.template set_per<CPU::MOS6502Mk2::Flag::Carry>(0);	// C = 0 => success.

			ram[0x90] = 0;	// IO status: no error.
			ram[0x93] = 0;	// Load/verify flag: was load.

			// Tape timing constants.
			using WaveType = Storage::Tape::Commodore::WaveType;
			const float medium_length = parser.expected_length(WaveType::Medium);
			const float short_length = parser.expected_length(WaveType::Short);

			const float timer_ticks_per_second = float(clock_rate_) / float(timer_cycle_length.as<int>());
			const auto medium_cutoff = uint16_t((medium_length * timer_ticks_per_second) * 0.75f);
			const auto short_cutoff = uint16_t((short_length * timer_ticks_per_second) * 0.75f);

			ram[0x7b8] = uint8_t(short_cutoff);
			ram[0x7b9] = uint8_t(short_cutoff >> 8);

			ram[0x7bc] = ram[0x7ba] = uint8_t(medium_cutoff);
			ram[0x7bd] = ram[0x7bb] = uint8_t(medium_cutoff >> 8);

			m6502.set_registers(registers);
			return true;
		}

		tape_player_->serialiser()->set_offset(start_offset);
		return false;
	}

	template <typename M6502T, typename MemoryT>
	std::optional<AcceleratedRange> accelerated_range(const uint16_t pc, M6502T &, MemoryT &map) {
		// Potential sequence:
		//
		// 24 01	BIT 01
		// d0 fc	BNE 3c8		<- PC will be here; trigger is the BIT operation above.
		// 24 01	BIT 01
		// f0 fc	BEQ 3cc
		//
		// Also check for BNE and BEQ the other way around.
		static constexpr uint8_t bne_beq[] = {
			0x24, 0x01, 0xd0, 0xfc, 0x24, 0x01, 0xf0, 0xfc
		};
		static constexpr uint8_t beq_bne[] = {
			0x24, 0x01, 0xf0, 0xfc, 0x24, 0x01, 0xd0, 0xfc
		};
		const uint8_t *memory_begin = &map.write(pc - 2);	// TODO: formalise getting a block pointer on `map`.
		if(
			std::equal(std::begin(bne_beq), std::end(bne_beq), memory_begin) ||
			std::equal(std::begin(beq_bne), std::end(bne_beq), memory_begin)
		) {
			return AcceleratedRange{uint16_t(pc - 2), uint16_t(pc + 6)};
		}

		return std::nullopt;
	}

private:
	int clock_rate_ = 0;

	std::unique_ptr<Storage::Tape::BinaryTapePlayer> tape_player_;
	bool play_button_ = false;

	void set_component_prefers_clocking(ClockingHint::Source *, ClockingHint::Preference) override {
		set_use_fast_tape();
	}
	bool use_fast_tape_hack_ = false;
	bool allow_fast_tape_hack_ = false;
	bool rom_is_paged_ = false;
	void set_use_fast_tape() {
		use_fast_tape_hack_ = allow_fast_tape_hack_ && rom_is_paged_ && !tape_player_->is_at_end();
	}

	uint8_t io_output_ = 0x00;
	uint8_t io_direction_ = 0x00;
	void update_tape_motor() {
		const auto output = io_output_ | ~io_direction_;
		tape_player_->set_motor_control(play_button_ && (~output & 0x08));
	}
};

}
