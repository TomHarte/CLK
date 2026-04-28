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

static constexpr bool DefaultMO6 = true;	// Indicates whether to load content on an MO5 or MO6.
static constexpr bool DumpFiles = true;		// Helpful to me for inspecting tape contents. Very ugly. I apologise.

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
		using namespace Storage::Tape::Thomson::MO;

		Parser parser;
		auto &tape = media.tapes.front();
		const auto serialiser = tape->serialiser();

		// Look for a first file.
		while(!serialiser->is_at_end()) {
			const auto file = parser.file(*serialiser);
			if(file && file->checksums_valid) {
				target->media.tapes = media.tapes;
				target->loading_command = file->type != File::Type::BASIC ? L"LOADM\"\",,R" : L"RUN\"";

				// TODO: determine whether BASIC 1 or BASIC 128 is appropriate.
				// Note on that: Microsoft BASIC provides optional 'protection', in which case the file on tape is
				// encrypted. That's currently obscuring efforts here. Will need to figure out how to
				// reverse-engineer that.
				break;
			}
		}

		// Failing that, look for any valid block and perform a RUN.
		if(target->media.tapes.empty()) {
			serialiser->reset();
			const auto block = parser.block(*serialiser);
			if(block.has_value()) {
				target->media.tapes = media.tapes;
				target->loading_command = L"LOADM\"\",,R";
			}
		}

		// Decorate loading command according to machine.
		if(!target->media.tapes.empty()) {
			if(DefaultMO6) {
				target->loading_command = std::wstring(L"2                ") + target->loading_command;
			}
			target->loading_command += '\n';
		}

		// Possibly provide further insight
		if constexpr (DumpFiles) {
			serialiser->reset();
			while(true) {
				auto next = parser.file(*serialiser);
				if(!next.has_value()) break;

				int c = 0;
				for(auto &b : next->data) {
					printf("%02x ", b);
					c++;
					if(!(c & 15)) printf("\n");
				}
				printf("\n\n");
			}
		}
	}

	// Currently: accept all floppy disks.
	// TODO: verify filing system structure. That'll allow for things like HFEs to be included in the automatic set.
	if(!media.disks.empty()) {
		target->floppy = MOTarget::Floppy::CD90_640;
		target->media.disks = media.disks;
		if(DefaultMO6) {
			target->loading_command = L"2";
		}
	}

	if(!media.cartridges.empty()) {
		target->media.cartridges = validated(media.cartridges);
	}


	// Fallback: if this is the only potential target but nothing seemed relevant, accept everything.
	if(target->media.empty() && is_confident) {
		target->media = media;
	}

	// It seems there are no catridges that require or do anything additional on an MO6.
	// So use an MO5 for faster launch.
	if(!target->media.cartridges.empty()) {
		target->model = MOTarget::Model::MO5v11;
	}


	if(!target->media.empty()) {
		destination.push_back(std::move(target));
	}
	return destination;
}
