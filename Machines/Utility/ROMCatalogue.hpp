//
//  ROMCatalogue.hpp
//  Clock Signal
//
//  Created by Thomas Harte on 01/06/2021.
//  Copyright © 2021 Thomas Harte. All rights reserved.
//

#ifndef ROMCatalogue_hpp
#define ROMCatalogue_hpp

#include <functional>
#include <map>
#include <optional>
#include <set>
#include <string>
#include <vector>

namespace ROM {

enum Name {
	None,

	// Acorn.
	AcornBASICII,
	AcornElectronMOS100,
	PRESADFSSlot1,
	PRESADFSSlot2,
	AcornADFS,
	PRESAdvancedPlus6,
	Acorn1770DFS,

	// Amstrad CPC.
	AMSDOS,
	CPC464Firmware,		CPC464BASIC,
	CPC664Firmware,		CPC664BASIC,
	CPC6128Firmware,	CPC6128BASIC,

	// Apple II.
	AppleIIOriginal,
	AppleIIPlus,
	AppleIICharacter,
	AppleIIe,
	AppleIIeCharacter,
	AppleIIEnhancedE,
	AppleIIEnhancedECharacter,

	// Apple IIgs.
	AppleIIgsROM00,
	AppleIIgsROM01,
	AppleIIgsROM03,
	AppleIIgsMicrocontrollerROM03,
	AppleIIgsCharacter,

	// Atari ST.
	AtariSTTOS100,
	AtariSTTOS104,

	// ColecoVision.
	ColecoVisionBIOS,

	// Commodore 1540/1541.
	Commodore1540,
	Commodore1541,

	// Disk II.
	DiskIIStateMachine16Sector,
	DiskIIBoot16Sector,
	DiskIIStateMachine13Sector,
	DiskIIBoot13Sector,

	// Macintosh.
	Macintosh128k,
	Macintosh512k,
	MacintoshPlus,

	// Master System.
	MasterSystemJapaneseBIOS,
	MasterSystemWesternBIOS,

	// MSX.
	MSXGenericBIOS,
	MSXJapaneseBIOS,
	MSXAmericanBIOS,
	MSXEuropeanBIOS,
	MSXDOS,

	// Oric.
	OricColourROM,
	OricBASIC10,
	OricBASIC11,
	OricPravetzBASIC,
	OricByteDrive500,
	OricJasmin,
	OricMicrodisc,
	Oric8DOSBoot,

	// Vic-20.
	Vic20BASIC,
	Vic20EnglishCharacters,
	Vic20EnglishPALKernel,
	Vic20EnglishNTSCKernel,
	Vic20DanishCharacters,
	Vic20DanishKernel,
	Vic20JapaneseCharacters,
	Vic20JapaneseKernel,
	Vic20SwedishCharacters,
	Vic20SwedishKernel,

	// ZX80/81.
	ZX80,
	ZX81,

	// ZX Spectrum.
	Spectrum48k,
	Spectrum128k,
	SpecrumPlus2,
	SpectrumPlus3,
};

using Map = std::map<ROM::Name, std::vector<uint8_t>>;

struct Description {
	/// The ROM's enum name.
	Name name = Name::None;
	/// The machine with which this ROM is associated, in a form that is safe for using as
	/// part of a file name.
	std::string machine_name;
	/// A descriptive name for this ROM, suitable for use in a bullet-point list, a bracket
	/// clause, etc, e.g. "the Electron MOS 1.0".
	std::string descriptive_name;
	/// All idiomatic file name for this ROM, e.g. "os10.rom".
	std::vector<std::string> file_names;
	/// The expected size of this ROM in bytes, e.g. 32768.
	size_t size = 0;
	/// CRC32s for all known acceptable copies of this ROM; intended to allow a host platform
	/// to test user-provided ROMs of unknown provenance. **Not** intended to be used
	/// to exclude ROMs where the user's intent is otherwise clear.
	std::set<uint32_t> crc32s;

