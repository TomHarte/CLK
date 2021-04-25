//
//  SNA.cpp
//  Clock Signal
//
//  Created by Thomas Harte on 24/04/2021.
//  Copyright © 2021 Thomas Harte. All rights reserved.
//

#include "SNA.hpp"

#include "../FileHolder.hpp"

#include "../../Analyser/Static/ZXSpectrum/Target.hpp"
#include "../../Machines/Sinclair/ZXSpectrum/State.hpp"

using namespace Storage::State;

std::unique_ptr<Analyser::Static::Target> SNA::load(const std::string &file_name) {
	// Make sure the file is accessible and appropriately sized.
	FileHolder file(file_name);
	if(file.stats().st_size != 48*1024 + 0x1b) {
		return nullptr;
	}

	// SNAs are always for 48kb machines.
	using Target = Analyser::Static::ZXSpectrum::Target;
	auto result = std::make_unique<Target>();
	result->model = Target::Model::FortyEightK;

	// Prepare to populate ZX Spectrum state.
	auto *const state = new Sinclair::ZXSpectrum::State();
	result->state = std::unique_ptr<Reflection::Struct>(state);

	// Comments below: [offset] [contents]

	//	00	I
	const uint8_t i = file.get8();

	//	01	HL';	03	DE';	05	BC';	07	AF'
	state->z80.registers.hlDash = file.get16le();
	state->z80.registers.deDash = file.get16le();
	state->z80.registers.bcDash = file.get16le();
	state->z80.registers.afDash = file.get16le();

	//	09	HL;		0B	DE;		0D	BC;		0F	IY;		11	IX
	state->z80.registers.hl = file.get16le();
	state->z80.registers.de = file.get16le();
	state->z80.registers.bc = file.get16le();
	state->z80.registers.iy = file.get16le();
	state->z80.registers.ix = file.get16le();

	//	13	IFF2 (in bit 2)
	const uint8_t iff = file.get8();
	state->z80.registers.iff1 = state->z80.registers.iff2 = iff & 4;

	//	14	R
	const uint8_t r = file.get8();
	state->z80.registers.ir = uint16_t((i << 8) | r);

	//	15	AF;		17	SP;		19	interrupt mode
	state->z80.registers.flags = file.get8();
	state->z80.registers.a = file.get8();
	state->z80.registers.stack_pointer = file.get16le();
	state->z80.registers.interrupt_mode = file.get8();

	//	1A	border colour
	state->video.border_colour = file.get8();

	//	1B–	48kb RAM contents
	state->ram = file.read(48*1024);

	// To establish program counter, point it to a RET that
	// I know is in the 16/48kb ROM. This avoids having to
	// try to do a pop here, given that the true program counter
	// might currently be in the ROM.
	state->z80.registers.program_counter = 0x1d83;

	return result;
}
