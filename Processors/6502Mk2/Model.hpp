//
//  Model.hpp
//  Clock Signal
//
//  Created by Thomas Harte on 20/10/2025.
//  Copyright © 2025 Thomas Harte. All rights reserved.
//

#pragma once

namespace CPU::MOS6502Mk2 {

enum Model {
	NES6502,			// The NES's 6502; like a 6502 but lacking decimal mode (though it retains the decimal flag).
	M6502,				// NMOS 6502.
	Synertek65C02,		// A 6502 extended with BRA, P[H/L][X/Y], STZ, TRB, TSB and the (zp) addressing mode, and more.
	Rockwell65C02,		// The Synertek extended with BBR, BBS, RMB and SMB.
	WDC65C02,			// The Rockwell extended with STP and WAI.
	M65816,				// The "16-bit" successor to the 6502.
};
constexpr bool has_decimal_mode(const Model model) { return model != Model::NES6502; }
constexpr bool is_8bit(const Model model) { return model <= Model::WDC65C02; }
constexpr bool is_16bit(const Model model) { return model == Model::M65816; }
constexpr bool is_65c02(const Model model) { return model >= Model::Synertek65C02; }
constexpr bool is_6502(const Model model) { return model <= Model::M6502; }

}
