//
//  LoricielsBactron.hpp
//  Clock Signal
//
//  Created by Thomas Harte on 26/04/2026.
//  Copyright © 2026 Thomas Harte. All rights reserved.
//

#pragma once

#include <algorithm>

#ifndef NDEBUG
#include <unordered_set>
#endif

namespace Thomson::FastLoader {

/*

	Fast loader used by, at least:

		(i) Bactron and MGT, in identical versions;
		(ii) Pulsar II, in a slight modification.

	Sampling code for (i) is:

		Z35CA	LDA     ,U                       ;35CA: A6 C4          '..'
				LSRA                             ;35CC: 44             'D'
				STA     ,-S                      ;35CD: A7 E2          '..'
				STA     ,S                       ;35CF: A7 E4          '..'		; [S], [S+1] = tape in b6
				LDA     ,U                       ;35D1: A6 C4          '..'
				LSRA                             ;35D3: 44             'D'
				ADDA    ,S                       ;35D4: AB E4          '..'
				STA     ,S                       ;35D6: A7 E4          '..'		; [S] = two-bit tape average in b6:b7
				LDA     ,U                       ;35D8: A6 C4          '..'
				LSRA                             ;35DA: 44             'D'
				ADDA    ,S+                      ;35DB: AB E0          '..'
				EORA    M3507                    ;35DD: B8 35 07       '.5.'
				RTS                              ;35E0: 39             '9'

	Per-bit loop is:

		Z35AD	BSR     Z35CA                    ;35AD: 8D 1B          '..'
				BPL     Z35AD                    ;35AF: 2A FC          '*.'		; loop until sample differs from 3507
				LDX     #M0041                   ;35B1: 8E 00 41       '..A'	; bit length
				BSR     Z35C4                    ;35B4: 8D 0E          '..'		; fixed delay: 7µs + [as per 35C4]
				BSR     Z35CA                    ;35B6: 8D 12          '..'		; sample again for bit
				BPL     Z35BF                    ;35B8: 2A 05          '*.'		; BPL -> 43 = COMA, which sets carry
				COM     M3507                    ;35BA: 73 35 07       's5.'
				CLRA                             ;35BD: 4F             'O'		; clear carry
				BRN     Z3603                    ;35BE: 21 43          '!C'
				ROL     M3508                    ;35C0: 79 35 08       'y5.'
				RTS                              ;35C3: 39             '9'

	Framing also requires, initially:

		Z3580	BSR     Z35AD                    ;3580: 8D 2B          '.+'
				TSTA                             ;3582: 4D             'M'
				BNE     Z3580                    ;3583: 26 FB          '&.'		; loop until a 0 is encountered.

	Delay loop is:

		Z35C4	DEX                              ;35C4: 30 1F          '0.'		; 5 µs
				BNE     Z35C4                    ;35C6: 26 FC          '&.'		; 3 µs
				CLRA                             ;35C8: 4F             'O'		; 2 µs
				RTS                              ;35C9: 39             '9'		; 5 µs

			i.e. length is 8*X + 7 µs.


	Differences for (ii):

		* the smpling Z35CA routine is at Z5F63 and the three-byte EOR M3507 has become a two-byte EOR M00A4;
		* the per-bit Z35AD routine is at Z5F48, subject to different call targets and M3508 now being M00A5;
		* the Z35C4 delay loop is relocated to Z5F5D;
		* there's similar framing logic from Z5F1D.

	So the full per-bit is:

		Z5F48	BSR     Z5F63                    ;5F48: 8D 19          '..'
				BPL     Z5F48                    ;5F4A: 2A FC          '*.'
				LDX     #M0041                   ;5F4C: 8E 00 41       '..A'
				BSR     Z5F5D                    ;5F4F: 8D 0C          '..'
				BSR     Z5F63                    ;5F51: 8D 10          '..'
				BPL     Z5F59                    ;5F53: 2A 04          '*.'
				COM     M00A4                    ;5F55: 03 A4          '..'
				CLRA                             ;5F57: 4F             'O'
				BRN     Z5F9D                    ;5F58: 21 43          '!C'
				ROL     M00A5                    ;5F5A: 09 A5          '..'
				RTS                              ;5F5C: 39             '9'

*/
struct LoricielsBactron: public Loader {
	static constexpr uint8_t sampler[] = {
		0xa6, 0xc4, 0x44, 0xa7, 0xe2, 0xa7, 0xe4, 0xa6, 0xc4, 0x44, 0xab, 0xe4, 0xa7, 0xe4, 0xa6, 0xc4,
		0x44, 0xab, 0xe0, /* 0xb8, 0x35, 0x07, 0x39, */
	};

