//
//  ROMCatalogue.cpp
//  Clock Signal
//
//  Created by Thomas Harte on 01/06/2021.
//  Copyright © 2021 Thomas Harte. All rights reserved.
//

#include "ROMCatalogue.hpp"

#include <algorithm>
#include <cassert>

using namespace ROM;

Request::Request(Name name, bool optional) {
	node.name = name;
	node.is_optional = optional;
}

Request Request::append(Node::Type type, const Request &rhs) {
	// Start with the easiest case: this is already an ::All request, and
	// so is the new thing.
	if(node.type == type && rhs.node.type == type) {
		Request new_request = *this;
		new_request.node.children.insert(new_request.node.children.end(), rhs.node.children.begin(), rhs.node.children.end());
		return new_request;
	}

	// Otherwise create a new parent node.
	Request parent;
	parent.node.type = type;
	parent.node.children.push_back(this->node);
	parent.node.children.push_back(rhs.node);
	return parent;
}

Request Request::operator &&(const Request &rhs) {
	return append(Node::Type::All, rhs);
}

Request Request::operator ||(const Request &rhs) {
	return append(Node::Type::Any, rhs);
}

bool Request::validate(Map &map) const {
	return node.validate(map);
}

std::vector<ROM::Description> Request::all_descriptions() const {
	std::vector<Description> result;
	node.add_descriptions(result);
	return result;
}

void Request::Node::add_descriptions(std::vector<Description> &result) const {
	if(type == Type::One) {
		result.push_back(name);
		return;
	}

	for(const auto &node: children) {
		node.add_descriptions(result);
	}
}

bool Request::Node::validate(Map &map) const {
	// Leaf nodes are easy: check that the named ROM is present,
	// unless it's optional, in which case it is always valid.
	//
	// If it is present, make sure it's the proper size.
	if(type == Type::One) {
		auto rom = map.find(name);
		if(rom == map.end()) {
			return is_optional;
		}

		const Description description(name);
		rom->second.resize(description.size);

		return true;
	}

	// This is a collection node then. Check for both any or all
	// simultaneously, since all nodes will need to be visited
	// regardless of any/all in order to ensure proper sizing.
	bool has_all = true;
	bool has_any = false;

	for(const auto &child: children) {
		const bool is_valid = child.validate(map);
		has_all &= is_valid;
		has_any |= is_valid;
	}

	return (type == Type::Any && has_any) || (type == Type::All && has_all);
}

std::optional<Description> Description::from_crc(uint32_t crc32) {
	for(int name = 1; name <= SpectrumPlus3; name++) {
		const Description candidate = Description(ROM::Name(name));

		const auto found_crc = std::find(candidate.crc32s.begin(), candidate.crc32s.end(), crc32);
		if(found_crc != candidate.crc32s.end()) {
			return candidate;
		}
	}

	return std::nullopt;
}

