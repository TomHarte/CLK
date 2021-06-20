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
#include <codecvt>
#include <iomanip>
#include <locale>
#include <sstream>

using namespace ROM;

namespace {
constexpr Name MaxName = Name::SpectrumPlus3;
}

Request::Request(Name name, bool optional) {
	node.name = name;
	node.is_optional = optional;
}

Request Request::append(Node::Type type, const Request &rhs) {
	// If either side is empty, act appropriately.
	if(node.empty() && !rhs.node.empty()) {
		return rhs;
	}
	if(rhs.node.empty()) {
		return *this;
	}

	// Just copy in the RHS child nodes if types match.
	if(node.type == type && rhs.node.type == type) {
		Request new_request = *this;
		new_request.node.children.insert(new_request.node.children.end(), rhs.node.children.begin(), rhs.node.children.end());
		new_request.node.sort();
		return new_request;
	}

	// Possibly: left is appropriate request and rhs is just one more thing?
	if(node.type == type && rhs.node.type == Node::Type::One) {
		Request new_request = *this;
		new_request.node.children.push_back(rhs.node);
		new_request.node.sort();
		return new_request;
	}

	// Or: right is appropriate request and this is just one more thing?
	if(rhs.node.type == type && node.type == Node::Type::One) {
		Request new_request = rhs;
		new_request.node.children.push_back(node);
		new_request.node.sort();
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

	for(const auto &child: children) {
		child.add_descriptions(result);
	}
}

Request Request::subtract(const ROM::Map &map) const {
	Request copy(*this);
	if(copy.node.subtract(map)) {
		copy.node.name = Name::None;
		copy.node.type = Node::Type::One;
	}
	return copy;
}

bool Request::Node::subtract(const ROM::Map &map) {
	switch(type) {
		case Type::One:
		return map.find(name) != map.end();

		default: {
			bool has_all = true;
			bool has_any = false;

			auto iterator = children.begin();
			while(iterator != children.end()) {
				const bool did_subtract = iterator->subtract(map);
				has_all &= did_subtract;
				has_any |= did_subtract;
				if(did_subtract) {
					iterator = children.erase(iterator);
				} else {
					++iterator;
				}
			}

			return (type == Type::All && has_all) || (type == Type::Any && has_any);
		}
	}
}

bool Request::empty() {
	return node.type == Node::Type::One && node.name == Name::None;
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

void Request::visit(
	const std::function<void(ListType, size_t)> &enter_list,
	const std::function<void(void)> &exit_list,
	const std::function<void(ROM::Request::ListType, const ROM::Description &, bool, size_t)> &add_item
) const {
	node.visit(enter_list, exit_list, add_item);
}

void Request::visit(
	const std::function<void(LineItem, ListType, int, const ROM::Description *, bool, size_t)> &add_item
) const {
	int indentation_level = 0;
	node.visit(
		[&indentation_level, &add_item] (ROM::Request::ListType type, size_t size) {
			add_item(LineItem::NewList, type, indentation_level, nullptr, false, size);
			++indentation_level;
		},
		[&indentation_level] {
			--indentation_level;
		},
		[&indentation_level, &add_item] (ROM::Request::ListType type, const ROM::Description &rom, bool is_optional, size_t remaining) {
			add_item(LineItem::Description, type, indentation_level, &rom, is_optional, remaining);
		}
	);
}

void Request::Node::visit(
	const std::function<void(ListType, size_t)> &enter_list,
	const std::function<void(void)> &exit_list,
	const std::function<void(ROM::Request::ListType type, const ROM::Description &, bool is_optional, size_t remaining)> &add_item
) const {
	switch(type) {
		case Type::One:
			enter_list(ListType::Single, 1);
			add_item(ROM::Request::ListType::Any, Description(name), is_optional, 0);
			exit_list();
		break;

		case Type::Any:
		case Type::All: {
			const ListType list_type = type == Type::Any ? ListType::Any : ListType::All;
			enter_list(list_type, children.size());
			for(size_t index = 0; index < children.size(); index++) {
				auto &child = children[index];

				if(child.type == Type::One) {
					add_item(list_type, Description(child.name), child.is_optional, children.size() - 1 - index);
				} else {
					child.visit(enter_list, exit_list, add_item);
				}
			}
			exit_list();
		} break;
	}
}

std::vector<Description> ROM::all_descriptions() {
	std::vector<Description> result;
	for(int name = 1; name <= MaxName; name++) {
		result.push_back(Description(ROM::Name(name)));
	}
	return result;
}

std::optional<Description> Description::from_crc(uint32_t crc32) {
	for(int name = 1; name <= MaxName; name++) {
		const Description candidate = Description(ROM::Name(name));

		const auto found_crc = std::find(candidate.crc32s.begin(), candidate.crc32s.end(), crc32);
		if(found_crc != candidate.crc32s.end()) {
			return candidate;
		}
	}

	return std::nullopt;
}

std::string Description::description(int flags) const {
	std::stringstream output;

	// If there are no CRCs, don't output them.
	if(crc32s.empty()) flags &= ~ DescriptionFlag::CRC;

	// Print the file name(s) and the descriptive name.
	if(flags & DescriptionFlag::Filename) {
		flags &= ~DescriptionFlag::Filename;

		output << machine_name <<  '/';
		if(file_names.size() == 1) {
			output << file_names[0];
		} else {
			output << "{";
			bool is_first = true;
			for(const auto &file_name: file_names) {
				if(!is_first) output << " or ";
				output << file_name;
				is_first = false;
			}
			output << "}";
		}
		output << " (" << descriptive_name;
		if(!flags) {
			output << ")";
			return output.str();
		}
		output << "; ";
	} else {
		output << descriptive_name;
		if(!flags) {
			return output.str();
		}
		output << " (";
	}

	// Print the size.
	if(flags & DescriptionFlag::Size) {
		flags &= ~DescriptionFlag::Size;
		output << size << " bytes";

		if(!flags) {
			output << ")";
			return output.str();
		}
		output << "; ";
	}

	// Print the CRC(s).
	if(flags & DescriptionFlag::CRC) {
		flags &= ~DescriptionFlag::CRC;

		output << ((crc32s.size() > 1) ? "usual crc32s: " : "usual crc32: ");
		bool is_first = true;
		for(const auto crc32: crc32s) {
			if(!is_first) output << ", ";
			is_first = false;
			output << std::hex << std::setfill('0') << std::setw(8) << crc32;
		}

		if(!flags) {
			output << ")";
			return output.str();
		}
	}

	return output.str();
}

std::wstring Request::description(int description_flags, wchar_t bullet_point) {
	std::wstringstream output;
	std::wstring_convert<std::codecvt_utf8<wchar_t>> wstring_converter;

	visit(
		[&output, description_flags, bullet_point, &wstring_converter] (ROM::Request::LineItem item, ROM::Request::ListType type, int indentation_level, const ROM::Description *description, bool is_optional, size_t remaining) {
			if(indentation_level) {
				output << std::endl;
				for(int c = 0; c < indentation_level; c++) output << '\t';
				output << bullet_point << ' ';
			}

			switch(item) {
				case ROM::Request::LineItem::NewList:
					if(remaining > 1) {
						if(!indentation_level) output << " ";
						switch(type) {
							default:
							case ROM::Request::ListType::All:	output << "all of:";		break;
							case ROM::Request::ListType::Any:
								if(remaining == 2) {
									output << "either of:";
								} else {
									output << "any of:";
								}
							break;
						}
					} else {
						output << ":";
					}
				break;

				case ROM::Request::LineItem::Description:
					if(is_optional) output << "optionally, ";

					output << wstring_converter.from_bytes(description->description(description_flags));

					if(remaining) {
						output << ";";
						if(remaining == 1) {
							output << ((type == ROM::Request::ListType::All) ? " and" : " or");
						}
					} else {
						output << ".";
					}
				break;
			}
		}
	);

	return output.str();
}


Description::Description(Name name) {
	switch(name) {
		default: assert(false);	break;

		case Name::AMSDOS:				*this = Description(name, "AmstradCPC", "the Amstrad Disk Operating System", "amsdos.rom", 16*1024, 0x1fe22ecdu);	break;
		case Name::CPC464Firmware:		*this = Description(name, "AmstradCPC", "the CPC 464 firmware", "os464.rom", 16*1024, 0x815752dfu);					break;
		case Name::CPC464BASIC:			*this = Description(name, "AmstradCPC", "the CPC 464 BASIC ROM", "basic464.rom", 16*1024, 0x7d9a3bacu);				break;
		case Name::CPC664Firmware:		*this = Description(name, "AmstradCPC", "the CPC 664 firmware", "os664.rom", 16*1024, 0x3f5a6dc4u);					break;
		case Name::CPC664BASIC:			*this = Description(name, "AmstradCPC", "the CPC 664 BASIC ROM", "basic664.rom", 16*1024, 0x32fee492u);				break;
		case Name::CPC6128Firmware:		*this = Description(name, "AmstradCPC", "the CPC 6128 firmware", "os6128.rom", 16*1024, 0x0219bb74u);				break;
		case Name::CPC6128BASIC:		*this = Description(name, "AmstradCPC", "the CPC 6128 BASIC ROM", "basic6128.rom", 16*1024, 0xca6af63du);			break;

		case Name::AppleIIEnhancedE:	*this = Description(name, "AppleII", "the Enhanced Apple IIe ROM", "apple2e.rom", 32*1024, 0x65989942u);				break;
		case Name::AppleIIe:			*this = Description(name, "AppleII", "the Apple IIe ROM", "apple2eu.rom", 32*1024, 0xe12be18du);						break;
		case Name::AppleIIPlus:			*this = Description(name, "AppleII", "the Apple II+ ROM", "apple2.rom", 12*1024, 0xf66f9c26u);							break;
		case Name::AppleIIOriginal:		*this = Description(name, "AppleII", "the original Apple II ROM", "apple2o.rom", 12*1024, 0xba210588u);					break;
		case Name::AppleIICharacter:	*this = Description(name, "AppleII", "the basic Apple II character ROM", "apple2-character.rom", 2*1024, 0x64f415c6u);	break;
		case Name::AppleIIeCharacter:	*this = Description(name, "AppleII", "the Apple IIe character ROM", "apple2eu-character.rom", 4*1024, 0x816a86f1u);		break;
		case Name::AppleIIEnhancedECharacter:
			*this = Description(name, "AppleII", "the Enhanced Apple IIe character ROM", "apple2e-character.rom", 4*1024, 0x2651014du);
		break;

		case Name::AppleIIgsROM00:	/* TODO */
		case Name::AppleIIgsROM01:		*this = Description(name, "AppleIIgs", "the Apple IIgs ROM01", "apple2gs.rom", 128*1024, 0x42f124b0u);			break;
		case Name::AppleIIgsROM03:		*this = Description(name, "AppleIIgs", "the Apple IIgs ROM03", "apple2gs.rom2", 256*1024, 0xde7ddf29u);			break;
		case Name::AppleIIgsCharacter:	*this = Description(name, "AppleIIgs", "the Apple IIgs character ROM", "apple2gs.chr", 4*1024, 0x91e53cd8u);	break;
		case AppleIIgsMicrocontrollerROM03:
			*this = Description(name, "AppleIIgs", "the Apple IIgs ROM03 ADB microcontroller ROM", "341s0632-2", 4*1024, 0xe1c11fb0u);
		break;

		case Name::DiskIIBoot16Sector:
			*this = Description(name, "DiskII", "the Disk II 16-sector boot ROM", "boot-16.rom", 256, 0xce7144f6u);
		break;
		case Name::DiskIIStateMachine16Sector:
			*this = Description(name, "DiskII", "the Disk II 16-sector state machine ROM", "state-machine-16.rom", 256, std::initializer_list<uint32_t>{ 0x9796a238, 0xb72a2c70 } );
		break;
		case Name::DiskIIBoot13Sector:
			*this = Description(name, "DiskII", "the Disk II 13-sector boot ROM", "boot-13.rom", 256, 0xd34eb2ffu);
		break;
		case Name::DiskIIStateMachine13Sector:
			*this = Description(name, "DiskII", "the Disk II 13-sector state machine ROM", "state-machine-13.rom", 256, 0x62e22620u);
		break;

		case Name::EnterpriseEXOS10: {
			const std::initializer_list<std::string> filenames = {"exos10.bin", "Exos (198x)(Enterprise).bin"};
			*this = Description(name, "Enterprise", "the Enterprise EXOS ROM v1.0", filenames, 32 * 1024, 0x30b26387u);
		} break;
		case Name::EnterpriseEXOS20: {
			const std::initializer_list<std::string> filenames = {"exos20.bin", "Expandible OS v2.0 (1984)(Intelligent Software).bin"};
			*this = Description(name, "Enterprise", "the Enterprise EXOS ROM v2.0", filenames, 32 * 1024, 0xd421795fu);
		} break;
		case Name::EnterpriseEXOS21: {
			const std::initializer_list<std::string> filenames = {"exos21.bin", "Expandible OS v2.1 (1985)(Intelligent Software).bin"};
			*this = Description(name, "Enterprise", "the Enterprise EXOS ROM v2.1", filenames, 32 * 1024, 0x982a3b44u);
		} break;
		case Name::EnterpriseEXOS23: {
			const std::initializer_list<std::string> filenames = {"exos23.bin", "Expandible OS v2.3 (1987)(Intelligent Software).bin"};
			*this = Description(name, "Enterprise", "the Enterprise EXOS ROM v2.1", filenames, 64 * 1024, 0x24838410u);
		} break;

		case Name::EnterpriseBASIC10: {
			const std::initializer_list<std::string> filenames = {"basic10.bin"};
			*this = Description(name, "Enterprise", "the Enterprise BASIC ROM v1.0", filenames, 16 * 1024, 0xd62e4fb7u);
		} break;
		case Name::EnterpriseBASIC10Part1: {
			const std::initializer_list<std::string> filenames = {"BASIC 1.0 - EPROM 1-2 (198x)(Enterprise).bin"};
			*this = Description(name, "Enterprise", "the Enterprise BASIC ROM v1.0, Part 1", filenames, 8193, 0x37bf48e1u);
		} break;
		case Name::EnterpriseBASIC10Part2: {
			const std::initializer_list<std::string> filenames = {"BASIC 1.0 - EPROM 2-2 (198x)(Enterprise).bin"};
			*this = Description(name, "Enterprise", "the Enterprise BASIC ROM v1.0, Part 2", filenames, 8193, 0xc5298c79u);
		} break;
		case Name::EnterpriseBASIC11: {
			const std::initializer_list<std::string> filenames = {"basic11.bin"};
			*this = Description(name, "Enterprise", "the Enterprise BASIC ROM v1.1", filenames, 16 * 1024, 0x683cf455u);
		} break;
		case Name::EnterpriseBASIC11Suffixed: {
			const std::initializer_list<std::string> filenames = {"BASIC 1.1 - EPROM 1.1 (198x)(Enterprise).bin"};
			*this = Description(name, "Enterprise", "the Enterprise BASIC ROM v1.1, with trailing byte", filenames, 16385, 0xc96b7602u);
		} break;
		case Name::EnterpriseBASIC21: {
			const std::initializer_list<std::string> filenames = {
				"basic21.bin",
				"BASIC Interpreter v2.1 (1985)(Intelligent Software).bin",
				"BASIC Interpreter v2.1 (1985)(Intelligent Software)[a].bin"
			};
			const std::initializer_list<uint32_t> crcs = { 0x55f96251, 0x683cf455 };
			*this = Description(name, "Enterprise", "the Enterprise BASIC ROM v2.1", filenames, 16 * 1024, crcs);
		} break;

		case Name::Macintosh128k:	*this = Description(name, "Macintosh", "the Macintosh 128k ROM", "mac128k.rom", 64*1024, 0x6d0c8a28u);	break;
		case Name::Macintosh512k:	*this = Description(name, "Macintosh", "the Macintosh 512k ROM", "mac512k.rom", 64*1024, 0xcf759e0d);	break;
		case Name::MacintoshPlus: {
			const std::initializer_list<uint32_t> crcs = { 0x4fa5b399, 0x7cacd18f, 0xb2102e8e };
			*this = Description(name, "Macintosh", "the Macintosh Plus ROM", "macplus.rom", 128*1024, crcs);
		} break;

		case Name::AtariSTTOS100:		*this = Description(name, "AtariST", "the UK TOS 1.00 ROM", "tos100.img", 192*1024, 0x1a586c64u);		break;
		case Name::AtariSTTOS104:		*this = Description(name, "AtariST", "the UK TOS 1.04 ROM", "tos104.img", 192*1024, 0xa50d1d43u);		break;
		case Name::AtariSTEmuTOS192:	*this = Description(name, "AtariST", "the UK EmuTOS 1.92 ROM", "etos192uk.img", 192*1024, 0xfc3b9e61u);	break;

		case Name::ColecoVisionBIOS:
			*this = Description(name, "ColecoVision", "the ColecoVision BIOS", "coleco.rom", 8*1024, 0x3aa93ef3u);
		break;

		case Name::ZX80:	*this = Description(name, "ZX8081", "the ZX80 BASIC ROM", "zx80.rom", 4 * 1024, 0x4c7fc597u);	break;
		case Name::ZX81:	*this = Description(name, "ZX8081", "the ZX81 BASIC ROM", "zx81.rom", 8 * 1024, 0x4b1dd6ebu);	break;

		case Name::Spectrum48k:		*this = Description(name, "ZXSpectrum", "the 48kb ROM", "48.rom", 16 * 1024, 0xddee531fu);		break;
		case Name::Spectrum128k:	*this = Description(name, "ZXSpectrum", "the 128kb ROM", "128.rom", 32 * 1024, 0x2cbe8995u);	break;
		case Name::SpecrumPlus2:	*this = Description(name, "ZXSpectrum", "the +2 ROM", "plus2.rom", 32 * 1024, 0xe7a517dcu); 	break;
		case Name::SpectrumPlus3: {
			const std::initializer_list<uint32_t> crcs = { 0x96e3c17a, 0xbe0d9ec4 };
			*this = Description(name, "ZXSpectrum", "the +2a/+3 ROM", "plus3.rom", 64 * 1024, crcs);
		} break;

		case Name::AcornBASICII:	*this = Description(name, "Electron", "the Acorn BASIC II ROM", "basic.rom", 16*1024, 0x79434781u); 			break;
		case Name::PRESADFSSlot1:	*this = Description(name, "Electron", "the E00 ADFS ROM, first slot", "ADFS-E00_1.rom", 16*1024, 0x51523993u); 	break;
		case Name::PRESADFSSlot2:	*this = Description(name, "Electron", "the E00 ADFS ROM, second slot", "ADFS-E00_2.rom", 16*1024, 0x8d17de0eu); break;
		case Name::AcornADFS:		*this = Description(name, "Electron", "the Acorn ADFS ROM", "adfs.rom", 16*1024, 0x3289bdc6u); 					break;
		case Name::Acorn1770DFS:	*this = Description(name, "Electron", "the 1770 DFS ROM", "DFS-1770-2.20.rom", 16*1024, 0xf3dc9bc5u);			break;
		case Name::PRESAdvancedPlus6:
			*this = Description(name, "Electron", "the 8kb Advanced Plus 6 ROM", "AP6v133.rom", 8*1024, 0xe0013cfcu);
		break;
		case Name::AcornElectronMOS100:
			*this = Description(name, "Electron", "the Electron MOS ROM v1.00", "os.rom", 16*1024, 0xbf63fb1fu);
		break;

		case Name::MasterSystemJapaneseBIOS:	*this = Description(name, "MasterSystem", "the Japanese Master System BIOS", "japanese-bios.sms", 8*1024, 0x48d44a13u); break;
		case Name::MasterSystemWesternBIOS:		*this = Description(name, "MasterSystem", "the European/US Master System BIOS", "bios.sms", 8*1024, 0x0072ed54u); 		break;

		case Name::Commodore1540:	*this = Description(name, "Commodore1540", "the 1540 ROM", "1540.bin", 16*1024, 0x718d42b1u);	break;
		case Name::Commodore1541:	*this = Description(name, "Commodore1540", "the 1541 ROM", "1541.bin", 16*1024, 0xfb760019);	break;

		case Name::Vic20BASIC:				*this = Description(name, "Vic20", "the VIC-20 BASIC ROM", "basic.bin", 8*1024, 0xdb4c43c1u);									break;
		case Name::Vic20EnglishCharacters:	*this = Description(name, "Vic20", "the English-language VIC-20 character ROM", "characters-english.bin", 4*1024, 0x83e032a6u);	break;
		case Name::Vic20EnglishPALKernel:	*this = Description(name, "Vic20", "the English-language PAL VIC-20 kernel ROM", "kernel-pal.bin", 8*1024, 0x4be07cb4u);		break;
		case Name::Vic20EnglishNTSCKernel:	*this = Description(name, "Vic20", "the English-language NTSC VIC-20 kernel ROM", "kernel-ntsc.bin", 8*1024, 0xe5e7c174u);		break;
		case Name::Vic20DanishCharacters:	*this = Description(name, "Vic20", "the Danish VIC-20 character ROM", "characters-danish.bin", 4*1024, 0x7fc11454u);			break;
		case Name::Vic20DanishKernel:		*this = Description(name, "Vic20", "the Danish VIC-20 kernel ROM", "kernel-danish.bin", 8*1024, 0x02adaf16u);					break;
		case Name::Vic20JapaneseCharacters:	*this = Description(name, "Vic20", "the Japanese VIC-20 character ROM", "characters-japanese.bin", 4*1024, 0xfcfd8a4bu);		break;
		case Name::Vic20JapaneseKernel:		*this = Description(name, "Vic20", "the Japanese VIC-20 kernel ROM", "kernel-japanese.bin", 8*1024, 0x336900d7u);				break;
		case Name::Vic20SwedishCharacters:	*this = Description(name, "Vic20", "the Swedish VIC-20 character ROM", "characters-swedish.bin", 4*1024, 0xd808551du);			break;
		case Name::Vic20SwedishKernel:		*this = Description(name, "Vic20", "the Swedish VIC-20 kernel ROM", "kernel-swedish.bin", 8*1024, 0xb2a60662u);					break;

		case Name::OricColourROM:		*this = Description(name, "Oric", "the Oric colour ROM", "colour.rom", 128, 0xd50fca65u);			break;
		case Name::OricBASIC10:			*this = Description(name, "Oric", "Oric BASIC 1.0", "basic10.rom", 16*1024, 0xf18710b4u);			break;
		case Name::OricBASIC11:			*this = Description(name, "Oric", "Oric BASIC 1.1", "basic11.rom", 16*1024, 0xc3a92befu);			break;
		case Name::OricPravetzBASIC:	*this = Description(name, "Oric", "Pravetz BASIC", "pravetz.rom", 16*1024, 0x58079502u);			break;
		case Name::OricByteDrive500:	*this = Description(name, "Oric", "the Oric Byte Drive 500 ROM", "bd500.rom", 8*1024, 0x61952e34u);	break;
		case Name::OricJasmin:			*this = Description(name, "Oric", "the Oric Jasmin ROM", "jasmin.rom", 2*1024, 0x37220e89u);		break;
		case Name::OricMicrodisc:		*this = Description(name, "Oric", "the Oric Microdisc ROM", "microdisc.rom", 8*1024, 0xa9664a9cu);	break;
		case Name::Oric8DOSBoot:		*this = Description(name, "Oric", "the 8DOS boot ROM", "8dos.rom", 512, 0x49a74c06u);				break;

		case Name::MSXGenericBIOS:	*this = Description(name, "MSX", "any MSX BIOS", "msx.rom", 32*1024, 0x94ee12f3u);					break;
		case Name::MSXJapaneseBIOS:	*this = Description(name, "MSX", "a Japanese MSX BIOS", "msx-japanese.rom", 32*1024, 0xee229390u);	break;
		case Name::MSXAmericanBIOS:	*this = Description(name, "MSX", "an American MSX BIOS", "msx-american.rom", 32*1024, 0u);			break;
		case Name::MSXEuropeanBIOS:	*this = Description(name, "MSX", "a European MSX BIOS", "msx-european.rom", 32*1024, 0u);			break;
		case Name::MSXDOS:			*this = Description(name, "MSX", "the MSX-DOS ROM", "disk.rom", 16*1024, 0x721f61dfu);				break;

		case Name::SinclairQLJS:
			*this = Description(name, "SinclairQL", "the Sinclair QL 'JS' ROM", "js.rom", 48*1024, 0x0f95aab5u);
		break;

	}
}
