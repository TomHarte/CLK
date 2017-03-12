//
//  StaticAnalyser.cpp
//  Clock Signal
//
//  Created by Thomas Harte on 15/09/2016.
//  Copyright © 2016 Thomas Harte. All rights reserved.
//

#include "StaticAnalyser.hpp"

#include "../Disassembler/Disassembler6502.hpp"

using namespace StaticAnalyser::Atari;

static void DeterminePagingFor2kCartridge(StaticAnalyser::Target &target, const Storage::Cartridge::Cartridge::Segment &segment)
{
	// if this is a 2kb cartridge then it's definitely either unpaged or a CommaVid
	uint16_t entry_address, break_address;

	entry_address = ((uint16_t)(segment.data[0x7fc] | (segment.data[0x7fd] << 8))) & 0x1fff;
	break_address = ((uint16_t)(segment.data[0x7fe] | (segment.data[0x7ff] << 8))) & 0x1fff;

	// a CommaVid start address needs to be outside of its RAM
	if(entry_address < 0x1800 || break_address < 0x1800) return;

	std::function<size_t(uint16_t address)> high_location_mapper = [](uint16_t address) {
		address &= 0x1fff;
		return (size_t)(address - 0x1800);
	};
	std::function<size_t(uint16_t address)> full_range_mapper = [](uint16_t address) {
		if(!(address & 0x1000)) return (size_t)-1;
		return (size_t)(address & 0x7ff);
	};

	StaticAnalyser::MOS6502::Disassembly high_location_disassembly =
		StaticAnalyser::MOS6502::Disassemble(segment.data, high_location_mapper, {entry_address, break_address});

	// if there are no subroutines in the top 2kb of memory then this isn't a CommaVid
	bool has_appropriate_subroutine_calls = false;
	bool has_inappropriate_subroutine_calls = false;
	for(uint16_t address : high_location_disassembly.internal_calls)
	{
		const uint16_t masked_address = address & 0x1fff;
		has_appropriate_subroutine_calls |= (masked_address >= 0x1800);
		has_inappropriate_subroutine_calls |= (masked_address < 0x1800);
	}

	// assumption here: a CommaVid will never branch into RAM. Possibly unsafe: if it won't then what's the RAM for?
	if(!has_appropriate_subroutine_calls || has_inappropriate_subroutine_calls) return;

	std::set<uint16_t> all_writes = high_location_disassembly.external_stores;
	all_writes.insert(high_location_disassembly.external_modifies.begin(), high_location_disassembly.external_modifies.end());

	// a CommaVid will use its RAM
	if(all_writes.empty()) return;

	bool has_appropriate_accesses = false;
	for(uint16_t address : all_writes)
	{
		const uint16_t masked_address = address & 0x1fff;
		if(masked_address >= 0x1400 && masked_address < 0x1800)
		{
			has_appropriate_accesses = true;
			break;
		}
	}

	// in desperation, accept any kind of store that looks likely to be intended for large amounts of memory
	bool has_wide_area_store = false;
	if(!has_appropriate_accesses)
	{
		for(std::map<uint16_t, StaticAnalyser::MOS6502::Instruction>::value_type &entry : high_location_disassembly.instructions_by_address)
		{
			if(entry.second.operation == StaticAnalyser::MOS6502::Instruction::STA)
			{
				has_wide_area_store |= entry.second.addressing_mode == StaticAnalyser::MOS6502::Instruction::Indirect;
				has_wide_area_store |= entry.second.addressing_mode == StaticAnalyser::MOS6502::Instruction::IndexedIndirectX;
				has_wide_area_store |= entry.second.addressing_mode == StaticAnalyser::MOS6502::Instruction::IndirectIndexedY;
			}
		}
	}

	// conclude that this is a CommaVid if it attempted to write something to the CommaVid RAM locations;
	// caveat: false positives aren't likely to be problematic; a false positive is a 2KB ROM that always addresses
	// itself so as to land in ROM even if mapped as a CommaVid and this code is on the fence as to whether it
	// attempts to modify itself but it probably doesn't
	if(has_appropriate_accesses || has_wide_area_store)
		target.atari.paging_model = StaticAnalyser::Atari2600PagingModel::CommaVid;
}

static void DeterminePagingFor8kCartridge(StaticAnalyser::Target &target, const Storage::Cartridge::Cartridge::Segment &segment, const StaticAnalyser::MOS6502::Disassembly &disassembly)
{
	// make an assumption that this is the Atari paging model
	target.atari.paging_model = StaticAnalyser::Atari2600PagingModel::Atari8k;

	std::set<uint16_t> internal_accesses;
	internal_accesses.insert(disassembly.internal_stores.begin(), disassembly.internal_stores.end());
	internal_accesses.insert(disassembly.internal_modifies.begin(), disassembly.internal_modifies.end());
	internal_accesses.insert(disassembly.internal_loads.begin(), disassembly.internal_loads.end());

	int atari_access_count = 0;
	int parker_access_count = 0;
	int tigervision_access_count = 0;
	for(uint16_t address : internal_accesses)
	{
		uint16_t masked_address = address & 0x1fff;
		atari_access_count += masked_address >= 0x1ff8 && masked_address < 0x1ffa;
		parker_access_count += masked_address >= 0x1fe0 && masked_address < 0x1ff8;
	}
	for(uint16_t address: disassembly.external_stores)
	{
		uint16_t masked_address = address & 0x1fff;
		tigervision_access_count += masked_address == 0x3f;
	}

	if(parker_access_count > atari_access_count) target.atari.paging_model = StaticAnalyser::Atari2600PagingModel::ParkerBros;
	else if(tigervision_access_count > atari_access_count) target.atari.paging_model = StaticAnalyser::Atari2600PagingModel::Tigervision;
}