Description::Description(Name name) {
	switch(name) {
		default: assert(false);	break;

		case Name::AMSDOS:
			*this = Description(name, "AmstradCPC", "the Amstrad Disk Operating System", "amsdos.rom", 16*1024, 0x1fe22ecdu);
		break;
		case Name::CPC464Firmware:
			*this = Description(name, "AmstradCPC", "the CPC 464 firmware", "os464.rom", 16*1024, 0x815752dfu);
		break;
		case Name::CPC464BASIC:
			*this = Description(name, "AmstradCPC", "the CPC 464 BASIC ROM", "basic464.rom", 16*1024, 0x7d9a3bacu);
		break;
		case Name::CPC664Firmware:
			*this = Description(name, "AmstradCPC", "the CPC 664 firmware", "os664.rom", 16*1024, 0x3f5a6dc4u);
		break;
		case Name::CPC664BASIC:
			*this = Description(name, "AmstradCPC", "the CPC 664 BASIC ROM", "basic664.rom", 16*1024, 0x32fee492u);
		break;
		case Name::CPC6128Firmware:
			*this = Description(name, "AmstradCPC", "the CPC 6128 firmware", "os664.rom", 16*1024, 0x0219bb74u);
		break;
		case Name::CPC6128BASIC:
			*this = Description(name, "AmstradCPC", "the CPC 6128 BASIC ROM", "basic664.rom", 16*1024, 0xca6af63du);
		break;

//"AppleII"
//	AppleIIOriginal,
//	AppleIIPlus,
//	AppleIICharacter,
//	AppleIIe,
//	AppleIIeCharacter,
//	AppleIIEnhancedE,
//	AppleIIEnhancedECharacter,
	}

//					rom_descriptions.push_back(video_.rom_description(Video::VideoBase::CharacterROM::EnhancedIIe));
//					rom_descriptions.emplace_back(machine_name, "the Enhanced Apple IIe ROM", "apple2e.rom", 32*1024, 0x65989942u);
//					rom_descriptions.push_back(video_.rom_description(Video::VideoBase::CharacterROM::IIe));
//					rom_descriptions.emplace_back(machine_name, "the Apple IIe ROM", "apple2eu.rom", 32*1024, 0xe12be18du);
//					rom_descriptions.push_back(video_.rom_description(Video::VideoBase::CharacterROM::II));
//					rom_descriptions.emplace_back(machine_name, "the Apple II+ ROM", "apple2.rom", 12*1024, 0xf66f9c26u);
//					rom_descriptions.push_back(video_.rom_description(Video::VideoBase::CharacterROM::II));
//					rom_descriptions.emplace_back(machine_name, "the original Apple II ROM", "apple2o.rom", 12*1024, 0xba210588u);

//		roms = rom_fetcher({
//			{"DiskII", "the Disk II 16-sector boot ROM", "boot-16.rom", 256, 0xce7144f6},
//			{"DiskII", "the Disk II 16-sector state machine ROM", "state-machine-16.rom", 256, { 0x9796a238, 0xb72a2c70 } }
//		});
//		roms = rom_fetcher({
//			{"DiskII", "the Disk II 13-sector boot ROM", "boot-13.rom", 256, 0xd34eb2ff},
//			{"DiskII", "the Disk II 16-sector state machine ROM", "state-machine-16.rom", 256, { 0x9796a238, 0xb72a2c70 } }
////			{"DiskII", "the Disk II 13-sector state machine ROM", "state-machine-13.rom", 256, 0x62e22620 }
//		});


//		enum class CharacterROM {
//			/// The ROM that shipped with both the Apple II and the II+.
//			II,
//			/// The ROM that shipped with the original IIe.
//			IIe,
//			/// The ROM that shipped with the Enhanced IIe.
//			EnhancedIIe,
//			/// The ROM that shipped with the IIgs.
//			IIgs
//		};
//
//		/// @returns A file-level description of @c rom.
//		static ROM::Name rom_name(CharacterROM rom) {
//			const std::string machine_name = "AppleII";
//			switch(rom) {
//				case CharacterROM::II:
//					return ROMMachine::ROM(machine_name, "the basic Apple II character ROM", "apple2-character.rom", 2*1024, 0x64f415c6);
//
//				case CharacterROM::IIe:
//					return ROMMachine::ROM(machine_name, "the Apple IIe character ROM", "apple2eu-character.rom", 4*1024, 0x816a86f1);
//
//				default:	// To appease GCC.
//				case CharacterROM::EnhancedIIe:
//					return ROMMachine::ROM(machine_name, "the Enhanced Apple IIe character ROM", "apple2e-character.rom", 4*1024, 0x2651014d);
//
//				case CharacterROM::IIgs:
//					return ROMMachine::ROM(machine_name, "the Apple IIgs character ROM", "apple2gs.chr", 4*1024, 0x91e53cd8);
//			}
//		}
}


//			const std::string machine_name = "AppleIIgs";
//			switch(target.model) {
//				case Target::Model::ROM00:
//					/* TODO */
//				case Target::Model::ROM01:
//					rom_descriptions.emplace_back(machine_name, "the Apple IIgs ROM01", "apple2gs.rom", 128*1024, 0x42f124b0u);
//				break;
//
//				case Target::Model::ROM03:
//					rom_descriptions.emplace_back(machine_name, "the Apple IIgs ROM03", "apple2gs.rom2", 256*1024, 0xde7ddf29u);
//				break;
//			}
//			rom_descriptions.push_back(video_->rom_description(Video::Video::CharacterROM::EnhancedIIe));
//
//			// TODO: pick a different ADB ROM for earlier machine revisions?
//			rom_descriptions.emplace_back(machine_name, "the Apple IIgs ADB microcontroller ROM", "341s0632-2", 4*1024, 0xe1c11fb0u);


