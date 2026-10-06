//
//  SNA.cpp
//  Clock Signal
//
//  Created by Thomas Harte on 24/04/2021.
//  Copyright © 2021 Thomas Harte. All rights reserved.
//

#include "SNA.hpp"

#include "Storage/FileHolder.hpp"

#include "Analyser/Static/ZXSpectrum/Target.hpp"
#include "Machines/Sinclair/ZXSpectrum/State.hpp"

using namespace Storage::State;

std::unique_ptr<Analyser::Static::Target> SNA::load(const std::filesystem::path &file_name) {
	// Make sure the file is accessible and appropriately sized.
	FileHolder file(file_name);

	static constexpr size_t InfoBlock48k = 0x1b;
	static constexpr size_t ExtraInfo128k = 4;
	static constexpr size_t Size48k = 48*1024 + InfoBlock48k;
	static constexpr size_t Size128kNoRepeat = 128*1024 + InfoBlock48k + ExtraInfo128k;
	static constexpr size_t Size128kRepeat = 144*1024 + InfoBlock48k + ExtraInfo128k;

	const auto file_size = file.stats().st_size;
	if(file_size != Size48k && file_size != Size128kNoRepeat && file_size != Size128kRepeat) {
		return nullptr;
	}

	// Use file size to determine target machine.
	using Target = Analyser::Static::ZXSpectrum::Target;
	auto result = std::make_unique<Target>();
	const bool is_48k = file_size == Size48k;
	result->model = is_48k ? Target::Model::FortyEightK : Target::Model::Plus2;

	// Prepare to populate ZX Spectrum state.
	auto *const state = new Sinclair::ZXSpectrum::State();
	result->state = std::unique_ptr<Reflection::Struct>(state);

	// Comments below: [offset] [contents]

	//	00	I
	const uint8_t i = file.get();

	//	01	HL';	03	DE';	05	BC';	07	AF'
	state->z80.registers.hl_dash = file.get_le<uint16_t>();
	state->z80.registers.de_dash = file.get_le<uint16_t>();
	state->z80.registers.bc_dash = file.get_le<uint16_t>();
	state->z80.registers.af_dash = file.get_le<uint16_t>();

	//	09	HL;		0B	DE;		0D	BC;		0F	IY;		11	IX
	state->z80.registers.hl = file.get_le<uint16_t>();
	state->z80.registers.de = file.get_le<uint16_t>();
	state->z80.registers.bc = file.get_le<uint16_t>();
	state->z80.registers.iy = file.get_le<uint16_t>();
	state->z80.registers.ix = file.get_le<uint16_t>();

	//	13	IFF2 (in bit 2)
	const uint8_t iff = file.get();
	state->z80.registers.iff1 = state->z80.registers.iff2 = iff & 4;

	//	14	R
	const uint8_t r = file.get();
	state->z80.registers.ir = uint16_t((i << 8) | r);

	//	15	AF;		17	SP;		19	interrupt mode
	state->z80.registers.flags = file.get();
	state->z80.registers.a = file.get();
	state->z80.registers.stack_pointer = file.get_le<uint16_t>();
	state->z80.registers.interrupt_mode = file.get();

	//	1A	border colour
	state->video.border_colour = file.get();

	//	1B–	48kb RAM contents
	state->ram = file.read(48*1024);

	if(is_48k) {
		// To establish program counter, point it to a RET that
		// I know is in the 16/48kb ROM. This avoids having to
		// try to do a pop here, given that the true program counter
		// might currently be in the ROM.
		state->z80.registers.program_counter = 0x1d83;
		return result;
	}

	state->z80.registers.program_counter = file.get_le<uint16_t>();
	state->last_7ffd = file.get();

	const bool trdos_paged = file.get();
	if(trdos_paged) {
		// This emulator does not currently support TR DOS.
		return nullptr;
	}

	std::vector<uint8_t> base_ram = state->ram;
	state->ram.resize(128 * 1024);

	// 48k pages are 5, 2 and whatever port 7ffd was told, in that order.
	const auto paged_bank = state->last_7ffd & 7;
	const auto copy_to = [&](auto start, const size_t page) {
		std::copy(start, start + 0x4000, &state->ram[page * 0x4000]);
	};
	copy_to(base_ram.begin() + 0x0000, 5);
	copy_to(base_ram.begin() + 0x4000, 2);
	copy_to(base_ram.begin() + 0x8000, size_t(paged_bank));

	// Other banks follow.
	for(int c = 0; c < 8; c++) {
		if(c == 5 || c == paged_bank || c == 2) {
			continue;
		}

		const auto next_bank = file.read(0x4000);
		copy_to(next_bank.begin(), size_t(c));
	}

	return result;
}
