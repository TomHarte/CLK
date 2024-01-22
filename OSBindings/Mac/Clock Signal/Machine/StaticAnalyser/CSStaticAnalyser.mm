//
//  CSStaticAnalyser.m
//  Clock Signal
//
//  Created by Thomas Harte on 31/08/2016.
//  Copyright 2016 Thomas Harte. All rights reserved.
//

#import "CSStaticAnalyser.h"

#import "CSMachine.h"
#import "CSMachine+Target.h"

#include "StaticAnalyser.hpp"

#include "../../../../../Analyser/Static/Acorn/Target.hpp"
#include "../../../../../Analyser/Static/Amiga/Target.hpp"
#include "../../../../../Analyser/Static/AmstradCPC/Target.hpp"
#include "../../../../../Analyser/Static/AppleII/Target.hpp"
#include "../../../../../Analyser/Static/AppleIIgs/Target.hpp"
#include "../../../../../Analyser/Static/AtariST/Target.hpp"
#include "../../../../../Analyser/Static/Commodore/Target.hpp"
#include "../../../../../Analyser/Static/Enterprise/Target.hpp"
#include "../../../../../Analyser/Static/Macintosh/Target.hpp"
#include "../../../../../Analyser/Static/MSX/Target.hpp"
#include "../../../../../Analyser/Static/Oric/Target.hpp"
#include "../../../../../Analyser/Static/PCCompatible/Target.hpp"
#include "../../../../../Analyser/Static/ZX8081/Target.hpp"
#include "../../../../../Analyser/Static/ZXSpectrum/Target.hpp"

#import "Clock_Signal-Swift.h"

@implementation CSStaticAnalyser {
	Analyser::Static::TargetList _targets;
}

// MARK: - File-based Initialiser

- (instancetype)initWithFileAtURL:(NSURL *)url {
	self = [super init];
	if(self) {
		_targets = Analyser::Static::GetTargets([url fileSystemRepresentation]);
		if(!_targets.size()) return nil;

		// TODO: could this better be supplied by the analyser? A hypothetical file format might
		// provide a better name for it contents than the file name?
		_displayName = [url.lastPathComponent copy];
	}
	return self;
}

// MARK: - Machine-based Initialisers

- (instancetype)initWithAmigaModel:(CSMachineAmigaModel)model chipMemorySize:(Kilobytes)chipMemorySize fastMemorySize:(Kilobytes)fastMemorySize {
	self = [super init];
	if(self) {
		using Target = Analyser::Static::Amiga::Target;
		auto target = std::make_unique<Target>();

		switch(chipMemorySize) {
			default: return nil;
			case 512:	target->chip_ram = Target::ChipRAM::FiveHundredAndTwelveKilobytes;	break;
			case 1024:	target->chip_ram = Target::ChipRAM::OneMegabyte;					break;
			case 2048:	target->chip_ram = Target::ChipRAM::TwoMegabytes;					break;
		}

		switch(fastMemorySize) {
			default: return nil;
			case 0:		target->fast_ram = Target::FastRAM::None;			break;
			case 1024:	target->fast_ram = Target::FastRAM::OneMegabyte;	break;
			case 2048:	target->fast_ram = Target::FastRAM::TwoMegabytes;	break;
			case 4096:	target->fast_ram = Target::FastRAM::FourMegabytes;	break;
			case 8192:	target->fast_ram = Target::FastRAM::EightMegabytes;	break;
		}

		_targets.push_back(std::move(target));
	}
	return self;
}

- (instancetype)initWithAmstradCPCModel:(CSMachineCPCModel)model {
	self = [super init];
	if(self) {
		using Target = Analyser::Static::AmstradCPC::Target;
		auto target = std::make_unique<Target>();
		switch(model) {
			case CSMachineCPCModel464:	target->model = Target::Model::CPC464;	break;
			case CSMachineCPCModel664:	target->model = Target::Model::CPC664;	break;
			case CSMachineCPCModel6128: target->model = Target::Model::CPC6128;	break;
		}
		_targets.push_back(std::move(target));
	}
	return self;
}

- (instancetype)initWithAppleIIModel:(CSMachineAppleIIModel)model diskController:(CSMachineAppleIIDiskController)diskController {
	self = [super init];
	if(self) {
		using Target = Analyser::Static::AppleII::Target;
		auto target = std::make_unique<Target>();
		switch(model) {
			default:									target->model = Target::Model::II;				break;
			case CSMachineAppleIIModelAppleIIPlus:		target->model = Target::Model::IIplus;			break;
			case CSMachineAppleIIModelAppleIIe:			target->model = Target::Model::IIe;				break;
			case CSMachineAppleIIModelAppleEnhancedIIe:	target->model = Target::Model::EnhancedIIe;		break;
		}
		switch(diskController) {
			default:
			case CSMachineAppleIIDiskControllerNone:			target->disk_controller = Target::DiskController::None;				break;
			case CSMachineAppleIIDiskControllerSixteenSector:	target->disk_controller = Target::DiskController::SixteenSector;	break;
			case CSMachineAppleIIDiskControllerThirteenSector:	target->disk_controller = Target::DiskController::ThirteenSector;	break;
		}
		_targets.push_back(std::move(target));
	}
	return self;
}

