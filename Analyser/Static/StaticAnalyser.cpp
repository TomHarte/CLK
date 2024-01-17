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

class MediaAccumulator {
	public:
	MediaAccumulator(const std::string &file_name, TargetPlatform::IntType &potential_platforms) :
		file_name_(file_name), potential_platforms_(potential_platforms), extension_(get_extension(file_name)) {}

	/// Adds @c instance to the media collection and adds @c platforms to the set of potentials.
	/// If @c instance is an @c TargetPlatform::TypeDistinguisher then it is given an opportunity to restrict the set of potentials.
	template <typename InstanceT>
	void insert(TargetPlatform::IntType platforms, InstanceT *instance) {
		if constexpr (std::is_base_of_v<Storage::Disk::Disk, InstanceT>) {
			media.disks.emplace_back(instance);
		} else if constexpr (std::is_base_of_v<Storage::Tape::Tape, InstanceT>) {
			media.tapes.emplace_back(instance);
		} else if constexpr (std::is_base_of_v<Storage::Cartridge::Cartridge, InstanceT>) {
			media.cartridges.emplace_back(instance);
		} else if constexpr (std::is_base_of_v<Storage::MassStorage::MassStorageDevice, InstanceT>) {
			media.mass_storage_devices.emplace_back(instance);
		} else {
			static_assert(always_false_v<InstanceT>, "Unexpected type encountered.");
		}

		potential_platforms_ |= platforms;

		// Check whether the instance itself has any input on target platforms.
		TargetPlatform::TypeDistinguisher *const distinguisher =
			dynamic_cast<TargetPlatform::TypeDistinguisher *>(instance);
		if(distinguisher) potential_platforms_ &= distinguisher->target_platform_type();
	}

	/// Concstructs a new instance of @c InstanceT supplying @c args and adds it to the back of @c list using @c insert_instance.
	template <typename InstanceT, typename... Args>
	void insert(TargetPlatform::IntType platforms, Args &&... args) {
		insert(platforms, new InstanceT(std::forward<Args>(args)...));
	}

	/// Calls @c insert with the specified parameters, ignoring any exceptions thrown.
	template <typename InstanceT, typename... Args>
	void try_insert(TargetPlatform::IntType platforms, Args &&... args) {
		try {
			insert<InstanceT>(platforms, std::forward<Args>(args)...);
		} catch(...) {}
	}

	/// Performs a @c try_insert for an object of @c InstanceT if @c extension matches that of the file name,
	/// providing the file name as the only construction argument.
	template <typename InstanceT>
	void try_standard(TargetPlatform::IntType platforms, const char *extension) {
		if(name_matches(extension))	{
			try_insert<InstanceT>(platforms, file_name_);
		}
	}

	bool name_matches(const char *extension) {
		return extension_ == extension;
	}

	Media media;

	private:
		const std::string &file_name_;
		TargetPlatform::IntType &potential_platforms_;
		const std::string extension_;
};

}