//			switch(model) {
//				default:
//				case Model::Mac128k:
//					ram_size = 128*1024;
//					rom_size = 64*1024;
//					rom_descriptions.emplace_back(machine_name, "the Macintosh 128k ROM", "mac128k.rom", 64*1024, 0x6d0c8a28);
//				break;
//				case Model::Mac512k:
//					ram_size = 512*1024;
//					rom_size = 64*1024;
//					rom_descriptions.emplace_back(machine_name, "the Macintosh 512k ROM", "mac512k.rom", 64*1024, 0xcf759e0d);
//				break;
//				case Model::Mac512ke:
//				case Model::MacPlus: {
//					ram_size = ((model == Model::MacPlus) ? 4096 : 512)*1024;
//					rom_size = 128*1024;
//					const std::initializer_list<uint32_t> crc32s = { 0x4fa5b399, 0x7cacd18f, 0xb2102e8e };
//					rom_descriptions.emplace_back(machine_name, "the Macintosh Plus ROM", "macplus.rom", 128*1024, crc32s);
//				} break;
//			}



//			std::vector<ROMMachine::ROM> rom_descriptions = {
//				{"AtariST", "the UK TOS 1.00 ROM", "tos100.img", 192*1024, 0x1a586c64}
////				{"AtariST", "the UK TOS 1.04 ROM", "tos104.img", 192*1024, 0xa50d1d43}
//			};


// 			rom_list.emplace_back(new ROMMachine::ROM("ColecoVision", "the ColecoVision BIOS", std::vector<std::string>{ "coleco.rom" }, 8*1024, 0x3aa93ef3u));


//			if(use_zx81_rom) {
//				rom_list.emplace_back(new ROMMachine::ROM("ZX8081", "the ZX81 BASIC ROM", std::vector<std::string>{ "zx81.rom" }, 8 * 1024, 0x4b1dd6ebu));
//			} else {
//				rom_list.emplace_back(new ROMMachine::ROM("ZX8081", "the ZX80 BASIC ROM", std::vector<std::string>{ "zx80.rom" }, 4 * 1024, 0x4c7fc597u));
//			}



//			const std::string machine = "ZXSpectrum";
//			switch(model) {
//				case Model::SixteenK:
//				case Model::FortyEightK:
//					rom_names.emplace_back(machine, "the 48kb ROM", "48.rom", 16 * 1024, 0xddee531fu);
//				break;
//
//				case Model::OneTwoEightK:
//					rom_names.emplace_back(machine, "the 128kb ROM", "128.rom", 32 * 1024, 0x2cbe8995u);
//				break;
//
//				case Model::Plus2:
//					rom_names.emplace_back(machine, "the +2 ROM", "plus2.rom", 32 * 1024, 0xe7a517dcu);
//				break;
//
//				case Model::Plus2a:
//				case Model::Plus3: {
//					const std::initializer_list<uint32_t> crc32s = { 0x96e3c17a, 0xbe0d9ec4 };
//					rom_names.emplace_back(machine, "the +2a/+3 ROM", "plus3.rom", 64 * 1024, crc32s);
//				} break;
//			}



//			const std::string machine_name = "Electron";
//			std::vector<ROMMachine::ROM> required_roms = {
//				{machine_name, "the Acorn BASIC II ROM", "basic.rom", 16*1024, 0x79434781},
//				{machine_name, "the Electron MOS ROM", "os.rom", 16*1024, 0xbf63fb1f}
//			};
//			const size_t pres_adfs_rom_position = required_roms.size();
//			if(target.has_pres_adfs) {
//				required_roms.emplace_back(machine_name, "the E00 ADFS ROM, first slot", "ADFS-E00_1.rom", 16*1024, 0x51523993);
//				required_roms.emplace_back(machine_name, "the E00 ADFS ROM, second slot", "ADFS-E00_2.rom", 16*1024, 0x8d17de0e);
//			}
//			const size_t acorn_adfs_rom_position = required_roms.size();
//			if(target.has_acorn_adfs) {
//				required_roms.emplace_back(machine_name, "the Acorn ADFS ROM", "adfs.rom", 16*1024, 0x3289bdc6);
//			}
//			const size_t dfs_rom_position = required_roms.size();
//			if(target.has_dfs) {
//				required_roms.emplace_back(machine_name, "the 1770 DFS ROM", "DFS-1770-2.20.rom", 16*1024, 0xf3dc9bc5);
//			}
//			const size_t ap6_rom_position = required_roms.size();
//			if(target.has_ap6_rom) {
//				required_roms.emplace_back(machine_name, "the 8kb Advanced Plus 6 ROM", "AP6v133.rom", 8*1024, 0xe0013cfc);
//			}


