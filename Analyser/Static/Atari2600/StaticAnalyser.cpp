//
//  StaticAnalyser.cpp
//  Clock Signal
//
//  Created by Thomas Harte on 15/09/2016.
//  Copyright 2016 Thomas Harte. All rights reserved.
//

#include "StaticAnalyser.hpp"

#include "Target.hpp"

#include "../Disassembler/6502.hpp"

using namespace Analyser::Static::Atari2600;
using Target = Analyser::Static::Atari2600::Target;

static void DeterminePagingFor2kCartridge(Target &target, const Storage::Cartridge::Cartridge::Segment &segment) {
	// if this is a 2kb cartridge then it's definitely either unpaged or a CommaVid
	uint16_t entry_address, break_address;

	entry_address = (static_cast<uint16_t>(segment.data[0x7fc] | (segment.data[0x7fd] << 8))) & 0x1fff;
	break_address = (static_cast<uint16_t>(segment.data[0x7fe] | (segment.data[0x7ff] << 8))) & 0x1fff;

	// a CommaVid start address needs to be outside of its RAM
	if(entry_address < 0x1800 || break_address < 0x1800) return;

	std::function<std::size_t(uint16_t address)> high_location_mapper = [](uint16_t address) {
		address &= 0x1fff;
		return static_cast<std::size_t>(address - 0x1800);
	};
	Analyser::Static::MOS6502::Disassembly high_location_disassembly =
		Analyser::Static::MOS6502::Disassemble(segment.data, high_location_mapper, {entry_address, break_address});

	// assume that any kind of store that looks likely to be intended for large amounts of memory implies
	// large amounts of memory
	bool has_wide_area_store = false;
	for(std::map<uint16_t, Analyser::Static::MOS6502::Instruction>::value_type &entry : high_location_disassembly.instructions_by_address) {
		if(entry.second.operation == Analyser::Static::MOS6502::Instruction::STA) {
			has_wide_area_store |= entry.second.addressing_mode == Analyser::Static::MOS6502::Instruction::Indirect;
			has_wide_area_store |= entry.second.addressing_mode == Analyser::Static::MOS6502::Instruction::IndexedIndirectX;
			has_wide_area_store |= entry.second.addressing_mode == Analyser::Static::MOS6502::Instruction::IndirectIndexedY;

			if(has_wide_area_store) break;
		}
	}

	// conclude that this is a CommaVid if it attempted to write something to the CommaVid RAM locations;
	// caveat: false positives aren't likely to be problematic; a false positive is a 2KB ROM that always addresses
	// itself so as to land in ROM even if mapped as a CommaVid and this code is on the fence as to whether it
	// attempts to modify itself but it probably doesn't
	if(has_wide_area_store) target.paging_model = Target::PagingModel::CommaVid;
}

static void DeterminePagingFor8kCartridge(Target &target, const Storage::Cartridge::Cartridge::Segment &segment, const Analyser::Static::MOS6502::Disassembly &disassembly) {
	// Activision stack titles have their vectors at the top of the low 4k, not the top, and
	// always list 0xf000 as both vectors; they do not repeat them, and, inexplicably, they all
	// issue an SEI as their first instruction (maybe some sort of relic of the development environment?)
	if(
		segment.data[4095] == 0xf0 && segment.data[4093] == 0xf0 && segment.data[4094] == 0x00 && segment.data[4092] == 0x00 &&
		(segment.data[8191] != 0xf0 || segment.data[8189] != 0xf0 || segment.data[8190] != 0x00 || segment.data[8188] != 0x00) &&
		segment.data[0] == 0x78
	) {
		target.paging_model = Target::PagingModel::ActivisionStack;
		return;
	}

	// make an assumption that this is the Atari paging model
	target.paging_model = Target::PagingModel::Atari8k;

	std::set<uint16_t> internal_accesses;
	internal_accesses.insert(disassembly.internal_stores.begin(), disassembly.internal_stores.end());
	internal_accesses.insert(disassembly.internal_modifies.begin(), disassembly.internal_modifies.end());
	internal_accesses.insert(disassembly.internal_loads.begin(), disassembly.internal_loads.end());

	int atari_access_count = 0;
	int parker_access_count = 0;
	int tigervision_access_count = 0;
	for(uint16_t address : internal_accesses) {
		uint16_t masked_address = address & 0x1fff;
		atari_access_count += masked_address >= 0x1ff8 && masked_address < 0x1ffa;
		parker_access_count += masked_address >= 0x1fe0 && masked_address < 0x1ff8;
	}
	for(uint16_t address: disassembly.external_stores) {
		uint16_t masked_address = address & 0x1fff;
		tigervision_access_count += masked_address == 0x3f;
	}

	if(parker_access_count > atari_access_count) target.paging_model = Target::PagingModel::ParkerBros;
	else if(tigervision_access_count > atari_access_count) target.paging_model = Target::PagingModel::Tigervision;
}