static Media GetMediaAndPlatforms(const std::string &file_name, TargetPlatform::IntType &potential_platforms) {
	MediaAccumulator accumulator(file_name, potential_platforms);

	// 2MG
	if(accumulator.name_matches("2mg")) {
		// 2MG uses a factory method; defer to it.
		try {
			const auto media = Disk::Disk2MG::open(file_name);
			std::visit([&](auto &&arg) {
				using Type = typename std::decay<decltype(arg)>::type;

				if constexpr (std::is_same<Type, nullptr_t>::value) {
					// It's valid for no media to be returned.
				} else if constexpr (std::is_same<Type, Disk::DiskImageHolderBase *>::value) {
					accumulator.insert(TargetPlatform::DiskII, arg);
				} else if constexpr (std::is_same<Type, MassStorage::MassStorageDevice *>::value) {
					// TODO: or is it Apple IIgs?
					accumulator.insert(TargetPlatform::AppleII, arg);
				} else {
					static_assert(always_false_v<Type>, "Unexpected type encountered.");
				}
			}, media);
		} catch(...) {}
	}

	accumulator.try_standard<Tape::ZX80O81P>(TargetPlatform::ZX8081, "80");
	accumulator.try_standard<Tape::ZX80O81P>(TargetPlatform::ZX8081, "81");

	accumulator.try_standard<Cartridge::BinaryDump>(TargetPlatform::Atari2600, "a26");
	accumulator.try_standard<Disk::DiskImageHolder<Disk::AcornADF>>(TargetPlatform::Acorn, "adf");
	accumulator.try_standard<Disk::DiskImageHolder<Disk::AmigaADF>>(TargetPlatform::Amiga, "adf");
	accumulator.try_standard<Disk::DiskImageHolder<Disk::AcornADF>>(TargetPlatform::Acorn, "adl");

	accumulator.try_standard<Cartridge::BinaryDump>(TargetPlatform::AllCartridge, "bin");

	accumulator.try_standard<Tape::CAS>(TargetPlatform::MSX, "cas");
	accumulator.try_standard<Tape::TZX>(TargetPlatform::AmstradCPC, "cdt");
	accumulator.try_standard<Cartridge::BinaryDump>(TargetPlatform::Coleco, "col");
	accumulator.try_standard<Tape::CSW>(TargetPlatform::AllTape, "csw");

	accumulator.try_standard<Disk::DiskImageHolder<Disk::D64>>(TargetPlatform::Commodore, "d64");
	accumulator.try_standard<MassStorage::DAT>(TargetPlatform::Acorn, "dat");
	accumulator.try_standard<Disk::DiskImageHolder<Disk::DMK>>(TargetPlatform::MSX, "dmk");
	accumulator.try_standard<Disk::DiskImageHolder<Disk::AppleDSK>>(TargetPlatform::DiskII, "do");
	accumulator.try_standard<Disk::DiskImageHolder<Disk::SSD>>(TargetPlatform::Acorn, "dsd");
	accumulator.try_standard<Disk::DiskImageHolder<Disk::CPCDSK>>(
		TargetPlatform::AmstradCPC | TargetPlatform::Oric | TargetPlatform::ZXSpectrum, "dsk");
	accumulator.try_standard<Disk::DiskImageHolder<Disk::AppleDSK>>(TargetPlatform::DiskII, "dsk");
	accumulator.try_standard<Disk::DiskImageHolder<Disk::MacintoshIMG>>(TargetPlatform::Macintosh, "dsk");
	accumulator.try_standard<MassStorage::HFV>(TargetPlatform::Macintosh, "dsk");
	accumulator.try_standard<MassStorage::DSK>(TargetPlatform::Macintosh, "dsk");
	accumulator.try_standard<Disk::DiskImageHolder<Disk::FAT12>>(TargetPlatform::MSX, "dsk");
	accumulator.try_standard<Disk::DiskImageHolder<Disk::OricMFMDSK>>(TargetPlatform::Oric, "dsk");

	accumulator.try_standard<Disk::DiskImageHolder<Disk::G64>>(TargetPlatform::Commodore, "g64");

	accumulator.try_standard<MassStorage::HDV>(TargetPlatform::AppleII, "hdv");
	accumulator.try_standard<Disk::DiskImageHolder<Disk::HFE>>(
		TargetPlatform::Acorn | TargetPlatform::AmstradCPC | TargetPlatform::Commodore | TargetPlatform::Oric | TargetPlatform::ZXSpectrum,
		"hfe");	// TODO: switch to AllDisk once the MSX stops being so greedy.

	accumulator.try_standard<Disk::DiskImageHolder<Disk::FAT12>>(TargetPlatform::PCCompatible, "ima");
	accumulator.try_standard<Disk::DiskImageHolder<Disk::MacintoshIMG>>(TargetPlatform::Macintosh, "image");
	accumulator.try_standard<Disk::DiskImageHolder<Disk::IMD>>(TargetPlatform::PCCompatible, "imd");
	accumulator.try_standard<Disk::DiskImageHolder<Disk::MacintoshIMG>>(TargetPlatform::Macintosh, "img");

	// Treat PC booter as a potential backup only if this doesn't parse as a FAT12.
	if(accumulator.name_matches("img")) {
		try {
			accumulator.insert<Disk::DiskImageHolder<Disk::FAT12>>(TargetPlatform::FAT12, file_name);
		} catch(...) {
			accumulator.try_standard<Disk::DiskImageHolder<Disk::PCBooter>>(TargetPlatform::PCCompatible, "img");
		}
	}

	accumulator.try_standard<Disk::DiskImageHolder<Disk::IPF>>(
		TargetPlatform::Amiga | TargetPlatform::AtariST | TargetPlatform::AmstradCPC | TargetPlatform::ZXSpectrum,
		"ipf");

	accumulator.try_standard<Disk::DiskImageHolder<Disk::MSA>>(TargetPlatform::AtariST, "msa");
	accumulator.try_standard<Cartridge::BinaryDump>(TargetPlatform::MSX, "mx2");
	accumulator.try_standard<Disk::DiskImageHolder<Disk::NIB>>(TargetPlatform::DiskII, "nib");

	accumulator.try_standard<Tape::ZX80O81P>(TargetPlatform::ZX8081, "o");
	accumulator.try_standard<Tape::ZX80O81P>(TargetPlatform::ZX8081, "p");
	accumulator.try_standard<Disk::DiskImageHolder<Disk::AppleDSK>>(TargetPlatform::DiskII, "po");

	if(accumulator.name_matches("po"))	{
		accumulator.try_insert<Disk::DiskImageHolder<Disk::MacintoshIMG>>(
			TargetPlatform::AppleIIgs,
			file_name, Disk::MacintoshIMG::FixedType::GCR);
	}

	accumulator.try_standard<Tape::ZX80O81P>(TargetPlatform::ZX8081, "p81");

	if(accumulator.name_matches("prg")) {
		// Try instantiating as a ROM; failing that accept as a tape.
		try {
			accumulator.insert<Cartridge::PRG>(TargetPlatform::Commodore, file_name);
		} catch(...) {
			try {
				accumulator.insert<Tape::PRG>(TargetPlatform::Commodore, file_name);
			} catch(...) {}
		}
	}

	accumulator.try_standard<Cartridge::BinaryDump>(
		TargetPlatform::AcornElectron | TargetPlatform::Coleco | TargetPlatform::MSX,
		"rom");

	accumulator.try_standard<Cartridge::BinaryDump>(TargetPlatform::Sega, "sg");
	accumulator.try_standard<Cartridge::BinaryDump>(TargetPlatform::Sega, "sms");
	accumulator.try_standard<Disk::DiskImageHolder<Disk::SSD>>(TargetPlatform::Acorn, "ssd");
	accumulator.try_standard<Disk::DiskImageHolder<Disk::FAT12>>(TargetPlatform::AtariST, "st");
	accumulator.try_standard<Disk::DiskImageHolder<Disk::STX>>(TargetPlatform::AtariST, "stx");

	accumulator.try_standard<Tape::CommodoreTAP>(TargetPlatform::Commodore, "tap");
	accumulator.try_standard<Tape::OricTAP>(TargetPlatform::Oric, "tap");
	accumulator.try_standard<Tape::ZXSpectrumTAP>(TargetPlatform::ZXSpectrum, "tap");
	accumulator.try_standard<Tape::TZX>(TargetPlatform::MSX, "tsx");
	accumulator.try_standard<Tape::TZX>(TargetPlatform::ZX8081 | TargetPlatform::ZXSpectrum, "tzx");

	accumulator.try_standard<Tape::UEF>(TargetPlatform::Acorn, "uef");

	accumulator.try_standard<Disk::DiskImageHolder<Disk::WOZ>>(TargetPlatform::DiskII, "woz");

	return accumulator.media;
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