- (instancetype)initWithAppleIIgsModel:(CSMachineAppleIIgsModel)model memorySize:(Kilobytes)memorySize {
	self = [super init];
	if(self) {
		using Target = Analyser::Static::AppleIIgs::Target;
		auto target = std::make_unique<Target>();
		switch(model) {
			default:								target->model = Target::Model::ROM00;			break;
			case CSMachineAppleIIgsModelROM01:		target->model = Target::Model::ROM01;			break;
			case CSMachineAppleIIgsModelROM03:		target->model = Target::Model::ROM03;			break;
		}
		switch(memorySize) {
			default:			target->memory_model = Target::MemoryModel::EightMB;					break;
			case 1024:			target->memory_model = Target::MemoryModel::OneMB;						break;
			case 256:			target->memory_model = Target::MemoryModel::TwoHundredAndFiftySixKB;	break;
		}
		_targets.push_back(std::move(target));
	}
	return self;
}


- (instancetype)initWithAtariSTModel:(CSMachineAtariSTModel)model memorySize:(Kilobytes)memorySize {
	self = [super init];
	if(self) {
		using Target = Analyser::Static::AtariST::Target;
		auto target = std::make_unique<Target>();
		switch(memorySize) {
			default:			target->memory_size = Target::MemorySize::FiveHundredAndTwelveKilobytes;	break;
			case 1024:			target->memory_size = Target::MemorySize::OneMegabyte;						break;
			case 4096:			target->memory_size = Target::MemorySize::FourMegabytes;					break;
		}
		_targets.push_back(std::move(target));
	}
	return self;
}

- (instancetype)initWithElectronDFS:(BOOL)dfs adfs:(BOOL)adfs ap6:(BOOL)ap6 sidewaysRAM:(BOOL)sidewaysRAM {
	self = [super init];
	if(self) {
		using Target = Analyser::Static::Acorn::Target;
		auto target = std::make_unique<Target>();
		target->has_dfs = dfs;
		target->has_pres_adfs = adfs;
		target->has_ap6_rom = ap6;
		target->has_sideways_ram = sidewaysRAM;
		_targets.push_back(std::move(target));
	}
	return self;
}

- (instancetype)initWithEnterpriseModel:(CSMachineEnterpriseModel)model speed:(CSMachineEnterpriseSpeed)speed exosVersion:(CSMachineEnterpriseEXOS)exosVersion basicVersion:(CSMachineEnterpriseBASIC)basicVersion dos:(CSMachineEnterpriseDOS)dos {
	self = [super init];
	if(self) {
		using Target = Analyser::Static::Enterprise::Target;
		auto target = std::make_unique<Target>();

		switch(model) {
			case CSMachineEnterpriseModel64:		target->model = Target::Model::Enterprise64;		break;
			default:
			case CSMachineEnterpriseModel128:		target->model = Target::Model::Enterprise128;		break;
			case CSMachineEnterpriseModel256:		target->model = Target::Model::Enterprise256;		break;
		}

		switch(speed) {
			case CSMachineEnterpriseSpeed6MHz:		target->speed = Target::Speed::SixMHz;				break;
			default:
			case CSMachineEnterpriseSpeed4MHz:		target->speed = Target::Speed::FourMHz;				break;
		}

		switch(exosVersion) {
			case CSMachineEnterpriseEXOSVersion21:	target->exos_version = Target::EXOSVersion::v21;	break;
			default:
			case CSMachineEnterpriseEXOSVersion20:	target->exos_version = Target::EXOSVersion::v20;	break;
			case CSMachineEnterpriseEXOSVersion10:	target->exos_version = Target::EXOSVersion::v10;	break;
		}

		switch(basicVersion) {
			case CSMachineEnterpriseBASICNone:		target->basic_version = Target::BASICVersion::None;	break;
			default:
			case CSMachineEnterpriseBASICVersion21:	target->basic_version = Target::BASICVersion::v21;	break;
			case CSMachineEnterpriseBASICVersion11:	target->basic_version = Target::BASICVersion::v11;	break;
			case CSMachineEnterpriseBASICVersion10:	target->basic_version = Target::BASICVersion::v10;	break;
		}

		switch(dos) {
			case CSMachineEnterpriseDOSEXDOS:		target->dos = Target::DOS::EXDOS;					break;
			default:
			case CSMachineEnterpriseDOSNone:		target->dos = Target::DOS::None;					break;
		}

		_targets.push_back(std::move(target));
	}
	return self;
}

