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

#include "Analyser/Static/Acorn/Target.hpp"
#include "Analyser/Static/Amiga/Target.hpp"
#include "Analyser/Static/AmstradCPC/Target.hpp"
#include "Analyser/Static/AppleII/Target.hpp"
#include "Analyser/Static/AppleIIgs/Target.hpp"
#include "Analyser/Static/AtariST/Target.hpp"
#include "Analyser/Static/Commodore/Target.hpp"
#include "Analyser/Static/Enterprise/Target.hpp"
#include "Analyser/Static/Macintosh/Target.hpp"
#include "Analyser/Static/MSX/Target.hpp"
#include "Analyser/Static/Oric/Target.hpp"
#include "Analyser/Static/PCCompatible/Target.hpp"
#include "Analyser/Static/ZX8081/Target.hpp"
#include "Analyser/Static/ZXSpectrum/Target.hpp"

#import "Clock_Signal-Swift.h"

@implementation CSMediaSet {
	Analyser::Static::Media _media;
}

- (instancetype)initWithMedia:(Analyser::Static::Media)media {
	self = [super init];
	if(self) {
		_media = media;
	}
	return self;
}

- (instancetype)initWithFileAtURL:(NSURL *)url {
	self = [super init];
	if(self) {
		_media = Analyser::Static::GetMedia([url fileSystemRepresentation]);
	}
	return self;
}

- (BOOL)empty {
	return _media.empty();
}

- (void)applyToMachine:(CSMachine *)machine {
	[machine applyMedia:_media];
}

- (void)obtainPermissions {
	for(const auto &bundle: _media.file_bundles) {
		// (1) Does this file bundle have a base path?
		const auto path = bundle->base_path();
		if(!path.has_value()) {
			continue;
		}
		NSString *pathName = [NSString stringWithUTF8String:path->c_str()];

		// (2) Can everything in that base path already be freely read?
		NSError *error;
		NSArray<NSString *> *allFiles = [[NSFileManager defaultManager] contentsOfDirectoryAtPath:pathName error:&error];
		BOOL hasFullAccess = YES;
		for(NSString *file in allFiles) {
			FILE *pilot = fopen([[pathName stringByAppendingPathComponent:file] UTF8String], "rb");
			if(!pilot) {
				hasFullAccess = NO;
				break;
			}
			fclose(pilot);
		}
		if(hasFullAccess) {
			continue;
		}

		// (3) Ask the user for permission.
		NSOpenPanel *request = [NSOpenPanel openPanel];
		request.prompt = @"Grant Permission";
		request.message = @"Please Grant Permission For Full Folder Access";
		request.canChooseFiles = NO;
		request.allowsMultipleSelection = NO;
		request.canChooseDirectories = YES;
		[request setDirectoryURL:[NSURL fileURLWithPath:pathName isDirectory:YES]];

		request.accessoryView = [NSTextField labelWithString:
			@"Clock Signal is sandboxed; it cannot access any of your files without explicit permission.\n"
			@"The type of program you are loading might require access to other files in its directory, which this "
			@"application does not currently have permission to do.\n"
			@"Please select 'Grant Permission' to give it permission to do so."
		];
		request.accessoryViewDisclosed = YES;
		// TODO: use delegate further to shepherd user.

		const auto response = [request runModal];
		if(response != NSModalResponseOK) {
			continue;
		}

		// Possibly substitute the base path, in case the one returned
		// is an indirection out of the sandbox.
		if(![request.URL isEqual:[NSURL fileURLWithPath:pathName]]) {
			NSLog(@"Need to substitute: %@", request.URL);
		}

		// TODO: bookmarkDataWithOptions on the URL, and store that somewhere for
		// later retrieval. Then try that again if the same directory presents itself.
	}
}

@end

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

