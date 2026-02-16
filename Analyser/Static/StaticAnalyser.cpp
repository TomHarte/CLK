//
//  StaticAnalyser.cpp
//  Clock Signal
//
//  Created by Thomas Harte on 23/08/2016.
//  Copyright 2016 Thomas Harte. All rights reserved.
//

#include "StaticAnalyser.hpp"

#include <algorithm>
#include <bit>
#include <cstddef>
#include <cstdlib>
#include <cstring>
#include <iterator>

// Analysers
#include "Analyser/Static/Acorn/StaticAnalyser.hpp"
#include "Analyser/Static/Amiga/StaticAnalyser.hpp"
#include "Analyser/Static/AmstradCPC/StaticAnalyser.hpp"
#include "Analyser/Static/AppleII/StaticAnalyser.hpp"
#include "Analyser/Static/AppleIIgs/StaticAnalyser.hpp"
#include "Analyser/Static/Atari2600/StaticAnalyser.hpp"
#include "Analyser/Static/AtariST/StaticAnalyser.hpp"
#include "Analyser/Static/Coleco/StaticAnalyser.hpp"
#include "Analyser/Static/Commodore/StaticAnalyser.hpp"
#include "Analyser/Static/DiskII/StaticAnalyser.hpp"
#include "Analyser/Static/Enterprise/StaticAnalyser.hpp"
#include "Analyser/Static/FAT12/StaticAnalyser.hpp"
#include "Analyser/Static/Macintosh/StaticAnalyser.hpp"
#include "Analyser/Static/MSX/StaticAnalyser.hpp"
#include "Analyser/Static/Oric/StaticAnalyser.hpp"
#include "Analyser/Static/PCCompatible/StaticAnalyser.hpp"
#include "Analyser/Static/Sega/StaticAnalyser.hpp"
#include "Analyser/Static/ZX8081/StaticAnalyser.hpp"
#include "Analyser/Static/ZXSpectrum/StaticAnalyser.hpp"

// Cartridges
#include "Storage/Cartridge/Formats/BinaryDump.hpp"
#include "Storage/Cartridge/Formats/PRG.hpp"

// Disks
#include "Storage/Disk/DiskImage/Formats/2MG.hpp"
#include "Storage/Disk/DiskImage/Formats/AcornADF.hpp"
#include "Storage/Disk/DiskImage/Formats/AmigaADF.hpp"
#include "Storage/Disk/DiskImage/Formats/AppleDSK.hpp"
#include "Storage/Disk/DiskImage/Formats/CPCDSK.hpp"
#include "Storage/Disk/DiskImage/Formats/D64.hpp"
#include "Storage/Disk/DiskImage/Formats/G64.hpp"
#include "Storage/Disk/DiskImage/Formats/DMK.hpp"
#include "Storage/Disk/DiskImage/Formats/FAT12.hpp"
#include "Storage/Disk/DiskImage/Formats/HFE.hpp"
#include "Storage/Disk/DiskImage/Formats/IPF.hpp"
#include "Storage/Disk/DiskImage/Formats/IMD.hpp"
#include "Storage/Disk/DiskImage/Formats/JFD.hpp"
#include "Storage/Disk/DiskImage/Formats/MacintoshIMG.hpp"
#include "Storage/Disk/DiskImage/Formats/MOOF.hpp"
#include "Storage/Disk/DiskImage/Formats/MSA.hpp"
#include "Storage/Disk/DiskImage/Formats/NIB.hpp"
#include "Storage/Disk/DiskImage/Formats/OricMFMDSK.hpp"
#include "Storage/Disk/DiskImage/Formats/PCBooter.hpp"
#include "Storage/Disk/DiskImage/Formats/SSD.hpp"
#include "Storage/Disk/DiskImage/Formats/STX.hpp"
#include "Storage/Disk/DiskImage/Formats/WOZ.hpp"

// File Bundles.
#include "Storage/FileBundle/FileBundle.hpp"

// Mass Storage Devices (i.e. usually, hard disks)
#include "Storage/MassStorage/Formats/DAT.hpp"
#include "Storage/MassStorage/Formats/DSK.hpp"
#include "Storage/MassStorage/Formats/HDV.hpp"
#include "Storage/MassStorage/Formats/HFV.hpp"
#include "Storage/MassStorage/Formats/VHD.hpp"

// State Snapshots
#include "Storage/State/SNA.hpp"
#include "Storage/State/SZX.hpp"
#include "Storage/State/Z80.hpp"

// Tapes
#include "Storage/Tape/Formats/CAS.hpp"
#include "Storage/Tape/Formats/CommodoreTAP.hpp"
#include "Storage/Tape/Formats/CSW.hpp"
#include "Storage/Tape/Formats/OricTAP.hpp"
#include "Storage/Tape/Formats/TapePRG.hpp"
#include "Storage/Tape/Formats/TapeUEF.hpp"
#include "Storage/Tape/Formats/TZX.hpp"
#include "Storage/Tape/Formats/ZX80O81P.hpp"
#include "Storage/Tape/Formats/ZXSpectrumTAP.hpp"