static void DeterminePagingFor16kCartridge(StaticAnalyser::Target &target, const Storage::Cartridge::Cartridge::Segment &segment, const StaticAnalyser::MOS6502::Disassembly &disassembly)
{
	// make an assumption that this is the Atari paging model
	target.atari.paging_model = StaticAnalyser::Atari2600PagingModel::Atari16k;

	std::set<uint16_t> internal_accesses;
	internal_accesses.insert(disassembly.internal_stores.begin(), disassembly.internal_stores.end());
	internal_accesses.insert(disassembly.internal_modifies.begin(), disassembly.internal_modifies.end());
	internal_accesses.insert(disassembly.internal_loads.begin(), disassembly.internal_loads.end());

	int atari_access_count = 0;
	int mnetwork_access_count = 0;
	for(uint16_t address : internal_accesses)
	{
		uint16_t masked_address = address & 0x1fff;
		atari_access_count += masked_address >= 0x1ff6 && masked_address < 0x1ffa;
		mnetwork_access_count += masked_address >= 0x1fe0 && masked_address < 0x1ffb;
	}

	if(mnetwork_access_count > atari_access_count) target.atari.paging_model = StaticAnalyser::Atari2600PagingModel::MNetwork;
}

static void DeterminePagingForCartridge(StaticAnalyser::Target &target, const Storage::Cartridge::Cartridge::Segment &segment)
{
	if(segment.data.size() == 2048)
	{
		DeterminePagingFor2kCartridge(target, segment);
		return;
	}

	uint16_t entry_address, break_address;

	entry_address = (uint16_t)(segment.data[segment.data.size() - 4] | (segment.data[segment.data.size() - 3] << 8));
	break_address = (uint16_t)(segment.data[segment.data.size() - 2] | (segment.data[segment.data.size() - 1] << 8));

	std::function<size_t(uint16_t address)> address_mapper = [](uint16_t address) {
		if(!(address & 0x1000)) return (size_t)-1;
		return (size_t)(address & 0xfff);
	};

	std::vector<uint8_t> final_4k(segment.data.end() - 4096, segment.data.end());
	StaticAnalyser::MOS6502::Disassembly disassembly = StaticAnalyser::MOS6502::Disassemble(final_4k, address_mapper, {entry_address, break_address});

	switch(segment.data.size())
	{
		case 8192:
			DeterminePagingFor8kCartridge(target, segment, disassembly);
		break;
		case 12288:
			target.atari.paging_model = StaticAnalyser::Atari2600PagingModel::CBSRamPlus;
		break;
		case 16384:
			DeterminePagingFor16kCartridge(target, segment, disassembly);
		break;
		case 32768:
			target.atari.paging_model = StaticAnalyser::Atari2600PagingModel::Atari32k;
		break;
		default:
		break;
	}

	// check for a Super Chip. Atari ROM images [almost] always have the same value stored over RAM
	// regions.
	if(target.atari.paging_model != StaticAnalyser::Atari2600PagingModel::CBSRamPlus && target.atari.paging_model != StaticAnalyser::Atari2600PagingModel::MNetwork)
	{
		bool has_superchip = true;
		for(size_t address = 0; address < 256; address++)
		{
			if(segment.data[address] != segment.data[0])
			{
				has_superchip = false;
				break;
			}
		}
		target.atari.uses_superchip = has_superchip;
	}

	// check for a Tigervision or Tigervision-esque scheme
	if(target.atari.paging_model == StaticAnalyser::Atari2600PagingModel::None && segment.data.size() > 4096)
	{
		bool looks_like_tigervision = disassembly.external_stores.find(0x3f) != disassembly.external_stores.end();
		if(looks_like_tigervision) target.atari.paging_model = StaticAnalyser::Atari2600PagingModel::Tigervision;
	}
}

void StaticAnalyser::Atari::AddTargets(
	const std::list<std::shared_ptr<Storage::Disk::Disk>> &disks,
	const std::list<std::shared_ptr<Storage::Tape::Tape>> &tapes,
	const std::list<std::shared_ptr<Storage::Cartridge::Cartridge>> &cartridges,
	std::list<StaticAnalyser::Target> &destination)
{
	// TODO: any sort of sanity checking at all; at the minute just trust the file type
	// approximation already performed.
	Target target;
	target.machine = Target::Atari2600;
	target.probability = 1.0;
	target.disks = disks;
	target.tapes = tapes;
	target.cartridges = cartridges;
	target.atari.paging_model = Atari2600PagingModel::None;
	target.atari.uses_superchip = false;

	// try to figure out the paging scheme
	if(!cartridges.empty())
	{
		const std::list<Storage::Cartridge::Cartridge::Segment> &segments = cartridges.front()->get_segments();

		if(segments.size() == 1)
		{
			const Storage::Cartridge::Cartridge::Segment &segment = segments.front();
			DeterminePagingForCartridge(target, segment);
		}
	}

	destination.push_back(target);
}