- (instancetype)initWithAppleIIModel:(CSMachineAppleIIModel)model diskController:(CSMachineAppleIIDiskController)diskController hasMockingboard:(BOOL)hasMockingboard {
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
		target->has_mockingboard = hasMockingboard;
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

- (instancetype)initWithArchimedesModel:(CSMachineArchimedesModel)model {
	self = [super init];
	if(self) {
		auto target = std::make_unique<Analyser::Static::Acorn::ArchimedesTarget>();
		_targets.push_back(std::move(target));
	}
	return self;
}

- (instancetype)initWithAtariSTMemorySize:(Kilobytes)memorySize {
	self = [super init];
	if(self) {
		using Target = Analyser::Static::AtariST::Target;
		auto target = std::make_unique<Target>();
		switch(memorySize) {
			default:	target->memory_size = Target::MemorySize::FiveHundredAndTwelveKilobytes;	break;
			case 1024:	target->memory_size = Target::MemorySize::OneMegabyte;						break;
			case 4096:	target->memory_size = Target::MemorySize::FourMegabytes;					break;
		}
		_targets.push_back(std::move(target));
	}
	return self;
}

- (instancetype)initWithBBCMicroDFS:(BOOL)dfs adfs:(BOOL)adfs sidewaysRAM:(BOOL)sidewaysRAM secondProcessor:(CSMachineBBCMicroSecondProcessor)secondProcessor {
	self = [super init];
	if(self) {
		using Target = Analyser::Static::Acorn::BBCMicroTarget;
		auto target = std::make_unique<Target>();
		target->has_1770dfs = dfs;
		target->has_adfs = adfs;
		target->has_sideways_ram = sidewaysRAM;

		switch(secondProcessor) {
			case CSMachineBBCMicroSecondProcessorNone:	target->tube_processor = Target::TubeProcessor::None;		break;
			case CSMachineBBCMicroSecondProcessor65C02:	target->tube_processor = Target::TubeProcessor::WDC65C02;	break;
			case CSMachineBBCMicroSecondProcessorZ80:	target->tube_processor = Target::TubeProcessor::Z80;		break;
		}
		_targets.push_back(std::move(target));
	}
	return self;
}

- (instancetype)initWithCommodoreTEDModel:(CSMachineCommodoreTEDModel)model hasC1541:(BOOL)hasC1541 {
	self = [super init];
	if(self) {
		using Target = Analyser::Static::Commodore::Plus4Target;
		auto target = std::make_unique<Target>();
		target->has_c1541 = hasC1541;
		_targets.push_back(std::move(target));
	}
	return self;
}

- (instancetype)initWithElectronDFS:(BOOL)dfs adfs:(BOOL)adfs ap6:(BOOL)ap6 sidewaysRAM:(BOOL)sidewaysRAM {
	self = [super init];
	if(self) {
		auto target = std::make_unique<Analyser::Static::Acorn::ElectronTarget>();
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
			case CSPCCompatibleSpeedOriginal:	target->model = Analyser::Static::PCCompatible::Model::XT;		break;
			case CSPCCompatibleSpeedTurbo:		target->model = Analyser::Static::PCCompatible::Model::TurboXT;	break;
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
		using Target = Analyser::Static::Commodore::Vic20Target;
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
	// TODO: the below could be worked out dynamically, I think. It's a bit of a hangover from before configuration
	// options were reflective.
	switch(_targets.front()->machine) {
		case Analyser::Machine::AmstradCPC:		return @"CompositeDynamicCropOptions";
		case Analyser::Machine::Archimedes:		return @"QuickLoadOptions";
		case Analyser::Machine::AppleII:		return @"AppleIIOptions";
		case Analyser::Machine::Atari2600:		return @"Atari2600Options";
		case Analyser::Machine::AtariST:		return @"CompositeOptions";
		case Analyser::Machine::BBCMicro:		return @"DynamicCropOptions";
		case Analyser::Machine::ColecoVision:	return @"CompositeOptions";
		case Analyser::Machine::Electron:		return @"QuickLoadCompositeOptions";
		case Analyser::Machine::Enterprise:		return @"CompositeOptions";
		case Analyser::Machine::Macintosh:		return @"MacintoshOptions";
		case Analyser::Machine::MasterSystem:	return @"CompositeOptions";
		case Analyser::Machine::MSX:			return @"QuickLoadCompositeOptions";
		case Analyser::Machine::Oric:			return @"OricOptions";
		case Analyser::Machine::Plus4:			return @"QuickLoadCompositeOptions";
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

- (nonnull CSMediaSet *)mediaSet {
	Analyser::Static::Media net;
	for(const auto &target: _targets) {
		net += target->media;
	}
	return [[CSMediaSet alloc] initWithMedia:net];
}

@end
