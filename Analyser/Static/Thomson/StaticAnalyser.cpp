//
//  StaticAnalyser.cpp
//  Clock Signal
//
//  Created by Thomas Harte on 23/03/2026.
//  Copyright © 2026 Thomas Harte. All rights reserved.
//

#include "StaticAnalyser.hpp"
#include "Target.hpp"

#include "Storage/Tape/Parsers/ThomsonMO.hpp"

Analyser::Static::TargetList Analyser::Static::Thomson::GetTargets(
	const Media &media,
	const std::string &,
	TargetPlatform::IntType,
	bool
) {
	using MOTarget = Analyser::Static::Thomson::MOTarget;

	TargetList destination;
	auto target = std::make_unique<MOTarget>();

	if(!media.tapes.empty()) {
		Storage::Tape::Thomson::MO::Parser parser;
		auto &tape = media.tapes.front();
		const auto serialiser = tape->serialiser();
		const auto first = parser.block(*serialiser);

		if(first && first->checksum_valid()) {
			target->media.tapes = media.tapes;

			// Cf. https://pulkomandy.tk/wiki/doku.php?id=documentations:monitor:tape.format for leader block format;
			// my parser provides block type separately and length implicitly so the type at offset 0xd is at 0xb
			// in ->data.
			static constexpr size_t TypeOffset = 0xb;
			if(!first->type && first->data.size() > TypeOffset) {
				if(!first->data[TypeOffset]) {	// File type; 0 = BASIC, 1 = DATA; 2 = binary.
					target->loading_command = L"RUN\"\n";
				} else {
					target->loading_command = L"LOADM\"\",,R\n";
				}
			}
		}
	}

	// Currently: accept all floppy disks and all cartridges.
	if(!media.disks.empty()) {
		target->floppy = MOTarget::Floppy::CD90_640;
		target->media.disks = media.disks;
	}

	if(!media.cartridges.empty()) {
		target->media.cartridges = media.cartridges;
	}

	if(!target->media.empty()) {
		destination.push_back(std::move(target));
	}
	return destination;
}
