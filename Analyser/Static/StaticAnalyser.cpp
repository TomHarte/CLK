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
#include "Atari2600/StaticAnalyser.hpp"
#include "AtariST/StaticAnalyser.hpp"
#include "Coleco/StaticAnalyser.hpp"
#include "Commodore/StaticAnalyser.hpp"
#include "DiskII/StaticAnalyser.hpp"
#include "Macintosh/StaticAnalyser.hpp"
#include "MSX/StaticAnalyser.hpp"
#include "Oric/StaticAnalyser.hpp"
#include "Sega/StaticAnalyser.hpp"
#include "ZX8081/StaticAnalyser.hpp"

// Cartridges
#include "../../Storage/Cartridge/Formats/BinaryDump.hpp"
#include "../../Storage/Cartridge/Formats/PRG.hpp"

// Disks
#include "../../Storage/Disk/DiskImage/Formats/AcornADF.hpp"
#include "../../Storage/Disk/DiskImage/Formats/AppleDSK.hpp"
#include "../../Storage/Disk/DiskImage/Formats/CPCDSK.hpp"
#include "../../Storage/Disk/DiskImage/Formats/D64.hpp"
#include "../../Storage/Disk/DiskImage/Formats/MacintoshIMG.hpp"
#include "../../Storage/Disk/DiskImage/Formats/G64.hpp"
#include "../../Storage/Disk/DiskImage/Formats/DMK.hpp"
#include "../../Storage/Disk/DiskImage/Formats/HFE.hpp"
#include "../../Storage/Disk/DiskImage/Formats/MSA.hpp"
#include "../../Storage/Disk/DiskImage/Formats/MSXDSK.hpp"
#include "../../Storage/Disk/DiskImage/Formats/NIB.hpp"
#include "../../Storage/Disk/DiskImage/Formats/OricMFMDSK.hpp"
#include "../../Storage/Disk/DiskImage/Formats/SSD.hpp"
#include "../../Storage/Disk/DiskImage/Formats/ST.hpp"
#include "../../Storage/Disk/DiskImage/Formats/STX.hpp"
#include "../../Storage/Disk/DiskImage/Formats/WOZ.hpp"

// Mass Storage Devices (i.e. usually, hard disks)
#include "../../Storage/MassStorage/Formats/DAT.hpp"
#include "../../Storage/MassStorage/Formats/HFV.hpp"

// Tapes
#include "../../Storage/Tape/Formats/CAS.hpp"
#include "../../Storage/Tape/Formats/CommodoreTAP.hpp"
#include "../../Storage/Tape/Formats/CSW.hpp"
#include "../../Storage/Tape/Formats/OricTAP.hpp"
#include "../../Storage/Tape/Formats/TapePRG.hpp"
#include "../../Storage/Tape/Formats/TapeUEF.hpp"
#include "../../Storage/Tape/Formats/TZX.hpp"
#include "../../Storage/Tape/Formats/ZX80O81P.hpp"

// Target Platform Types
#include "../../Storage/TargetPlatforms.hpp"

using namespace Analyser::Static;

