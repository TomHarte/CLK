//
//  CommodoreAnalyser.cpp
//  Clock Signal
//
//  Created by Thomas Harte on 06/09/2016.
//  Copyright © 2016 Thomas Harte. All rights reserved.
//

#include "CommodoreAnalyser.hpp"

#include "Tape.hpp"

using namespace StaticAnalyser::Commodore;

void StaticAnalyser::Commodore::AddTargets(
	const std::list<std::shared_ptr<Storage::Disk::Disk>> &disks,
	const std::list<std::shared_ptr<Storage::Tape::Tape>> &tapes,
	const std::list<std::shared_ptr<Storage::Cartridge::Cartridge>> &cartridges,
	std::list<StaticAnalyser::Target> &destination)
{
	Target target;
	target.machine = Target::Vic20;	// TODO: machine estimation
	target.probability = 1.0; // TODO: a proper estimation

	// strip out inappropriate cartridges
//	target.cartridges = AcornCartridgesFrom(cartridges);

	// if there are any tapes, attempt to get data from the first
	if(tapes.size() > 0)
	{
		std::shared_ptr<Storage::Tape::Tape> tape = tapes.front();
		tape->reset();
		std::list<File> files = GetFiles(tape);
		tape->reset();

		// continue if there are any files
		if(files.size())
		{
			bool is_basic = true;

			// decide whether this is a BASIC file based on the proposition that:
			//	(1) they're always relocatable; and
			//	(2) they have a per-line structure of:
			//		[4 bytes: address of start of next line]
			//		[4 bytes: this line number]
			//		... null-terminated code ...
			//	(with a next line address of 0000 indicating end of program)ß
			if(files.front().type != File::RelocatableProgram) is_basic = false;
			else
			{
				uint16_t line_address = 0;
				int line_number = -1;

				uint16_t starting_address = files.front().starting_address;
				line_address = starting_address;
				is_basic = false;
				while(1)
				{
					if(line_address - starting_address >= files.front().data.size() + 2) break;

					uint16_t next_line_address = files.front().data[line_address - starting_address];
					next_line_address |= files.front().data[line_address - starting_address + 1] << 8;

					if(!next_line_address)
					{
						is_basic = true;
						break;
					}
					if(next_line_address < line_address + 5) break;

					if(line_address - starting_address >= files.front().data.size() + 5) break;
					uint16_t next_line_number = files.front().data[line_address - starting_address + 2];
					next_line_number |= files.front().data[line_address - starting_address + 3] << 8;

					if(next_line_number <= line_number) break;

					line_number = (uint16_t)next_line_number;
					line_address = next_line_address;
				}
			}

			target.vic20.memory_model = Vic20MemoryModel::Unexpanded;
			if(is_basic)
			{
				target.loadingCommand = "LOAD\"\",1,0\nRUN\n";
				switch(files.front().starting_address)
				{
					case 0x1001:
					default: break;
					case 0x1201:
						target.vic20.memory_model = Vic20MemoryModel::ThirtyTwoKB;
					break;
					case 0x0401:
						target.vic20.memory_model = Vic20MemoryModel::EightKB;
					break;
				}
			}
			else
			{
				// TODO: this is machine code. So, ummm?
			}

			target.tapes = tapes;
		}
	}

	if(target.tapes.size() || target.cartridges.size() || target.disks.size())
		destination.push_back(target);
}