//				new ROMMachine::ROM("MasterSystem",
//					is_japanese ? "the Japanese Master System BIOS" : "the European/US Master System BIOS",
//					is_japanese ? "japanese-bios.sms" : "bios.sms",
//					8*1024,
//					is_japanese ? 0x48d44a13u : 0x0072ed54u,
//					true
//				)


//	switch(personality) {
//		case Personality::C1540:
//			device_name = "1540";
//			crc = 0x718d42b1;
//		break;
//		case Personality::C1541:
//			device_name = "1541";
//			crc = 0xfb760019;
//		break;
//	}
//
//	auto roms = rom_fetcher({ {"Commodore1540", "the " + device_name + " ROM", device_name + ".bin", 16*1024, crc} });


//			const std::string machine_name = "Vic20";
//			std::vector<ROMMachine::ROM> rom_names = {
//				{machine_name, "the VIC-20 BASIC ROM", "basic.bin", 8*1024, 0xdb4c43c1u}
//			};
//			switch(target.region) {
//				default:
//					rom_names.emplace_back(machine_name, "the English-language VIC-20 character ROM", "characters-english.bin", 4*1024, 0x83e032a6u);
//					rom_names.emplace_back(machine_name, "the English-language PAL VIC-20 kernel ROM", "kernel-pal.bin", 8*1024, 0x4be07cb4u);
//				break;
//				case Analyser::Static::Commodore::Target::Region::American:
//					rom_names.emplace_back(machine_name, "the English-language VIC-20 character ROM", "characters-english.bin", 4*1024, 0x83e032a6u);
//					rom_names.emplace_back(machine_name, "the English-language NTSC VIC-20 kernel ROM", "kernel-ntsc.bin", 8*1024, 0xe5e7c174u);
//				break;
//				case Analyser::Static::Commodore::Target::Region::Danish:
//					rom_names.emplace_back(machine_name, "the Danish VIC-20 character ROM", "characters-danish.bin", 4*1024, 0x7fc11454u);
//					rom_names.emplace_back(machine_name, "the Danish VIC-20 kernel ROM", "kernel-danish.bin", 8*1024, 0x02adaf16u);
//				break;
//				case Analyser::Static::Commodore::Target::Region::Japanese:
//					rom_names.emplace_back(machine_name, "the Japanese VIC-20 character ROM", "characters-japanese.bin", 4*1024, 0xfcfd8a4bu);
//					rom_names.emplace_back(machine_name, "the Japanese VIC-20 kernel ROM", "kernel-japanese.bin", 8*1024, 0x336900d7u);
//				break;
//				case Analyser::Static::Commodore::Target::Region::Swedish:
//					rom_names.emplace_back(machine_name, "the Swedish VIC-20 character ROM", "characters-swedish.bin", 4*1024, 0xd808551du);
//					rom_names.emplace_back(machine_name, "the Swedish VIC-20 kernel ROM", "kernel-swedish.bin", 8*1024, 0xb2a60662u);
//				break;
//			}


