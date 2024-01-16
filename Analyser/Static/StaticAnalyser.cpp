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
#include "Amiga/StaticAnalyser.hpp"
#include "AmstradCPC/StaticAnalyser.hpp"
#include "AppleII/StaticAnalyser.hpp"
#include "AppleIIgs/StaticAnalyser.hpp"
#include "Atari2600/StaticAnalyser.hpp"
#include "AtariST/StaticAnalyser.hpp"
#include "Coleco/StaticAnalyser.hpp"
#include "Commodore/StaticAnalyser.hpp"
#include "DiskII/StaticAnalyser.hpp"
#include "Enterprise/StaticAnalyser.hpp"
#include "FAT12/StaticAnalyser.hpp"
#include "Macintosh/StaticAnalyser.hpp"
#include "MSX/StaticAnalyser.hpp"
#include "Oric/StaticAnalyser.hpp"
#include "PCCompatible/StaticAnalyser.hpp"
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
#include "../../Storage/Disk/DiskImage/Formats/G64.hpp"
#include "../../Storage/Disk/DiskImage/Formats/DMK.hpp"
#include "../../Storage/Disk/DiskImage/Formats/FAT12.hpp"
#include "../../Storage/Disk/DiskImage/Formats/HFE.hpp"
#include "../../Storage/Disk/DiskImage/Formats/IPF.hpp"
#include "../../Storage/Disk/DiskImage/Formats/IMD.hpp"
#include "../../Storage/Disk/DiskImage/Formats/MacintoshIMG.hpp"
#include "../../Storage/Disk/DiskImage/Formats/MSA.hpp"
#include "../../Storage/Disk/DiskImage/Formats/NIB.hpp"
#include "../../Storage/Disk/DiskImage/Formats/OricMFMDSK.hpp"
#include "../../Storage/Disk/DiskImage/Formats/PCBooter.hpp"
#include "../../Storage/Disk/DiskImage/Formats/SSD.hpp"
#include "../../Storage/Disk/DiskImage/Formats/STX.hpp"
#include "../../Storage/Disk/DiskImage/Formats/WOZ.hpp"

// Mass Storage Devices (i.e. usually, hard disks)
#include "../../Storage/MassStorage/Formats/DAT.hpp"
#include "../../Storage/MassStorage/Formats/DSK.hpp"
#include "../../Storage/MassStorage/Formats/HDV.hpp"
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

template<class> inline constexpr bool always_false_v = false;

using namespace Analyser::Static;
using namespace Storage;

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

/// Adds @c instance to @c list and adds @c platforms to the set of @c potential_platforms.
/// If @c instance is an @c TargetPlatform::TypeDistinguisher then it is given an opportunity to restrict the set of @c potential_platforms.
template <typename ContainerT, typename InstanceT>
void insert_instance(ContainerT &list, InstanceT instance, TargetPlatform::IntType &potential_platforms, TargetPlatform::IntType platforms) {
	list.emplace_back(instance);
	potential_platforms |= platforms;

	TargetPlatform::TypeDistinguisher *const distinguisher =
		dynamic_cast<TargetPlatform::TypeDistinguisher *>(list.back().get());
	if(distinguisher) potential_platforms &= distinguisher->target_platform_type();
}

/// Concstructs a new instance of @c InstanceT supplying @c args and adds it to the back of @c list using @c insert_instance.
template <typename InstanceT, typename ContainerT, typename... Args>
void insert(ContainerT &list, TargetPlatform::IntType &potential_platforms, TargetPlatform::IntType platforms, Args &&... args) {
	insert_instance(list, new InstanceT(std::forward<Args>(args)...), potential_platforms, platforms);
}

/// Calls @c insert with the specified parameters, ignoring any exceptions thrown.
template <typename InstanceT, typename ContainerT, typename... Args>
void try_insert(ContainerT &list, TargetPlatform::IntType &potential_platforms, TargetPlatform::IntType platforms, Args &&... args) {
	try {
		insert<InstanceT>(list, potential_platforms, platforms, std::forward<Args>(args)...);
	} catch(...) {}
}

}

