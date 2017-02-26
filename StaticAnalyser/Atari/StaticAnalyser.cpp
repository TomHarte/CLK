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
			uint16_t entry_address, break_address;
			const Storage::Cartridge::Cartridge::Segment &segment = segments.front();
			if(segment.data.size() < 4096)
			{
				entry_address = (uint16_t)(segment.data[0x7fc] | (segment.data[0x7fd] << 8));
				break_address = (uint16_t)(segment.data[0x7fe] | (segment.data[0x7ff] << 8));
			}
			else
			{
				entry_address = (uint16_t)(segment.data[0xffc] | (segment.data[0xffd] << 8));
				break_address = (uint16_t)(segment.data[0xffe] | (segment.data[0xfff] << 8));
			}
			StaticAnalyser::MOS6502::Disassembly disassembly =
				StaticAnalyser::MOS6502::Disassemble(segment.data, 0x1000, {entry_address, break_address}, 0x1fff);

			// check for any sort of on-cartridge RAM; that might imply a Super Chip or else immediately tip the
			// hat that this is a CBS RAM+ cartridge
			if(!disassembly.internal_stores.empty())
			{
				bool writes_above_128 = false;
				for(uint16_t address : disassembly.internal_stores)
				{
					writes_above_128 |= ((address & 0x1fff) > 0x10ff) && ((address & 0x1fff) < 0x1200);
				}
				if(writes_above_128)
					target.atari.paging_model = Atari2600PagingModel::CBSRamPlus;
				else
					target.atari.uses_superchip = true;
			}
		}
	}

	destination.push_back(target);
}
