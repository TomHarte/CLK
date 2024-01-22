//
//  Cartridge.hpp
//  Clock Signal
//
//  Created by Thomas Harte on 25/01/2018.
//  Copyright 2018 Thomas Harte. All rights reserved.
//

#pragma once

#include "../../../Storage/Cartridge/Cartridge.hpp"

namespace Analyser::Static::MSX {

/*!
	Extends the base cartridge class by adding a (guess at) the banking scheme.
*/
struct Cartridge: public ::Storage::Cartridge::Cartridge {
	enum Type {
		None,
		Konami,
		KonamiWithSCC,
		ASCII8kb,
		ASCII16kb,
		FMPac
	};
	const Type type;

	Cartridge(const std::vector<Segment> &segments, Type type) :
		Storage::Cartridge::Cartridge(segments), type(type) {}
};

}
