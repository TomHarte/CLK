//
//  StaticAnalyser.cpp
//  Clock Signal
//
//  Created by Thomas Harte on 23/08/2016.
//  Copyright 2016 Thomas Harte. All rights reserved.
//

#include "StaticAnalyser.hpp"

#include <algorithm>
#include <cstdlib>
#include <cstring>
#include <iterator>

// Analysers
#include "Acorn/StaticAnalyser.hpp"
#include "AmstradCPC/StaticAnalyser.hpp"
#include "AppleII/StaticAnalyser.hpp"
#include "AppleIIgs/StaticAnalyser.hpp"
#include "Atari2600/StaticAnalyser.hpp"
#include "AtariST/StaticAnalyser.hpp"
#include "Coleco/StaticAnalyser.hpp"
#include "Commodore/StaticAnalyser.hpp"
#include "DiskII/StaticAnalyser.hpp"
#include "Enterprise/StaticAnalyser.hpp"
#include "Macintosh/StaticAnalyser.hpp"
#include "MSX/StaticAnalyser.hpp"
#include "Oric/StaticAnalyser.hpp"
#include "Sega/StaticAnalyser.hpp"
#include "ZX8081/StaticAnalyser.hpp"
#include "ZXSpectrum/StaticAnalyser.hpp"

// Cartridges
#include "../../Storage/Cartridge/Formats/BinaryDump.hpp"
#include "../../Storage/Cartridge/Formats/PRG.hpp"

// Disks
#include "../../Storage/Disk/DiskImage/Formats/2MG.hpp"
#include "../../Storage/Disk/DiskImage/Formats/AcornADF.hpp"
#include "../../Storage/Disk/DiskImage/Formats/AmigaADF.hpp"
#include "../../Storage/Disk/DiskImage/Formats/AppleDSK.hpp"
#include "../../Storage/Disk/DiskImage/Formats/CPCDSK.hpp"
#include "../../Storage/Disk/DiskImage/Formats/D64.hpp"
#include "../../Storage/Disk/DiskImage/Formats/MacintoshIMG.hpp"
#include "../../Storage/Disk/DiskImage/Formats/G64.hpp"
#include "../../Storage/Disk/DiskImage/Formats/DMK.hpp"
#include "../../Storage/Disk/DiskImage/Formats/FAT12.hpp"
#include "../../Storage/Disk/DiskImage/Formats/HFE.hpp"
#include "../../Storage/Disk/DiskImage/Formats/MSA.hpp"
#include "../../Storage/Disk/DiskImage/Formats/NIB.hpp"
#include "../../Storage/Disk/DiskImage/Formats/OricMFMDSK.hpp"
#include "../../Storage/Disk/DiskImage/Formats/SSD.hpp"
#include "../../Storage/Disk/DiskImage/Formats/ST.hpp"
#include "../../Storage/Disk/DiskImage/Formats/STX.hpp"
#include "../../Storage/Disk/DiskImage/Formats/WOZ.hpp"

// Mass Storage Devices (i.e. usually, hard disks)
#include "../../Storage/MassStorage/Formats/DAT.hpp"
#include "../../Storage/MassStorage/Formats/DSK.hpp"
#include "../../Storage/MassStorage/Formats/HFV.hpp"

// State Snapshots
#include "../../Storage/State/SNA.hpp"
#include "../../Storage/State/SZX.hpp"
#include "../../Storage/State/Z80.hpp"

// Tapes
#include "../../Storage/Tape/Formats/CAS.hpp"
#include "../../Storage/Tape/Formats/CommodoreTAP.hpp"
#include "../../Storage/Tape/Formats/CSW.hpp"
#include "../../Storage/Tape/Formats/OricTAP.hpp"
#include "../../Storage/Tape/Formats/TapePRG.hpp"
#include "../../Storage/Tape/Formats/TapeUEF.hpp"
#include "../../Storage/Tape/Formats/TZX.hpp"
#include "../../Storage/Tape/Formats/ZX80O81P.hpp"
#include "../../Storage/Tape/Formats/ZXSpectrumTAP.hpp"