static Media GetMediaAndPlatforms(const std::string &file_name, TargetPlatform::IntType &potential_platforms) {
	const std::string extension = get_extension(file_name);

	Media result;

#define Format(ext, list, class, platforms) \
	if(extension == ext)	{		\
		try_insert<class>(list, potential_platforms, platforms, file_name);	\
	}

	// 2MG
	if(extension == "2mg") {
		// 2MG uses a factory method; defer to it.
		try {
			const auto media = Disk::Disk2MG::open(file_name);
			std::visit([&](auto &&arg) {
				using Type = typename std::decay<decltype(arg)>::type;

				if constexpr (std::is_same<Type, nullptr_t>::value) {
					// It's valid for no media to be returned.
				} else if constexpr (std::is_same<Type, Disk::DiskImageHolderBase *>::value) {
					insert_instance(result.disks, arg, potential_platforms, TargetPlatform::DiskII);
				} else if constexpr (std::is_same<Type, MassStorage::MassStorageDevice *>::value) {
					// TODO: or is it Apple IIgs?
					insert_instance(result.mass_storage_devices, arg, potential_platforms, TargetPlatform::AppleII);
				} else {
					static_assert(always_false_v<Type>, "Unexpected type encountered.");
				}
			}, media);
		} catch(...) {}
	}

	Format("80", result.tapes, Tape::ZX80O81P, TargetPlatform::ZX8081)											// 80
	Format("81", result.tapes, Tape::ZX80O81P, TargetPlatform::ZX8081)											// 81
	Format("a26", result.cartridges, Cartridge::BinaryDump, TargetPlatform::Atari2600)							// A26
	Format("adf", result.disks, Disk::DiskImageHolder<Disk::AcornADF>, TargetPlatform::Acorn)			// ADF (Acorn)
	Format("adf", result.disks, Disk::DiskImageHolder<Disk::AmigaADF>, TargetPlatform::Amiga)			// ADF (Amiga)
	Format("adl", result.disks, Disk::DiskImageHolder<Disk::AcornADF>, TargetPlatform::Acorn)			// ADL
	Format("bin", result.cartridges, Cartridge::BinaryDump, TargetPlatform::AllCartridge)						// BIN (cartridge dump)
	Format("cas", result.tapes, Tape::CAS, TargetPlatform::MSX)													// CAS
	Format("cdt", result.tapes, Tape::TZX, TargetPlatform::AmstradCPC)											// CDT
	Format("col", result.cartridges, Cartridge::BinaryDump, TargetPlatform::Coleco)								// COL
	Format("csw", result.tapes, Tape::CSW, TargetPlatform::AllTape)												// CSW
	Format("d64", result.disks, Disk::DiskImageHolder<Disk::D64>, TargetPlatform::Commodore)			// D64
	Format("dat", result.mass_storage_devices, MassStorage::DAT, TargetPlatform::Acorn)							// DAT
	Format("dmk", result.disks, Disk::DiskImageHolder<Disk::DMK>, TargetPlatform::MSX)					// DMK
	Format("do", result.disks, Disk::DiskImageHolder<Disk::AppleDSK>, TargetPlatform::DiskII)			// DO
	Format("dsd", result.disks, Disk::DiskImageHolder<Disk::SSD>, TargetPlatform::Acorn)				// DSD
	Format(	"dsk",
			result.disks,
			Disk::DiskImageHolder<Disk::CPCDSK>,
			TargetPlatform::AmstradCPC | TargetPlatform::Oric | TargetPlatform::ZXSpectrum)						// DSK (Amstrad CPC, etc)
	Format("dsk", result.disks, Disk::DiskImageHolder<Disk::AppleDSK>, TargetPlatform::DiskII)			// DSK (Apple II)
	Format("dsk", result.disks, Disk::DiskImageHolder<Disk::MacintoshIMG>, TargetPlatform::Macintosh)	// DSK (Macintosh, floppy disk)
	Format("dsk", result.mass_storage_devices, MassStorage::HFV, TargetPlatform::Macintosh)						// DSK (Macintosh, hard disk, single volume image)
	Format("dsk", result.mass_storage_devices, MassStorage::DSK, TargetPlatform::Macintosh)						// DSK (Macintosh, hard disk, full device image)
	Format("dsk", result.disks, Disk::DiskImageHolder<Disk::FAT12>, TargetPlatform::MSX)				// DSK (MSX)
	Format("dsk", result.disks, Disk::DiskImageHolder<Disk::OricMFMDSK>, TargetPlatform::Oric)			// DSK (Oric)
	Format("g64", result.disks, Disk::DiskImageHolder<Disk::G64>, TargetPlatform::Commodore)			// G64
	Format("hdv", result.mass_storage_devices, MassStorage::HDV, TargetPlatform::AppleII)						// HDV (Apple II, hard disk, single volume image)
	Format(	"hfe",
			result.disks,
			Disk::DiskImageHolder<Disk::HFE>,
			TargetPlatform::Acorn | TargetPlatform::AmstradCPC | TargetPlatform::Commodore | TargetPlatform::Oric | TargetPlatform::ZXSpectrum)
			// HFE (TODO: switch to AllDisk once the MSX stops being so greedy)
	Format("ima", result.disks, Disk::DiskImageHolder<Disk::FAT12>, TargetPlatform::PCCompatible)			// IMG (MS-DOS style)
	Format("image", result.disks, Disk::DiskImageHolder<Disk::MacintoshIMG>, TargetPlatform::Macintosh)	// IMG (DiskCopy 4.2)
	Format("imd", result.disks, Disk::DiskImageHolder<Disk::IMD>, TargetPlatform::PCCompatible)			// IMD
	Format("img", result.disks, Disk::DiskImageHolder<Disk::MacintoshIMG>, TargetPlatform::Macintosh)		// IMG (DiskCopy 4.2)

	// Treat PC booter as a potential backup only if this doesn't parse as a FAT12.
	if(extension == "img") {
		try {
			insert<Disk::DiskImageHolder<Disk::FAT12>>(result.disks, potential_platforms, TargetPlatform::FAT12, file_name);				// IMG (Enterprise or MS-DOS style)
		} catch(...) {
			Format("img", result.disks, Disk::DiskImageHolder<Disk::PCBooter>, TargetPlatform::PCCompatible)		// IMG (PC raw booter)
		}
	}

	Format(	"ipf",
			result.disks,
			Disk::DiskImageHolder<Disk::IPF>,
			TargetPlatform::Amiga | TargetPlatform::AtariST | TargetPlatform::AmstradCPC | TargetPlatform::ZXSpectrum)		// IPF
	Format("msa", result.disks, Disk::DiskImageHolder<Disk::MSA>, TargetPlatform::AtariST)				// MSA
	Format("mx2", result.cartridges, Cartridge::BinaryDump, TargetPlatform::MSX)								// MX2
	Format("nib", result.disks, Disk::DiskImageHolder<Disk::NIB>, TargetPlatform::DiskII)				// NIB
	Format("o", result.tapes, Tape::ZX80O81P, TargetPlatform::ZX8081)											// O
	Format("p", result.tapes, Tape::ZX80O81P, TargetPlatform::ZX8081)											// P
	Format("po", result.disks, Disk::DiskImageHolder<Disk::AppleDSK>, TargetPlatform::DiskII)			// PO (original Apple II kind)

	// PO (Apple IIgs kind)
	if(extension == "po")	{
		try_insert<Disk::DiskImageHolder<Disk::MacintoshIMG>>(
			result.disks,
			potential_platforms, TargetPlatform::AppleIIgs,
			file_name, Disk::MacintoshIMG::FixedType::GCR);
	}

	Format("p81", result.tapes, Tape::ZX80O81P, TargetPlatform::ZX8081)											// P81

	// PRG
	if(extension == "prg") {
		// try instantiating as a ROM; failing that accept as a tape
		try {
			insert<Cartridge::PRG>(result.cartridges, potential_platforms, TargetPlatform::Commodore, file_name);
		} catch(...) {
			try {
				insert<Tape::PRG>(result.tapes, potential_platforms, TargetPlatform::Commodore, file_name);
			} catch(...) {}
		}
	}

	Format(	"rom",
			result.cartridges,
			Cartridge::BinaryDump,
			TargetPlatform::AcornElectron | TargetPlatform::Coleco | TargetPlatform::MSX)						// ROM
	Format("sg", result.cartridges, Cartridge::BinaryDump, TargetPlatform::Sega)								// SG
	Format("sms", result.cartridges, Cartridge::BinaryDump, TargetPlatform::Sega)								// SMS
	Format("ssd", result.disks, Disk::DiskImageHolder<Disk::SSD>, TargetPlatform::Acorn)				// SSD
	Format("st", result.disks, Disk::DiskImageHolder<Disk::FAT12>, TargetPlatform::AtariST)			// ST
	Format("stx", result.disks, Disk::DiskImageHolder<Disk::STX>, TargetPlatform::AtariST)				// STX
	Format("tap", result.tapes, Tape::CommodoreTAP, TargetPlatform::Commodore)									// TAP (Commodore)
	Format("tap", result.tapes, Tape::OricTAP, TargetPlatform::Oric)											// TAP (Oric)
	Format("tap", result.tapes, Tape::ZXSpectrumTAP, TargetPlatform::ZXSpectrum)								// TAP (ZX Spectrum)
	Format("tsx", result.tapes, Tape::TZX, TargetPlatform::MSX)													// TSX
	Format("tzx", result.tapes, Tape::TZX, TargetPlatform::ZX8081 | TargetPlatform::ZXSpectrum)					// TZX
	Format("uef", result.tapes, Tape::UEF, TargetPlatform::Acorn)												// UEF (tape)
	Format("woz", result.disks, Disk::DiskImageHolder<Disk::WOZ>, TargetPlatform::DiskII)				// WOZ

#undef Format

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
#define Format(ext, class)											\
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
	Append(Amiga);
	Append(Atari2600);
	Append(AtariST);
	Append(Coleco);
	Append(Commodore);
	Append(DiskII);
	Append(Enterprise);
	Append(FAT12);
	Append(Macintosh);
	Append(MSX);
	Append(Oric);
	Append(PCCompatible);
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