- (instancetype)initWithMacintoshModel:(CSMachineMacintoshModel)model {
	self = [super init];
	if(self) {
		using Target = Analyser::Static::Macintosh::Target;
		auto target = std::make_unique<Target>();

		using Model = Target::Model;
		switch(model) {
			default:
			case CSMachineMacintoshModel128k:	target->model = Model::Mac128k;		break;
			case CSMachineMacintoshModel512k:	target->model = Model::Mac512k;		break;
			case CSMachineMacintoshModel512ke:	target->model = Model::Mac512ke;	break;
			case CSMachineMacintoshModelPlus:	target->model = Model::MacPlus;		break;
		}

		_targets.push_back(std::move(target));
	}
	return self;
}

- (instancetype)initWithMSXModel:(CSMachineMSXModel)model region:(CSMachineMSXRegion)region hasDiskDrive:(BOOL)hasDiskDrive hasMSXMUSIC:(BOOL)hasMSXMUSIC {
	self = [super init];
	if(self) {
		using Target = Analyser::Static::MSX::Target;
		auto target = std::make_unique<Target>();
		target->has_disk_drive = hasDiskDrive;
		target->has_msx_music = hasMSXMUSIC;
		switch(region) {
			case CSMachineMSXRegionAmerican:	target->region = Target::Region::USA;		break;
			case CSMachineMSXRegionEuropean:	target->region = Target::Region::Europe;	break;
			case CSMachineMSXRegionJapanese:	target->region = Target::Region::Japan;		break;
		}
		switch(model) {
			case CSMachineMSXModelMSX1:			target->model = Target::Model::MSX1;		break;
			case CSMachineMSXModelMSX2:			target->model = Target::Model::MSX2;		break;
		}
		_targets.push_back(std::move(target));
	}
	return self;
}

- (instancetype)initWithOricModel:(CSMachineOricModel)model diskInterface:(CSMachineOricDiskInterface)diskInterface {
	self = [super init];
	if(self) {
		using Target = Analyser::Static::Oric::Target;
		auto target = std::make_unique<Target>();
		switch(model) {
			case CSMachineOricModelOric1:		target->rom = Target::ROM::BASIC10;	break;
			case CSMachineOricModelOricAtmos:	target->rom = Target::ROM::BASIC11;	break;
			case CSMachineOricModelPravetz:		target->rom = Target::ROM::Pravetz;	break;
		}
		switch(diskInterface) {
			case CSMachineOricDiskInterfaceNone:		target->disk_interface = Target::DiskInterface::None;		break;
			case CSMachineOricDiskInterfaceMicrodisc:	target->disk_interface = Target::DiskInterface::Microdisc;	break;
			case CSMachineOricDiskInterfacePravetz:		target->disk_interface = Target::DiskInterface::Pravetz;	break;
			case CSMachineOricDiskInterfaceJasmin:		target->disk_interface = Target::DiskInterface::Jasmin;		break;
			case CSMachineOricDiskInterfaceBD500:		target->disk_interface = Target::DiskInterface::BD500;		break;
		}
		_targets.push_back(std::move(target));
	}
	return self;
}

- (instancetype)initWithPCCompatibleSpeed:(CSPCCompatibleSpeed)speed videoAdaptor:(CSPCCompatibleVideoAdaptor)adaptor {
	self = [super init];
	if(self) {
		using Target = Analyser::Static::PCCompatible::Target;
		auto target = std::make_unique<Target>();
		switch(adaptor) {
			case CSPCCompatibleVideoAdaptorMDA:	target->adaptor = Target::VideoAdaptor::MDA;	break;
			case CSPCCompatibleVideoAdaptorCGA:	target->adaptor = Target::VideoAdaptor::CGA;	break;
		}
		switch(speed) {
			case CSPCCompatibleSpeedOriginal:	target->speed = Target::Speed::ApproximatelyOriginal;	break;
			case CSPCCompatibleSpeedTurbo:		target->speed = Target::Speed::Fast;					break;
		}
		_targets.push_back(std::move(target));
	}
	return self;
}

- (instancetype)initWithSpectrumModel:(CSMachineSpectrumModel)model {
	self = [super init];
	if(self) {
		using Target = Analyser::Static::ZXSpectrum::Target;
		auto target = std::make_unique<Target>();
		switch(model) {
			case CSMachineSpectrumModelSixteenK:		target->model = Target::Model::SixteenK;		break;
			case CSMachineSpectrumModelFortyEightK:		target->model = Target::Model::FortyEightK;		break;
			case CSMachineSpectrumModelOneTwoEightK:	target->model = Target::Model::OneTwoEightK;	break;
			case CSMachineSpectrumModelPlus2:			target->model = Target::Model::Plus2;			break;
			case CSMachineSpectrumModelPlus2a:			target->model = Target::Model::Plus2a;			break;
			case CSMachineSpectrumModelPlus3:			target->model = Target::Model::Plus3;			break;
		}
		_targets.push_back(std::move(target));
	}
	return self;
}