// Target Platform Types
#include "../../Storage/TargetPlatforms.hpp"

using namespace Analyser::Static;

namespace {

std::string get_extension(const std::string &name) {
	// Get the extension, if any; it will be assumed that extensions are reliable, so an extension is a broad-phase
	// test as to file format.
	std::string::size_type final_dot = name.find_last_of(".");
	if(final_dot == std::string::npos) return name;
	std::string extension = name.substr(final_dot + 1);
	std::transform(extension.begin(), extension.end(), extension.begin(), ::tolower);
	return extension;
}

}

static Media GetMediaAndPlatforms(const std::string &file_name, TargetPlatform::IntType &potential_platforms) {
	Media result;
	const std::string extension = get_extension(file_name);

#define InsertInstance(list, instance, platforms) \
	list.emplace_back(instance);\
	potential_platforms |= platforms;\
	TargetPlatform::TypeDistinguisher *distinguisher = dynamic_cast<TargetPlatform::TypeDistinguisher *>(list.back().get());\
	if(distinguisher) potential_platforms &= distinguisher->target_platform_type(); \

#define Insert(list, class, platforms, ...) \
	InsertInstance(list, new Storage::class(__VA_ARGS__), platforms);

#define TryInsert(list, class, platforms, ...) \
	try {\
		Insert(list, class, platforms, __VA_ARGS__) \
	} catch(...) {}

#define Format(ext, list, class, platforms) \
	if(extension == ext)	{		\
		TryInsert(list, class, platforms, file_name)	\
	}

	// 2MG
	if(extension == "2mg") {
		// 2MG uses a factory method; defer to it.
		try {
			InsertInstance(result.disks, Storage::Disk::Disk2MG::open(file_name), TargetPlatform::DiskII)
		} catch(...) {}
	}

	Format("80", result.tapes, Tape::ZX80O81P, TargetPlatform::ZX8081)											// 80
	Format("81", result.tapes, Tape::ZX80O81P, TargetPlatform::ZX8081)											// 81
	Format("a26", result.cartridges, Cartridge::BinaryDump, TargetPlatform::Atari2600)							// A26
	Format("adf", result.disks, Disk::DiskImageHolder<Storage::Disk::AcornADF>, TargetPlatform::Acorn)			// ADF (Acorn)
	Format("adf", result.disks, Disk::DiskImageHolder<Storage::Disk::AmigaADF>, TargetPlatform::Amiga)			// ADF (Amiga)
	Format("adl", result.disks, Disk::DiskImageHolder<Storage::Disk::AcornADF>, TargetPlatform::Acorn)			// ADL
	Format("bin", result.cartridges, Cartridge::BinaryDump, TargetPlatform::AllCartridge)						// BIN (cartridge dump)
	Format("cas", result.tapes, Tape::CAS, TargetPlatform::MSX)													// CAS
	Format("cdt", result.tapes, Tape::TZX, TargetPlatform::AmstradCPC)											// CDT
	Format("col", result.cartridges, Cartridge::BinaryDump, TargetPlatform::Coleco)								// COL
	Format("csw", result.tapes, Tape::CSW, TargetPlatform::AllTape)												// CSW
	Format("d64", result.disks, Disk::DiskImageHolder<Storage::Disk::D64>, TargetPlatform::Commodore)			// D64
	Format("dat", result.mass_storage_devices, MassStorage::DAT, TargetPlatform::Acorn)							// DAT
	Format("dmk", result.disks, Disk::DiskImageHolder<Storage::Disk::DMK>, TargetPlatform::MSX)					// DMK
	Format("do", result.disks, Disk::DiskImageHolder<Storage::Disk::AppleDSK>, TargetPlatform::DiskII)			// DO
	Format("dsd", result.disks, Disk::DiskImageHolder<Storage::Disk::SSD>, TargetPlatform::Acorn)				// DSD
	Format(	"dsk",
			result.disks,
			Disk::DiskImageHolder<Storage::Disk::CPCDSK>,
			TargetPlatform::AmstradCPC | TargetPlatform::Oric | TargetPlatform::ZXSpectrum)						// DSK (Amstrad CPC, etc)
	Format("dsk", result.disks, Disk::DiskImageHolder<Storage::Disk::AppleDSK>, TargetPlatform::DiskII)			// DSK (Apple II)
	Format("dsk", result.disks, Disk::DiskImageHolder<Storage::Disk::MacintoshIMG>, TargetPlatform::Macintosh)	// DSK (Macintosh, floppy disk)
	Format("dsk", result.mass_storage_devices, MassStorage::HFV, TargetPlatform::Macintosh)						// DSK (Macintosh, hard disk, single volume image)
	Format("dsk", result.mass_storage_devices, MassStorage::DSK, TargetPlatform::Macintosh)						// DSK (Macintosh, hard disk, full device image)
	Format("dsk", result.disks, Disk::DiskImageHolder<Storage::Disk::FAT12>, TargetPlatform::MSX)				// DSK (MSX)
	Format("dsk", result.disks, Disk::DiskImageHolder<Storage::Disk::OricMFMDSK>, TargetPlatform::Oric)			// DSK (Oric)
	Format("g64", result.disks, Disk::DiskImageHolder<Storage::Disk::G64>, TargetPlatform::Commodore)			// G64
	Format(	"hfe",
			result.disks,
			Disk::DiskImageHolder<Storage::Disk::HFE>,
			TargetPlatform::Acorn | TargetPlatform::AmstradCPC | TargetPlatform::Commodore | TargetPlatform::Oric | TargetPlatform::ZXSpectrum)
			// HFE (TODO: switch to AllDisk once the MSX stops being so greedy)
	Format("img", result.disks, Disk::DiskImageHolder<Storage::Disk::MacintoshIMG>, TargetPlatform::Macintosh)		// IMG (DiskCopy 4.2)
	Format("image", result.disks, Disk::DiskImageHolder<Storage::Disk::MacintoshIMG>, TargetPlatform::Macintosh)	// IMG (DiskCopy 4.2)
	Format("img", result.disks, Disk::DiskImageHolder<Storage::Disk::FAT12>, TargetPlatform::Enterprise)		// IMG (Enterprise/MS-DOS style)
	Format("msa", result.disks, Disk::DiskImageHolder<Storage::Disk::MSA>, TargetPlatform::AtariST)				// MSA
	Format("nib", result.disks, Disk::DiskImageHolder<Storage::Disk::NIB>, TargetPlatform::DiskII)				// NIB
	Format("o", result.tapes, Tape::ZX80O81P, TargetPlatform::ZX8081)											// O
	Format("p", result.tapes, Tape::ZX80O81P, TargetPlatform::ZX8081)											// P
	Format("po", result.disks, Disk::DiskImageHolder<Storage::Disk::AppleDSK>, TargetPlatform::DiskII)			// PO (original Apple II kind)

	// PO (Apple IIgs kind)
	if(extension == "po")	{
		TryInsert(result.disks, Disk::DiskImageHolder<Storage::Disk::MacintoshIMG>, TargetPlatform::AppleIIgs, file_name, Storage::Disk::MacintoshIMG::FixedType::GCR)
	}

	Format("p81", result.tapes, Tape::ZX80O81P, TargetPlatform::ZX8081)											// P81

	// PRG
	if(extension == "prg") {
		// try instantiating as a ROM; failing that accept as a tape
		try {
			Insert(result.cartridges, Cartridge::PRG, TargetPlatform::Commodore, file_name)
		} catch(...) {
			try {
				Insert(result.tapes, Tape::PRG, TargetPlatform::Commodore, file_name)
			} catch(...) {}
		}
	}

	Format(	"rom",
			result.cartridges,
			Cartridge::BinaryDump,
			TargetPlatform::AcornElectron | TargetPlatform::Coleco | TargetPlatform::MSX)						// ROM
	Format("sg", result.cartridges, Cartridge::BinaryDump, TargetPlatform::Sega)								// SG
	Format("sms", result.cartridges, Cartridge::BinaryDump, TargetPlatform::Sega)								// SMS
	Format("ssd", result.disks, Disk::DiskImageHolder<Storage::Disk::SSD>, TargetPlatform::Acorn)				// SSD
	Format("st", result.disks, Disk::DiskImageHolder<Storage::Disk::ST>, TargetPlatform::AtariST)				// ST
	Format("stx", result.disks, Disk::DiskImageHolder<Storage::Disk::STX>, TargetPlatform::AtariST)				// STX
	Format("tap", result.tapes, Tape::CommodoreTAP, TargetPlatform::Commodore)									// TAP (Commodore)
	Format("tap", result.tapes, Tape::OricTAP, TargetPlatform::Oric)											// TAP (Oric)
	Format("tap", result.tapes, Tape::ZXSpectrumTAP, TargetPlatform::ZXSpectrum)								// TAP (ZX Spectrum)
	Format("tsx", result.tapes, Tape::TZX, TargetPlatform::MSX)													// TSX
	Format("tzx", result.tapes, Tape::TZX, TargetPlatform::ZX8081 | TargetPlatform::ZXSpectrum)					// TZX
	Format("uef", result.tapes, Tape::UEF, TargetPlatform::Acorn)												// UEF (tape)
	Format("woz", result.disks, Disk::DiskImageHolder<Storage::Disk::WOZ>, TargetPlatform::DiskII)				// WOZ

#undef Format
#undef Insert
#undef TryInsert
#undef InsertInstance

	return result;
}