static Media GetMediaAndPlatforms(const std::string &file_name, TargetPlatform::IntType &potential_platforms) {
	Media result;

	// Get the extension, if any; it will be assumed that extensions are reliable, so an extension is a broad-phase
	// test as to file format.
	std::string::size_type final_dot = file_name.find_last_of(".");
	if(final_dot == std::string::npos) return result;
	std::string extension = file_name.substr(final_dot + 1);
	std::transform(extension.begin(), extension.end(), extension.begin(), ::tolower);

#define Insert(list, class, platforms) \
	list.emplace_back(new Storage::class(file_name));\
	potential_platforms |= platforms;\
	TargetPlatform::TypeDistinguisher *distinguisher = dynamic_cast<TargetPlatform::TypeDistinguisher *>(list.back().get());\
	if(distinguisher) potential_platforms &= distinguisher->target_platform_type();

#define TryInsert(list, class, platforms) \
	try {\
		Insert(list, class, platforms) \
	} catch(...) {}

#define Format(ext, list, class, platforms) \
	if(extension == ext)	{		\
		TryInsert(list, class, platforms)	\
	}

	Format("80", result.tapes, Tape::ZX80O81P, TargetPlatform::ZX8081)											// 80
	Format("81", result.tapes, Tape::ZX80O81P, TargetPlatform::ZX8081)											// 81
	Format("a26", result.cartridges, Cartridge::BinaryDump, TargetPlatform::Atari2600)							// A26
	Format("adf", result.disks, Disk::DiskImageHolder<Storage::Disk::AcornADF>, TargetPlatform::Acorn)			// ADF
	Format("adl", result.disks, Disk::DiskImageHolder<Storage::Disk::AcornADF>, TargetPlatform::Acorn)			// ADL
	Format("bin", result.cartridges, Cartridge::BinaryDump, TargetPlatform::AllCartridge)						// BIN (cartridge dump)
	Format("cas", result.tapes, Tape::CAS, TargetPlatform::MSX)													// CAS
	Format("cdt", result.tapes, Tape::TZX, TargetPlatform::AmstradCPC)											// CDT
	Format("col", result.cartridges, Cartridge::BinaryDump, TargetPlatform::ColecoVision)						// COL
	Format("csw", result.tapes, Tape::CSW, TargetPlatform::AllTape)												// CSW
	Format("d64", result.disks, Disk::DiskImageHolder<Storage::Disk::D64>, TargetPlatform::Commodore)			// D64
	Format("dat", result.mass_storage_devices, MassStorage::DAT, TargetPlatform::Acorn)							// DAT
	Format("dmk", result.disks, Disk::DiskImageHolder<Storage::Disk::DMK>, TargetPlatform::MSX)					// DMK
	Format("do", result.disks, Disk::DiskImageHolder<Storage::Disk::AppleDSK>, TargetPlatform::DiskII)			// DO
	Format("dsd", result.disks, Disk::DiskImageHolder<Storage::Disk::SSD>, TargetPlatform::Acorn)				// DSD
	Format(	"dsk",
			result.disks,
			Disk::DiskImageHolder<Storage::Disk::CPCDSK>,
			TargetPlatform::AmstradCPC | TargetPlatform::Oric)													// DSK (Amstrad CPC)
	Format("dsk", result.disks, Disk::DiskImageHolder<Storage::Disk::AppleDSK>, TargetPlatform::DiskII)			// DSK (Apple II)
	Format("dsk", result.disks, Disk::DiskImageHolder<Storage::Disk::MacintoshIMG>, TargetPlatform::Macintosh)	// DSK (Macintosh, floppy disk)
	Format("dsk", result.mass_storage_devices, MassStorage::HFV, TargetPlatform::Macintosh)						// DSK (Macintosh, hard disk)
	Format("dsk", result.disks, Disk::DiskImageHolder<Storage::Disk::MSXDSK>, TargetPlatform::MSX)				// DSK (MSX)
	Format("dsk", result.disks, Disk::DiskImageHolder<Storage::Disk::OricMFMDSK>, TargetPlatform::Oric)			// DSK (Oric)
	Format("g64", result.disks, Disk::DiskImageHolder<Storage::Disk::G64>, TargetPlatform::Commodore)			// G64
	Format(	"hfe",
			result.disks,
			Disk::DiskImageHolder<Storage::Disk::HFE>,
			TargetPlatform::Acorn | TargetPlatform::AmstradCPC | TargetPlatform::Commodore | TargetPlatform::Oric)
			// HFE (TODO: switch to AllDisk once the MSX stops being so greedy)
	Format("img", result.disks, Disk::DiskImageHolder<Storage::Disk::MacintoshIMG>, TargetPlatform::Macintosh)		// IMG (DiskCopy 4.2)
	Format("image", result.disks, Disk::DiskImageHolder<Storage::Disk::MacintoshIMG>, TargetPlatform::Macintosh)	// IMG (DiskCopy 4.2)
	Format("msa", result.disks, Disk::DiskImageHolder<Storage::Disk::MSA>, TargetPlatform::AtariST)				// MSA
	Format("nib", result.disks, Disk::DiskImageHolder<Storage::Disk::NIB>, TargetPlatform::DiskII)				// NIB
	Format("o", result.tapes, Tape::ZX80O81P, TargetPlatform::ZX8081)											// O
	Format("p", result.tapes, Tape::ZX80O81P, TargetPlatform::ZX8081)											// P
	Format("po", result.disks, Disk::DiskImageHolder<Storage::Disk::AppleDSK>, TargetPlatform::DiskII)			// PO
	Format("p81", result.tapes, Tape::ZX80O81P, TargetPlatform::ZX8081)											// P81

	// PRG
	if(extension == "prg") {
		// try instantiating as a ROM; failing that accept as a tape
		try {
			Insert(result.cartridges, Cartridge::PRG, TargetPlatform::Commodore)
		} catch(...) {
			try {
				Insert(result.tapes, Tape::PRG, TargetPlatform::Commodore)
			} catch(...) {}
		}
	}

	Format(	"rom",
			result.cartridges,
			Cartridge::BinaryDump,
			TargetPlatform::AcornElectron | TargetPlatform::ColecoVision | TargetPlatform::MSX)					// ROM
	Format("sg", result.cartridges, Cartridge::BinaryDump, TargetPlatform::Sega)								// SG
	Format("sms", result.cartridges, Cartridge::BinaryDump, TargetPlatform::Sega)								// SMS
	Format("ssd", result.disks, Disk::DiskImageHolder<Storage::Disk::SSD>, TargetPlatform::Acorn)				// SSD
	Format("st", result.disks, Disk::DiskImageHolder<Storage::Disk::ST>, TargetPlatform::AtariST)				// ST
	Format("stx", result.disks, Disk::DiskImageHolder<Storage::Disk::STX>, TargetPlatform::AtariST)				// STX
	Format("tap", result.tapes, Tape::CommodoreTAP, TargetPlatform::Commodore)									// TAP (Commodore)
	Format("tap", result.tapes, Tape::OricTAP, TargetPlatform::Oric)											// TAP (Oric)
	Format("tsx", result.tapes, Tape::TZX, TargetPlatform::MSX)													// TSX
	Format("tzx", result.tapes, Tape::TZX, TargetPlatform::ZX8081)												// TZX
	Format("uef", result.tapes, Tape::UEF, TargetPlatform::Acorn)												// UEF (tape)
	Format("woz", result.disks, Disk::DiskImageHolder<Storage::Disk::WOZ>, TargetPlatform::DiskII)				// WOZ

#undef Format
#undef Insert
#undef TryInsert

	return result;
}