- (instancetype)initWithVic20Region:(CSMachineVic20Region)region memorySize:(Kilobytes)memorySize hasC1540:(BOOL)hasC1540 {
	self = [super init];
	if(self) {
		using Target = Analyser::Static::Commodore::Target;
		auto target = std::make_unique<Target>();
		switch(region) {
			case CSMachineVic20RegionDanish:	target->region = Target::Region::Danish;	break;
			case CSMachineVic20RegionSwedish:	target->region = Target::Region::Swedish;	break;
			case CSMachineVic20RegionAmerican:	target->region = Target::Region::American;	break;
			case CSMachineVic20RegionEuropean:	target->region = Target::Region::European;	break;
			case CSMachineVic20RegionJapanese:	target->region = Target::Region::Japanese;	break;
		}
		auto memory_model = Target::MemoryModel::Unexpanded;
		switch(memorySize) {
			default:	break;
			case 8:		memory_model = Target::MemoryModel::EightKB;		break;
			case 32:	memory_model = Target::MemoryModel::ThirtyTwoKB;	break;
		}
		target->set_memory_model(memory_model);
		target->has_c1540 = !!hasC1540;
		_targets.push_back(std::move(target));
	}
	return self;
}

static Analyser::Static::ZX8081::Target::MemoryModel ZX8081MemoryModelFromSize(Kilobytes size) {
	using MemoryModel = Analyser::Static::ZX8081::Target::MemoryModel;
	switch(size) {
		default:	return MemoryModel::Unexpanded;
		case 16:	return MemoryModel::SixteenKB;
		case 64:	return MemoryModel::SixtyFourKB;
	}
}

- (instancetype)initWithZX80MemorySize:(Kilobytes)memorySize useZX81ROM:(BOOL)useZX81ROM {
	self = [super init];
	if(self) {
		using Target = Analyser::Static::ZX8081::Target;
		auto target = std::make_unique<Target>();
		target->is_ZX81 = false;
		target->ZX80_uses_ZX81_ROM = !!useZX81ROM;
		target->memory_model = ZX8081MemoryModelFromSize(memorySize);
		_targets.push_back(std::move(target));
	}
	return self;
}

- (instancetype)initWithZX81MemorySize:(Kilobytes)memorySize {
	self = [super init];
	if(self) {
		using Target = Analyser::Static::ZX8081::Target;
		auto target = std::make_unique<Target>();
		target->is_ZX81 = true;
		target->memory_model = ZX8081MemoryModelFromSize(memorySize);
		_targets.push_back(std::move(target));
	}
	return self;
}

// MARK: - NIB mapping

- (NSString *)optionsNibName {
	switch(_targets.front()->machine) {
//		case Analyser::Machine::AmstradCPC:		return @"QuickLoadCompositeOptions";
		case Analyser::Machine::AmstradCPC:		return @"CompositeOptions";
		case Analyser::Machine::AppleII:		return @"AppleIIOptions";
		case Analyser::Machine::Atari2600:		return @"Atari2600Options";
		case Analyser::Machine::AtariST:		return @"CompositeOptions";
		case Analyser::Machine::ColecoVision:	return @"CompositeOptions";
		case Analyser::Machine::Electron:		return @"QuickLoadCompositeOptions";
		case Analyser::Machine::Enterprise:		return @"CompositeOptions";
		case Analyser::Machine::Macintosh:		return @"MacintoshOptions";
		case Analyser::Machine::MasterSystem:	return @"CompositeOptions";
		case Analyser::Machine::MSX:			return @"QuickLoadCompositeOptions";
		case Analyser::Machine::Oric:			return @"OricOptions";
		case Analyser::Machine::PCCompatible:	return @"CompositeOptions";
		case Analyser::Machine::Vic20:			return @"QuickLoadCompositeOptions";
		case Analyser::Machine::ZX8081:			return @"ZX8081Options";
		case Analyser::Machine::ZXSpectrum:		return @"QuickLoadCompositeOptions"; // TODO: @"ZXSpectrumOptions";
		default: return nil;
	}
}

- (Analyser::Static::TargetList &)targets {
	return _targets;
}

@end

@implementation CSMediaSet {
	Analyser::Static::Media _media;
}

- (instancetype)initWithFileAtURL:(NSURL *)url {
	self = [super init];
	if(self) {
		_media = Analyser::Static::GetMedia([url fileSystemRepresentation]);
	}
	return self;
}

- (void)applyToMachine:(CSMachine *)machine {
	[machine applyMedia:_media];
}

- (BOOL)empty {
	return _media.empty();
}

@end