	static constexpr uint8_t bactron_outer[] = {
		0x8d, 0x1b, 0x2a, 0xfc, 0x8e, 0x00, 0x41, 0x8d, 0x0e, 0x8d, 0x12
	};

	static constexpr uint8_t pulsar2_outer[] = {
		0x8d, 0x19, 0x2a, 0xfc, 0x8e, 0x00, 0x41, 0x8d, 0x0c, 0x8d, 0x10
	};

	static std::optional<uint16_t> detect(const uint16_t address, const MemoryAccess &memory) {
		if(
			address > 2 &&
			std::equal(
				std::begin(sampler),
				std::end(sampler),
				memory.iterator(address - 2)
			)
		) {
			switch(address) {
				case 0x35cc: return 0x35ad;	// Bactron, MGT.
				case 0x5f65: return 0x5f48;	// Pulsar II.
				case 0x9d76: return 0x9d59;	// Yeti.
				case 0x5f66: return 0x5f49;	// Eliminator
				case 0x3459: return 0x343a;	// Mach 3
				case 0x402b: return 0x400c; // Space Racer
				default:
#ifndef NDEBUG
					if(logged_.find(address) == logged_.end()) {
						logged_.insert(address);

						printf("Probable relocated Loriciels Bactron at %04x\n", address);

						printf("%04x:\n", address - 0x200);
						for(int c = address - 0x200; c < address + 0x200; c++) {
							printf("%02x ", memory.read(c));
						}
						printf("\n");
					}
#endif
				break;
			}
		}
		return std::nullopt;
	}

	std::pair<TrapAction, bool> did_trap(
		const uint16_t address,
		MemoryAccess &memory,
		CPU::M6809::Registers &registers,
		Storage::Tape::BinaryTapePlayer &player,
		Storage::Tape::TapeSerialiser &
	) override {
		// Check that this is still the expected routine.
		if(!std::equal(
				std::begin(bactron_outer),
				std::end(bactron_outer),
				memory.iterator(address)
			) &&
			!std::equal(
				std::begin(pulsar2_outer),
				std::end(pulsar2_outer),
				memory.iterator(address)
			)
		) {
			return std::make_pair(TrapAction::None, false);
		}

		enum class Type {
			Bactron,
			PulsarII,
			Yeti,
			Eliminator,
			Mach3,
			SpaceRacer,
		};

		const auto direct_page = uint16_t(registers.reg<CPU::M6809::R8::DP>() << 8);
		const auto type = [&]() {
			switch(address) {
				case 0x35ad:	return Type::Bactron;
				case 0x5f48:	return Type::PulsarII;
				case 0x9d59:	return Type::Yeti;
				case 0x5f49:	return Type::Eliminator;
				case 0x343a:	return Type::Mach3;
				case 0x400c:	return Type::SpaceRacer;
				default: __builtin_unreachable();
			}
		} ();
		uint8_t &level = [&]() -> uint8_t & {
			switch(type) {
				case Type::Bactron:		return memory[0x3507];
				case Type::PulsarII:	return memory[direct_page | 0xa4];
				case Type::Yeti:		return memory[direct_page | 0xb3];
				case Type::Eliminator:	return memory[direct_page | 0xa5];
				case Type::Mach3:		return memory[0x3394];
				case Type::SpaceRacer:	return memory[0x3f66];
				default: __builtin_unreachable();
			}
		} ();
		uint8_t &byte = [&]() -> uint8_t & {
			switch(type) {
				case Type::Bactron:		return memory[0x3508];
				case Type::PulsarII:	return memory[direct_page | 0xa5];
				case Type::Yeti:		return memory[direct_page | 0xb4];
				case Type::Eliminator:	return memory[direct_page | 0xa6];
				case Type::Mach3:		return memory[0x3395];
				case Type::SpaceRacer:	return memory[0x3f67];
				default: __builtin_unreachable();
			}
		} ();

		// Sample current level.
		const bool start_level = level & 0x80;

		// Proceed to next edge; time can be skipped here.
		while(player.input() == start_level) {
			player.complete_pulse();
		}

		// Wait for 14 + 8*(0x41) µs.
		player.run_for(Storage::Time(14 + 8*0x41, 1'000'000));

		// Sample current level to determine bit, shift and complement as appropriate.
		const bool final_level = player.input();
		byte <<= 1;
		const bool is_one = final_level == start_level;
		if(is_one) {
			byte |= 1;
		} else {
			level ^= 0xff;
		}

		registers.reg<CPU::M6809::R8::A>() = is_one ? ~registers.reg<CPU::M6809::R8::A>() : 0;

		return std::make_pair(TrapAction::RTS, true);
	}

#ifndef NDEBUG
private:
	inline static std::unordered_set<uint16_t> logged_;
#endif
};

}