Media Analyser::Static::GetMedia(const std::string &file_name) {
	TargetPlatform::IntType throwaway;
	return GetMediaAndPlatforms(file_name, throwaway);
}

TargetList Analyser::Static::GetTargets(const std::string &file_name) {
	TargetList targets;

	// Collect all disks, tapes ROMs, etc as can be extrapolated from this file, forming the
	// union of all platforms this file might be a target for.
	TargetPlatform::IntType potential_platforms = 0;
	Media media = GetMediaAndPlatforms(file_name, potential_platforms);

	// Hand off to platform-specific determination of whether these things are actually compatible and,
	// if so, how to load them.
	#define Append(x) {\
		auto new_targets = x::GetTargets(media, file_name, potential_platforms);\
		std::move(new_targets.begin(), new_targets.end(), std::back_inserter(targets));\
	}
	if(potential_platforms & TargetPlatform::Acorn)			Append(Acorn);
	if(potential_platforms & TargetPlatform::AmstradCPC)	Append(AmstradCPC);
	if(potential_platforms & TargetPlatform::AppleII)		Append(AppleII);
	if(potential_platforms & TargetPlatform::Atari2600)		Append(Atari2600);
	if(potential_platforms & TargetPlatform::AtariST)		Append(AtariST);
	if(potential_platforms & TargetPlatform::ColecoVision)	Append(Coleco);
	if(potential_platforms & TargetPlatform::Commodore)		Append(Commodore);
	if(potential_platforms & TargetPlatform::DiskII)		Append(DiskII);
	if(potential_platforms & TargetPlatform::Macintosh)		Append(Macintosh);
	if(potential_platforms & TargetPlatform::MSX)			Append(MSX);
	if(potential_platforms & TargetPlatform::Oric)			Append(Oric);
	if(potential_platforms & TargetPlatform::Sega)			Append(Sega);
	if(potential_platforms & TargetPlatform::ZX8081)		Append(ZX8081);
	#undef Append

	// Reset any tapes to their initial position
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