//			const std::string machine_name = "Oric";
//			std::vector<ROMMachine::ROM> rom_names = { {machine_name, "the Oric colour ROM", "colour.rom", 128, 0xd50fca65u, true} };
//			switch(target.rom) {
//				case Analyser::Static::Oric::Target::ROM::BASIC10:
//					rom_names.emplace_back(machine_name, "Oric BASIC 1.0", "basic10.rom", 16*1024, 0xf18710b4u);
//				break;
//				case Analyser::Static::Oric::Target::ROM::BASIC11:
//					rom_names.emplace_back(machine_name, "Oric BASIC 1.1", "basic11.rom", 16*1024, 0xc3a92befu);
//				break;
//				case Analyser::Static::Oric::Target::ROM::Pravetz:
//					rom_names.emplace_back(machine_name, "Pravetz BASIC", "pravetz.rom", 16*1024, 0x58079502u);
//				break;
//			}
//			size_t diskii_state_machine_index = 0;
//			switch(disk_interface) {
//				default: break;
//				case DiskInterface::BD500:
//					rom_names.emplace_back(machine_name, "the Oric Byte Drive 500 ROM", "bd500.rom", 8*1024, 0x61952e34u);
//				break;
//				case DiskInterface::Jasmin:
//					rom_names.emplace_back(machine_name, "the Oric Jasmin ROM", "jasmin.rom", 2*1024, 0x37220e89u);
//				break;
//				case DiskInterface::Microdisc:
//					rom_names.emplace_back(machine_name, "the Oric Microdisc ROM", "microdisc.rom", 8*1024, 0xa9664a9cu);
//				break;
//				case DiskInterface::Pravetz:
//					rom_names.emplace_back(machine_name, "the 8DOS boot ROM", "8dos.rom", 512, 0x49a74c06u);
//					// These ROM details are coupled with those in the DiskIICard.
//					diskii_state_machine_index = rom_names.size();
//					rom_names.emplace_back("DiskII", "the Disk II 16-sector state machine ROM", "state-machine-16.rom", 256, std::initializer_list<uint32_t>{ 0x9796a238u, 0xb72a2c70u });
//				break;
//			}
//
//			const auto collection = ROMMachine::ROMCollection(rom_names);


//			const std::string machine_name = "MSX";
//			std::vector<ROMMachine::ROM> required_roms = {
//				{machine_name, "any MSX BIOS", "msx.rom", 32*1024, 0x94ee12f3u}
//			};
//
//			bool is_ntsc = true;
//			uint8_t character_generator = 1;	/* 0 = Japan, 1 = USA, etc, 2 = USSR */
//			uint8_t date_format = 1;			/* 0 = Y/M/D, 1 = M/D/Y, 2 = D/M/Y */
//			uint8_t keyboard = 1;				/* 0 = Japan, 1 = USA, 2 = France, 3 = UK, 4 = Germany, 5 = USSR, 6 = Spain */
//
//			// TODO: CRCs below are incomplete, at best.
//			switch(target.region) {
//				case Target::Region::Japan:
//					required_roms.emplace_back(machine_name, "a Japanese MSX BIOS", "msx-japanese.rom", 32*1024, 0xee229390u);
//					vdp_->set_tv_standard(TI::TMS::TVStandard::NTSC);
//
//					is_ntsc = true;
//					character_generator = 0;
//					date_format = 0;
//				break;
//				case Target::Region::USA:
//					required_roms.emplace_back(machine_name, "an American MSX BIOS", "msx-american.rom", 32*1024, 0u);
//					vdp_->set_tv_standard(TI::TMS::TVStandard::NTSC);
//
//					is_ntsc = true;
//					character_generator = 1;
//					date_format = 1;
//				break;
//				case Target::Region::Europe:
//					required_roms.emplace_back(machine_name, "a European MSX BIOS", "msx-european.rom", 32*1024, 0u);
//					vdp_->set_tv_standard(TI::TMS::TVStandard::PAL);
//
//					is_ntsc = false;
//					character_generator = 1;
//					date_format = 2;
//				break;
//			}
//
//			ROMMachine::ROMVector rom_list;
//			ROMMachine::ROMCollection *bios = new ROMMachine::ROMCollection(required_roms, ROMMachine::ROMCollection::Type::Any);
//			rom_list.emplace_back(bios);
//
//			// Fetch the necessary ROMs; try the region-specific ROM first,
//			// but failing that fall back on patching the main one.
//			size_t disk_index = 0;
//			if(target.has_disk_drive) {
//				disk_index = required_roms.size();
//				rom_list.emplace_back(new ROMMachine::ROM(machine_name, "the MSX-DOS ROM", "disk.rom", 16*1024, 0x721f61dfu));
//			}