static void DeterminePagingFor16kCartridge(Target &target, const Storage::Cartridge::Cartridge::Segment &segment, const Analyser::Static::MOS6502::Disassembly &disassembly) {
	// make an assumption that this is the Atari paging model
	target.paging_model = Target::PagingModel::Atari16k;

	std::set<uint16_t> internal_accesses;
	internal_accesses.insert(disassembly.internal_stores.begin(), disassembly.internal_stores.end());
	internal_accesses.insert(disassembly.internal_modifies.begin(), disassembly.internal_modifies.end());
	internal_accesses.insert(disassembly.internal_loads.begin(), disassembly.internal_loads.end());

	int atari_access_count = 0;
	int mnetwork_access_count = 0;
	for(uint16_t address : internal_accesses) {
		uint16_t masked_address = address & 0x1fff;
		atari_access_count += masked_address >= 0x1ff6 && masked_address < 0x1ffa;
		mnetwork_access_count += masked_address >= 0x1fe0 && masked_address < 0x1ffb;
	}

	if(mnetwork_access_count > atari_access_count) target.paging_model = Target::PagingModel::MNetwork;
}

static void DeterminePagingFor64kCartridge(Target &target, const Storage::Cartridge::Cartridge::Segment &segment, const Analyser::Static::MOS6502::Disassembly &disassembly) {
	// make an assumption that this is a Tigervision if there is a write to 3F
	target.paging_model =
		(disassembly.external_stores.find(0x3f) != disassembly.external_stores.end()) ?
			Target::PagingModel::Tigervision : Target::PagingModel::MegaBoy;
}

static void DeterminePagingForCartridge(Target &target, const Storage::Cartridge::Cartridge::Segment &segment) {
	if(segment.data.size() == 2048) {
		DeterminePagingFor2kCartridge(target, segment);
		return;
	}

	uint16_t entry_address, break_address;

	entry_address = static_cast<uint16_t>(segment.data[segment.data.size() - 4] | (segment.data[segment.data.size() - 3] << 8));
	break_address = static_cast<uint16_t>(segment.data[segment.data.size() - 2] | (segment.data[segment.data.size() - 1] << 8));

	std::function<std::size_t(uint16_t address)> address_mapper = [](uint16_t address) {
		if(!(address & 0x1000)) return static_cast<std::size_t>(-1);
		return static_cast<std::size_t>(address & 0xfff);
	};

	std::vector<uint8_t> final_4k(segment.data.end() - 4096, segment.data.end());
	Analyser::Static::MOS6502::Disassembly disassembly = Analyser::Static::MOS6502::Disassemble(final_4k, address_mapper, {entry_address, break_address});

	switch(segment.data.size()) {
		case 8192:
			DeterminePagingFor8kCartridge(target, segment, disassembly);
		break;
		case 10495:
			target.paging_model = Target::PagingModel::Pitfall2;
		break;
		case 12288:
			target.paging_model = Target::PagingModel::CBSRamPlus;
		break;
		case 16384:
			DeterminePagingFor16kCartridge(target, segment, disassembly);
		break;
		case 32768:
			target.paging_model = Target::PagingModel::Atari32k;
		break;
		case 65536:
			DeterminePagingFor64kCartridge(target, segment, disassembly);
		break;
		default:
		break;
	}

	// check for a Super Chip. Atari ROM images [almost] always have the same value stored over RAM
	// regions; when they don't they at least seem to have the first 128 bytes be the same as the
	// next 128 bytes. So check for that.
	if(	target.paging_model != Target::PagingModel::CBSRamPlus &&
		target.paging_model != Target::PagingModel::MNetwork) {
		bool has_superchip = true;
		for(std::size_t address = 0; address < 128; address++) {
			if(segment.data[address] != segment.data[address+128]) {
				has_superchip = false;
				break;
			}
		}
		target.uses_superchip = has_superchip;
	}

	// check for a Tigervision or Tigervision-esque scheme
	if(target.paging_model == Target::PagingModel::None && segment.data.size() > 4096) {
		bool looks_like_tigervision = disassembly.external_stores.find(0x3f) != disassembly.external_stores.end();
		if(looks_like_tigervision) target.paging_model = Target::PagingModel::Tigervision;
	}
}

Analyser::Static::TargetList Analyser::Static::Atari2600::GetTargets(const Media &media, const std::string &file_name, TargetPlatform::IntType potential_platforms) {
	// TODO: sanity checking; is this image really for an Atari 2600?
	auto target = std::make_unique<Target>();
	target->confidence = 0.5;
	target->media.cartridges = media.cartridges;
	target->paging_model = Target::PagingModel::None;
	target->uses_superchip = false;

	// try to figure out the paging scheme
	if(!media.cartridges.empty()) {
		const auto &segments = media.cartridges.front()->get_segments();

		if(segments.size() == 1) {
			const Storage::Cartridge::Cartridge::Segment &segment = segments.front();
			DeterminePagingForCartridge(*target, segment);
		}
	}
	TargetList destinations;
	destinations.push_back(std::move(target));
	return destinations;
}
