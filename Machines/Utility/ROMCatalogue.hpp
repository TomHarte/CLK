//
//  ROMCatalogue.hpp
//  Clock Signal
//
//  Created by Thomas Harte on 01/06/2021.
//  Copyright © 2021 Thomas Harte. All rights reserved.
//

#ifndef ROMCatalogue_hpp
#define ROMCatalogue_hpp

#include <map>
#include <optional>
#include <string>
#include <vector>

namespace ROM {

enum Name {
	Invalid,

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
	Name name = Name::Invalid;
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
	std::vector<uint32_t> crc32s;

	/// Constructs the @c Description that correlates to @c name.
	Description(Name name);

	/// Constructs the @c Description that correlates to @c crc32.
	static std::optional<Description> from_crc(uint32_t crc32);

	private:
		template <typename FileNameT, typename CRC32T> Description(
			Name name, std::string machine_name, std::string descriptive_name, FileNameT file_names, size_t size, CRC32T crc32s
		) : name{name}, machine_name{machine_name}, descriptive_name{descriptive_name}, file_names{file_names}, size{size}, crc32s{crc32s} {}
};

struct Request {
	Request(Name name, bool optional = false);
	Request() {}

	Request operator &&(const Request &);
	Request operator ||(const Request &);

	/// Inspects the ROMMap to ensure that it satisfies this @c Request.
	/// @c returns @c true if the request is satisfied; @c false otherwise.
	///
	/// All ROMs in the map will be resized to their idiomatic sizes.
	bool validate(Map &) const;

	std::vector<Description> all_descriptions() const;

	enum class ListType {
		Any, All, Single
	};
	void visit(
		const std::function<void(ListType)> &enter_list,
		const std::function<void(void)> &exit_list,
		const std::function<void(ROM::Request::ListType type, const ROM::Description &, bool is_optional, size_t remaining)> &add_item
	) const;

	private:
		struct Node {
			enum class Type {
				Any, All, One
			};
			Type type = Type::One;
			Name name = Name::Invalid;
			/// @c true if this ROM is optional for machine startup. Generally indicates something
			/// that would make emulation more accurate, but not sufficiently so to make it
			/// a necessity.
			bool is_optional = false;
			std::vector<Node> children;

			void add_descriptions(std::vector<Description> &) const;
			bool validate(Map &) const;
			void visit(
				const std::function<void(ListType)> &enter_list,
				const std::function<void(void)> &exit_list,
				const std::function<void(ROM::Request::ListType type, const ROM::Description &, bool is_optional, size_t remaining)> &add_item
			) const;
		};
		Node node;

		Request append(Node::Type type, const Request &rhs);
};

}

#endif /* ROMCatalogue_hpp */