	/// Constructs the @c Description that correlates to @c name.
	Description(Name name);

	/// Constructs the @c Description that correlates to @c crc32, if any.
	static std::optional<Description> from_crc(uint32_t crc32);

	enum DescriptionFlag {
		Size = 1 << 0,
		CRC = 1 << 1,
		Filename = 1 << 2,
	};

	/// Provides a single-line of text describing this ROM, including the usual base text
	/// plus all the fields provided as @c flags .
	std::string description(int flags) const;

	private:
		template <typename FileNameT, typename CRC32T> Description(
			Name name, std::string machine_name, std::string descriptive_name, FileNameT file_names, size_t size, CRC32T crc32s = uint32_t(0)
		) : name{name}, machine_name{machine_name}, descriptive_name{descriptive_name}, file_names{file_names}, size{size}, crc32s{crc32s} {
			// Slightly lazy: deal with the case where the constructor wasn't provided with any
			// CRCs by spotting that the set has exactly one member, which has value 0. The alternative
			// would be to provide a partial specialisation that never put anything into the set.
			if(this->crc32s.size() == 1 && !*this->crc32s.begin()) {
				this->crc32s.clear();
			}
		}
};

/// @returns a vector of all possible instances of ROM::Description — i.e. descriptions of every ROM
/// currently known to the ROM catalogue.
std::vector<Description> all_descriptions();

struct Request {
	Request(Name name, bool optional = false);
	Request() {}

	/// Forms the request that would be satisfied by @c this plus the right-hand side.
	Request operator &&(const Request &);

	/// Forms the request that would be satisfied by either @c this or the right-hand side.
	Request operator ||(const Request &);

	/// Inspects the ROMMap to ensure that it satisfies this @c Request.
	/// @c returns @c true if the request is satisfied; @c false otherwise.
	///
	/// All ROMs in the map will be resized to their idiomatic sizes.
	bool validate(Map &) const;

	/// Returns a flattened array of all @c ROM::Descriptions that relate to anything
	/// anywhere in this ROM request.
	std::vector<Description> all_descriptions() const;

	/// @returns @c true if this request is empty, i.e. would be satisfied with no ROMs; @c false otherwise.
	bool empty();

	/// @returns what remains of this ROM request given that everything in @c map has been found.
	Request subtract(const ROM::Map &map) const;

	enum class ListType {
		Any, All, Single
	};
	void visit(
		const std::function<void(ListType, size_t size)> &enter_list,
		const std::function<void(void)> &exit_list,
		const std::function<void(ROM::Request::ListType type, const ROM::Description &, bool is_optional, size_t remaining)> &add_item
	) const;

	enum class LineItem {
		NewList, Description
	};
	void visit(
		const std::function<void(LineItem, ListType, int level, const ROM::Description *, bool is_optional, size_t remaining)> &add_item
	) const;

	/// @returns a full bullet-pointed list of the requirements of this request, including
	/// appropriate conjuntives. This text is intended to be glued to the end of an opening
	/// portion of a sentence, e.g. "Please supply" + request.description(0, L'*').
	std::wstring description(int description_flags, wchar_t bullet_point);

	private:
		struct Node {
			enum class Type {
				Any, All, One
			};
			Type type = Type::One;
			Name name = Name::None;
			/// @c true if this ROM is optional for machine startup. Generally indicates something
			/// that would make emulation more accurate, but not sufficiently so to make it
			/// a necessity.
			bool is_optional = false;
			std::vector<Node> children;

			void add_descriptions(std::vector<Description> &) const;
			bool validate(Map &) const;
			void visit(
				const std::function<void(ListType, size_t)> &enter_list,
				const std::function<void(void)> &exit_list,
				const std::function<void(ROM::Request::ListType type, const ROM::Description &, bool is_optional, size_t remaining)> &add_item
			) const;
			bool subtract(const ROM::Map &map);
		};
		Node node;

		Request append(Node::Type type, const Request &rhs);
};

}

#endif /* ROMCatalogue_hpp */
