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

namespace {

using CartridgeList = std::vector<std::shared_ptr<Storage::Cartridge::Cartridge>>;
CartridgeList validated(const CartridgeList &cartridges) {
	//
	// This is an adhoc, empirical take on detecting cartridge validity.
	// For the MO series it looks like:
	//
	//	(1) the final two bytes of the cartridge are an entry vector, so should be in
	//	the range [b000, f000); and
	//	(2) a cartridge name begins 32 bytes before the end of the cartridge, and is at
	//	most 20 bytes long including a terminator of value 0x04.
	//

	CartridgeList result;
	for(const auto &cartridge: cartridges) {
		if(cartridge->segments().empty()) {
			continue;
		}
		auto &front = cartridge->segments().front().data;
		if(front.size() & 16383) {
			continue;
		}

		const auto vector = (front[16382] << 8) | front[16383];
		if(vector < 0xb000 || vector > 0xefe0) {
			continue;
		}

		const bool has_valid_string = [&]() {
			for(size_t c = 0; c < 20; c++) {
				const auto next = front[16384 - 32 + c];
				if(next & 0x80) return false;
				if(next == 0x04) return true;
			}
			return false;
		} ();

		if(!has_valid_string) {
			continue;
		}

		result.push_back(cartridge);
	}
	return result;
}

static constexpr bool DefaultMO6 = true;

}

Analyser::Static::TargetList Analyser::Static::Thomson::GetTargets(
	const Media &media,
	const std::string &,
	TargetPlatform::IntType,
	const bool is_confident
) {
	using MOTarget = Analyser::Static::Thomson::MOTarget;

	TargetList destination;
	auto target = std::make_unique<MOTarget>();

	if(DefaultMO6) {
		target->model = MOTarget::Model::MO6v3;
	}

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
				const auto command = [&]() -> std::wstring {
					if(!first->data[TypeOffset]) {	// File type; 0 = BASIC, 1 = DATA; 2 = binary.
						return L"RUN\"";
					} else {
						return L"LOADM\"\",,R";
					}
				};

				if(DefaultMO6) {
					target->loading_command = std::wstring(L"1    ") + command() + L"\n";
				} else {
					target->loading_command = command() + L"\n";
				}
			}
		}
	}

	// Currently: accept all floppy disks and all cartridges.
	if(!media.disks.empty()) {
		target->floppy = MOTarget::Floppy::CD90_640;
		target->media.disks = media.disks;
		if(DefaultMO6) {
			target->loading_command = L"1";
		}
	}

	if(!media.cartridges.empty()) {
		target->media.cartridges = is_confident ? media.cartridges : validated(media.cartridges);

		// Upon survey, it seemed that there are no catridges that require or that do anything additional on an MO6.
		// So switch to an MO5 for faster launch.
		if(!target->media.cartridges.empty()) {
			target->model = MOTarget::Model::MO5v11;
		}
	}

	if(!target->media.empty()) {
		destination.push_back(std::move(target));
	}
	return destination;
}
