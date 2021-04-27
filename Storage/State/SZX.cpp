//
//  SZX.cpp
//  Clock Signal
//
//  Created by Thomas Harte on 26/04/2021.
//  Copyright © 2021 Thomas Harte. All rights reserved.
//

#include "SZX.hpp"

#include "../FileHolder.hpp"

#include "../../Analyser/Static/ZXSpectrum/Target.hpp"
#include "../../Machines/Sinclair/ZXSpectrum/State.hpp"

#define LOG_PREFIX "[SZX] "
#include "../../Outputs/Log.hpp"

#include <zlib.h>

using namespace Storage::State;

std::unique_ptr<Analyser::Static::Target> SZX::load(const std::string &file_name) {
	FileHolder file(file_name);

	// Construct a target with a Spectrum state.
	using Target = Analyser::Static::ZXSpectrum::Target;
	auto result = std::make_unique<Target>();
	auto *const state = new Sinclair::ZXSpectrum::State();
	result->state = std::unique_ptr<Reflection::Struct>(state);

	// Check signature and major version number.
	if(!file.check_signature("ZXST")) {
		return nullptr;
	}
	const uint8_t major_version = file.get8();
	[[maybe_unused]] const uint8_t minor_version = file.get8();
	if(major_version > 1) {
		return nullptr;
	}

	// Check for a supported machine type.
	const uint8_t machine_type = file.get8();
	switch(machine_type) {
		default: return nullptr;

		case 0:		result->model = Target::Model::SixteenK;		break;
		case 1:		result->model = Target::Model::FortyEightK;		break;
		case 2:		result->model = Target::Model::OneTwoEightK;	break;
		case 3:		result->model = Target::Model::Plus2;			break;
		case 4:		result->model = Target::Model::Plus2a;			break;
		case 5:		result->model = Target::Model::Plus3;			break;
	}

	// Consequential upon selected machine...
	switch(result->model) {
		case Target::Model::SixteenK:		state->ram.resize(16 * 1024);	break;
		case Target::Model::FortyEightK:	state->ram.resize(48 * 1024);	break;
		default:
			state->ram.resize(128 * 1024);
		break;
	}

	const uint8_t file_flags = file.get8();
	[[maybe_unused]] const bool uses_late_timings = file_flags & 1;

	// Now parse all included blocks.
	while(true) {
		const uint32_t blockID = file.get32le();
		const uint32_t size = file.get32le();
		if(file.eof()) break;
		const auto location = file.tell();

#define BLOCK(str)	str[0] | (str[1] << 8) | (str[2] << 16) | (str[3] << 24)

		switch(blockID) {
			default:
				LOG("Unhandled block " << char(blockID) << char(blockID >> 8) << char(blockID >> 16) << char(blockID >> 24));
			break;

			// ZXSTZ80REGS
			case BLOCK("Z80R"): {
				state->z80.registers.flags = file.get8();
				state->z80.registers.a = file.get8();

				state->z80.registers.bc = file.get16le();
				state->z80.registers.de = file.get16le();
				state->z80.registers.hl = file.get16le();

				state->z80.registers.af_dash = file.get16le();
				state->z80.registers.bc_dash = file.get16le();
				state->z80.registers.de_dash = file.get16le();
				state->z80.registers.hl_dash = file.get16le();

				state->z80.registers.ix = file.get16le();
				state->z80.registers.iy = file.get16le();
				state->z80.registers.stack_pointer = file.get16le();
				state->z80.registers.program_counter = file.get16le();

				const uint8_t i = file.get8();
				const uint8_t r = file.get8();
				state->z80.registers.ir = uint16_t((i << 8) | r);

				state->z80.registers.iff1 = file.get8();
				state->z80.registers.iff2 = file.get8();
				state->z80.registers.interrupt_mode = file.get8();

				state->video.half_cycles_since_interrupt = int(file.get32le()) * 2;

				// SZX includes a count of remaining cycles that interrupt should be asserted for
				// because it supports hardware that might cause an interrupt other than the display.
				// This emulator doesn't, so this field can be ignored.
				[[maybe_unused]] uint8_t remaining_interrupt_cycles = file.get8();


				const uint8_t flags = file.get8();
				state->z80.execution_state.is_halted = flags & 2;
				// TODO: bit 0 indicates that the last instruction was an EI, or an invalid
				// DD or FD.  I assume I'm supposed to use that to conclude an interrupt
				// verdict but I'm unclear what the effect of an invalid DD or FD is so
				// have not yet implemented this.

				state->z80.registers.memptr = file.get16le();
			} break;

			// ZXSTAYBLOCK
			case BLOCK("AY\0\0"): {
				// This applies to 48kb machines with AY boxes only. This emulator
				// doesn't currently support those.
				[[maybe_unused]] const uint8_t interface_type = file.get8();

				state->ay.selected_register = file.get8();
				file.read(state->ay.registers, 16);
			} break;

			// ZXSTRAMPAGE
			case BLOCK("RAMP"): {
				const uint16_t flags = file.get16le();
				const uint8_t page = file.get8();

				std::vector<uint8_t> contents;
				if(flags & 1) {
					// ZLib compression is applied.
					contents.resize(16 * 1024);
					const std::vector<uint8_t> source = file.read(size - 3);

					uLongf output_length;
					uncompress(contents.data(), &output_length, source.data(), source.size());
					assert(output_length == contents.size());
				} else {
					// Data is raw.
					contents = file.read(16 * 1024);
				}

				switch(result->model) {
					case Target::Model::SixteenK:
					case Target::Model::FortyEightK: {
						size_t address = 0;
						switch(page) {
							default: break;
							case 5: address = 0x4000;	break;
							case 2: address = 0x8000;	break;
							case 0: address = 0xc000;	break;
						}

						if(address > 0 && (address - 0x4000) <= state->ram.size()) {
							memcpy(&state->ram[address - 0x4000], contents.data(), 0x4000);
						}
					} break;

					default:
						if(page < 8) {
							memcpy(&state->ram[page * 0x4000], contents.data(), 0x4000);
						}
					break;
				}
			} break;

			// ZXSTSPECREGS
			case BLOCK("SPCR"): {
				state->video.border_colour = file.get8();
				state->last_7ffd = file.get8();
				state->last_1ffd = file.get8();

				// TODO: use last write to FE, at least.
			} break;
		}

#undef BLOCK

		// Advance to the next block.
		file.seek(location + size, SEEK_SET);
	}

	return result;
}