Media Analyser::Static::GetMedia(const std::string &file_name) {
	TargetPlatform::IntType throwaway;
	return GetMediaAndPlatforms(file_name, throwaway);
}

TargetList Analyser::Static::GetTargets(const std::string &file_name) {
	TargetList targets;
	const std::string extension = get_extension(file_name);

	// Check whether the file directly identifies a target; if so then just return that.
#define Format(ext, class) 											\
	if(extension == ext)	{										\
		try {														\
			auto target = Storage::State::class::load(file_name);	\
			if(target) {											\
				targets.push_back(std::move(target));				\
				return targets;										\
			}														\
		} catch(...) {}												\
	}

	Format("sna", SNA);
	Format("szx", SZX);
	Format("z80", Z80);

#undef TryInsert

	// Otherwise:
	//
	// Collect all disks, tapes ROMs, etc as can be extrapolated from this file, forming the
	// union of all platforms this file might be a target for.
	TargetPlatform::IntType potential_platforms = 0;
	Media media = GetMediaAndPlatforms(file_name, potential_platforms);

	// Hand off to platform-specific determination of whether these
	// things are actually compatible and, if so, how to load them.
#define Append(x) if(potential_platforms & TargetPlatform::x) {\
	auto new_targets = x::GetTargets(media, file_name, potential_platforms);\
	std::move(new_targets.begin(), new_targets.end(), std::back_inserter(targets));\
}
	Append(Acorn);
	Append(AmstradCPC);
	Append(AppleII);
	Append(AppleIIgs);
	Append(Atari2600);
	Append(AtariST);
	Append(Coleco);
	Append(Commodore);
	Append(DiskII);
	Append(Enterprise);
	Append(Macintosh);
	Append(MSX);
	Append(Oric);
	Append(Sega);
	Append(ZX8081);
	Append(ZXSpectrum);
#undef Append

	// Reset any tapes to their initial position.
	for(const auto &target : targets) {
		for(auto &tape : target->media.tapes) {
			tape->reset();
		}
	}

	// Sort by initial confidence. Use a stable sort in case any of the machine-specific analysers
	// picked their insertion order carefully.
	std::stable_sort(targets.begin(), targets.end(),
		[] (const std::unique_ptr<Target> &a, const std::unique_ptr<Target> &b) {
			return a->confidence > b->confidence;
		});

	return targets;
}