// Target Platform Types
#include "Storage/TargetPlatforms.hpp"

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
	void insert(const TargetPlatform::IntType platforms, std::shared_ptr<InstanceT> instance) {
		if constexpr (std::is_base_of_v<Storage::Disk::Disk, InstanceT>) {
			media.disks.push_back(instance);
		} else if constexpr (std::is_base_of_v<Storage::Tape::Tape, InstanceT>) {
			media.tapes.push_back(instance);
		} else if constexpr (std::is_base_of_v<Storage::Cartridge::Cartridge, InstanceT>) {
			media.cartridges.push_back(instance);
		} else if constexpr (std::is_base_of_v<Storage::MassStorage::MassStorageDevice, InstanceT>) {
			media.mass_storage_devices.push_back(instance);
		} else if constexpr (std::is_base_of_v<Storage::FileBundle::FileBundle, InstanceT>) {
			media.file_bundles.push_back(instance);
		} else {
			static_assert(always_false_v<InstanceT>, "Unexpected type encountered.");
		}

		potential_platforms_ |= platforms;

		// Check whether the instance itself has any input on target platforms.
		TargetPlatform::Distinguisher *const distinguisher =
			dynamic_cast<TargetPlatform::Distinguisher *>(instance.get());
		if(distinguisher) {
			was_distinguished = true;
			potential_platforms_ &= distinguisher->target_platforms();
		}
	}

	/// Concstructs a new instance of @c InstanceT supplying @c args and adds it to the back of @c list using @c insert_instance.
	template <typename InstanceT, typename... Args>
	void insert(const TargetPlatform::IntType platforms, Args &&... args) {
		insert(platforms, std::make_shared<InstanceT>(std::forward<Args>(args)...));
	}

	/// Calls @c insert with the specified parameters, ignoring any exceptions thrown.
	template <typename InstanceT, typename... Args>
	void try_insert(const TargetPlatform::IntType platforms, Args &&... args) {
		try {
			insert<InstanceT>(platforms, std::forward<Args>(args)...);
		} catch(...) {}
	}

	/// Performs a @c try_insert for an object of @c InstanceT if @c extension matches that of the file name,
	/// providing the file name as the only construction argument.
	template <typename InstanceT>
	void try_standard(const TargetPlatform::IntType platforms, const char *extension) {
		if(name_matches(extension))	{
			try_insert<InstanceT>(platforms, file_name_);
		}
	}

	bool name_matches(const char *const extension) {
		return extension_ == extension;
	}

	Media media;
	bool was_distinguished = false;

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

				if constexpr (std::is_same<Type, std::nullptr_t>::value) {
					// It's valid for no media to be returned.
				} else if constexpr (std::is_same<Type, Disk::DiskImageHolderBase *>::value) {
					accumulator.insert(TargetPlatform::DiskII, std::shared_ptr<Disk::DiskImageHolderBase>(arg));
				} else if constexpr (std::is_same<Type, MassStorage::MassStorageDevice *>::value) {
					// TODO: or is it Apple IIgs?
					accumulator.insert(TargetPlatform::AppleII, std::shared_ptr<MassStorage::MassStorageDevice>(arg));
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

	accumulator.try_standard<FileBundle::LocalFSFileBundle>(TargetPlatform::Enterprise, "bas");
	accumulator.try_standard<Cartridge::BinaryDump>(TargetPlatform::AllCartridge, "bin");

	accumulator.try_standard<Tape::CAS>(TargetPlatform::MSX, "cas");
	accumulator.try_standard<Tape::TZX>(TargetPlatform::AmstradCPC, "cdt");
	accumulator.try_standard<Cartridge::BinaryDump>(TargetPlatform::Coleco, "col");
	accumulator.try_standard<FileBundle::LocalFSFileBundle>(TargetPlatform::Enterprise, "com");
	accumulator.try_standard<Tape::CSW>(TargetPlatform::AllTape, "csw");

	accumulator.try_standard<Disk::DiskImageHolder<Disk::D64>>(TargetPlatform::Commodore8bit, "d64");
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

	accumulator.try_standard<Disk::DiskImageHolder<Disk::G64>>(TargetPlatform::Commodore8bit, "g64");

	accumulator.try_standard<MassStorage::HDV>(TargetPlatform::AppleII, "hdv");
	accumulator.try_standard<Disk::DiskImageHolder<Disk::HFE>>(
		TargetPlatform::Acorn | TargetPlatform::AmstradCPC | TargetPlatform::Commodore |
		TargetPlatform::Oric | TargetPlatform::ZXSpectrum,
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

	accumulator.try_standard<Disk::DiskImageHolder<Disk::JFD>>(TargetPlatform::Archimedes, "jfd");

	accumulator.try_standard<Disk::DiskImageHolder<Disk::MOOF>>(TargetPlatform::Macintosh, "moof");
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

	static constexpr auto PRGTargets = TargetPlatform::Vic20; //Commodore8bit;	// Disabled until analysis improves.
	if(accumulator.name_matches("prg")) {
		// Try instantiating as a ROM; failing that accept as a tape.
		try {
			accumulator.insert<Cartridge::PRG>(PRGTargets, file_name);
		} catch(...) {
			try {
				accumulator.insert<Tape::PRG>(PRGTargets, file_name);
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

	accumulator.try_standard<Tape::CommodoreTAP>(TargetPlatform::Commodore8bit, "tap");
	accumulator.try_standard<Tape::OricTAP>(TargetPlatform::Oric, "tap");
	accumulator.try_standard<Tape::ZXSpectrumTAP>(TargetPlatform::ZXSpectrum, "tap");
	accumulator.try_standard<Tape::TZX>(TargetPlatform::MSX, "tsx");
	accumulator.try_standard<Tape::TZX>(TargetPlatform::ZX8081 | TargetPlatform::ZXSpectrum, "tzx");

	accumulator.try_standard<Tape::UEF>(TargetPlatform::Acorn, "uef");

	accumulator.try_standard<MassStorage::VHD>(TargetPlatform::PCCompatible, "vhd");

	accumulator.try_standard<Disk::DiskImageHolder<Disk::WOZ>>(TargetPlatform::DiskII, "woz");

	return accumulator.media;
}

Media Analyser::Static::GetMedia(const std::string &file_name) {
	TargetPlatform::IntType throwaway;
	return GetMediaAndPlatforms(file_name, throwaway);
}

TargetList Analyser::Static::GetTargets(const std::string &file_name) {
	const std::string extension = get_extension(file_name);
	TargetList targets;

	// Check whether the file directly identifies a target; if so then just return that.
	const auto try_snapshot = [&](const char *ext, auto loader) -> bool {
		if(extension != ext) {
			return false;
		}
		try {
			auto target = loader(file_name);
			if(target) {
				targets.push_back(std::move(target));
				return true;
			}
		} catch(...) {}

		return false;
	};

	if(try_snapshot("sna", Storage::State::SNA::load)) return targets;
	if(try_snapshot("szx", Storage::State::SZX::load)) return targets;
	if(try_snapshot("z80", Storage::State::Z80::load)) return targets;

	// Otherwise:
	//
	// Collect all disks, tapes ROMs, etc as can be extrapolated from this file, forming the
	// union of all platforms this file might be a target for.
	TargetPlatform::IntType potential_platforms = 0;
	Media media = GetMediaAndPlatforms(file_name, potential_platforms);

	int total_options = std::popcount(potential_platforms);
	const bool is_confident = total_options == 1;
	// i.e. This analyser `is_confident` if file analysis suggested only one potential target platform.
	// The machine-specific static analyser will still run in case it can provide meaningful annotations on
	// loading command, machine configuration, etc, but the flag will be passed onwards to mean "don't reject this".

	// Hand off to platform-specific determination of whether these
	// things are actually compatible and, if so, how to load them.
	const auto append = [&](TargetPlatform::IntType platform, auto evaluator) {
		if(!(potential_platforms & platform)) {
			return;
		}
		auto new_targets = evaluator(media, file_name, potential_platforms, is_confident);
		targets.insert(
			targets.end(),
			std::make_move_iterator(new_targets.begin()),
			std::make_move_iterator(new_targets.end())
		);
	};

	append(TargetPlatform::Acorn, Acorn::GetTargets);
	append(TargetPlatform::AmstradCPC, AmstradCPC::GetTargets);
	append(TargetPlatform::AppleII, AppleII::GetTargets);
	append(TargetPlatform::AppleIIgs, AppleIIgs::GetTargets);
	append(TargetPlatform::Amiga, Amiga::GetTargets);
	append(TargetPlatform::Atari2600, Atari2600::GetTargets);
	append(TargetPlatform::AtariST, AtariST::GetTargets);
	append(TargetPlatform::Coleco, Coleco::GetTargets);
	append(TargetPlatform::Commodore8bit, Commodore::GetTargets);
	append(TargetPlatform::DiskII, DiskII::GetTargets);
	append(TargetPlatform::Enterprise, Enterprise::GetTargets);
	append(TargetPlatform::FAT12, FAT12::GetTargets);
	append(TargetPlatform::Macintosh, Macintosh::GetTargets);
	append(TargetPlatform::MSX, MSX::GetTargets);
	append(TargetPlatform::Oric, Oric::GetTargets);
	append(TargetPlatform::PCCompatible, PCCompatible::GetTargets);
	append(TargetPlatform::Sega, Sega::GetTargets);
	append(TargetPlatform::ZX8081, ZX8081::GetTargets);
	append(TargetPlatform::ZXSpectrum, ZXSpectrum::GetTargets);

	// Sort by initial confidence. Use a stable sort in case any of the machine-specific analysers
	// picked their insertion order carefully.
	std::stable_sort(targets.begin(), targets.end(),
		[] (const std::unique_ptr<Target> &a, const std::unique_ptr<Target> &b) {
			return a->confidence > b->confidence;
		});

	return targets;
}
