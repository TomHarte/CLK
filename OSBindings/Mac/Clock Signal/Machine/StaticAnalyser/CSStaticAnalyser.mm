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

#import "NSString+StringView.h"

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

#include "Storage/FileBundle/FileBundle.hpp"

#import "Clock_Signal-Swift.h"

namespace {

struct PermissionDelegate: public Storage::FileBundle::FileBundle::PermissionDelegate {
	void validate_open(
		Storage::FileBundle::FileBundle &bundle,
		const std::string_view path,
		const Storage::FileMode mode
	) {
		NSData *bookmarkData;
		NSString *stringPath = [[NSString alloc] initWithStringView:path];
		NSURL *url = [NSURL fileURLWithPath:stringPath isDirectory:NO];
		NSError *error;

		// Check for and possibly apply an existing bookmark.
		NSString *bookmarkKey = [[url URLByDeletingLastPathComponent] absoluteString];
		bookmarkData = [[NSUserDefaults standardUserDefaults] objectForKey:bookmarkKey];
		if(bookmarkData) {
			NSURL *accessURL =
				[NSURL
					URLByResolvingBookmarkData:bookmarkData
					options:NSURLBookmarkResolutionWithSecurityScope | NSURLBookmarkResolutionWithoutUI
					relativeToURL:nil
					bookmarkDataIsStale:nil
					error:nil];
			[accessURL startAccessingSecurityScopedResource];
		}

		// If the file exists can now be accessed, no further action required.
		NSFileHandle *file = [&]() {
			switch(mode) {
				case Storage::FileMode::ReadWrite:	{
					NSFileHandle *updating = [NSFileHandle fileHandleForUpdatingURL:url error:&error];
					if(updating) return updating;
					[[fallthrough]];
				}
				default:
				case Storage::FileMode::Read:		return [NSFileHandle fileHandleForReadingFromURL:url error:&error];
				case Storage::FileMode::Rewrite:	return [NSFileHandle fileHandleForWritingToURL:url error:&error];
			}
		}();

		// Managed to open the file: that's enough.
		if(file) {
			return;
		}

		// Otherwise: if not being opened exclusively for reading, see whether the file can be created.
		if(
			error.domain == NSCocoaErrorDomain &&
			error.code == NSFileNoSuchFileError &&
			mode != Storage::FileMode::Read
		) {
			NSFileManager *manager = [NSFileManager defaultManager];
			if([manager createFileAtPath:url.path contents:nil attributes:nil]) {
				[manager removeItemAtPath:url.path error:&error];
				return;
			}
		}

		// Failing that, ask the user for permission and keep the bookmark.
		__block NSURL *selectedURL;

		// Ask the user for permission.
		dispatch_sync(dispatch_get_main_queue(), ^{
			NSOpenPanel *request = [NSOpenPanel openPanel];
			request.prompt = NSLocalizedString(@"Grant Permission", @"");
			request.message = NSLocalizedString(@"Please Grant Permission For Full Folder Access", @"");
			request.canChooseFiles = NO;
			request.allowsMultipleSelection = NO;
			request.canChooseDirectories = YES;
			[request setDirectoryURL:[url URLByDeletingLastPathComponent]];

			// TODO: a nicer accessory view; NSTextField or the relevant equivalent
			// with an attributed string might work.
			request.accessoryView = [NSTextField labelWithString:[&] {
				const auto key_file = bundle.key_file();

				if(key_file) {
					return [NSString stringWithFormat:
						@"Clock Signal cannot access your files without explicit permission but "
						@"%s is trying to use additional files in its folder.\n"
						@"Please select 'Grant Permission' if you are willing to let it to do so.",
							key_file->c_str()
					];
				} else {
					assert(bundle.base_path().has_value());
					return [NSString stringWithFormat:
						@"Clock Signal cannot access your files without explicit permission but "
						@"your emulated machine is trying to use additional files from %s.\n"
						@"Please select 'Grant Permission' if you are willing to let it to do so.",
							bundle.base_path()->c_str()
					];
				}
			}()];

			request.accessoryViewDisclosed = YES;
			[request runModal];

			selectedURL = request.URL;
		});

		// Store bookmark data for potential later retrieval.
		// That amounts to this application remembering the user's permission.
		error = nil;
		[[NSUserDefaults standardUserDefaults]
			setObject:[selectedURL
				bookmarkDataWithOptions:NSURLBookmarkCreationWithSecurityScope
				includingResourceValuesForKeys:nil
				relativeToURL:nil
				error:&error]
			forKey:bookmarkKey];
	}

	void validate_erase(Storage::FileBundle::FileBundle &, std::string_view) {
		// Currently a no-op, as it so happens that the only machine that currently
		// uses a file bundle is the Enterprise, and its semantics involve opening
		// a file before it can be erased.
	}
};

PermissionDelegate permission_delegate;

}

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

- (void)addPermissionHandler {
	for(const auto &bundle: _media.file_bundles) {
		bundle->set_permission_delegate(&permission_delegate);
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

- (instancetype)initWithAmigaModel:(CSMachineAmigaModel)model
	chipMemorySize:(Kilobytes)chipMemorySize
	fastMemorySize:(Kilobytes)fastMemorySize
{
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

- (instancetype)initWithAppleIIModel:(CSMachineAppleIIModel)model
	diskController:(CSMachineAppleIIDiskController)diskController
	hasMockingboard:(BOOL)hasMockingboard
{
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

- (instancetype)initWithAppleIIgsModel:(CSMachineAppleIIgsModel)model
	memorySize:(Kilobytes)memorySize
{
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

- (instancetype)initWithBBCMicroDFS:(BOOL)dfs
	adfs:(BOOL)adfs
	sidewaysRAM:(BOOL)sidewaysRAM
	beebSID:(BOOL)beebSID
	secondProcessor:(CSMachineBBCMicroSecondProcessor)secondProcessor
{
	self = [super init];
	if(self) {
		using Target = Analyser::Static::Acorn::BBCMicroTarget;
		auto target = std::make_unique<Target>();
		target->has_1770dfs = dfs;
		target->has_adfs = adfs;
		target->has_beebsid = beebSID;
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

- (instancetype)initWithCommodoreTEDModel:(CSMachineCommodoreTEDModel)model
	hasC1541:(BOOL)hasC1541
{
	self = [super init];
	if(self) {
		using Target = Analyser::Static::Commodore::Plus4Target;
		auto target = std::make_unique<Target>();
		target->has_c1541 = hasC1541;
		_targets.push_back(std::move(target));
	}
	return self;
}

- (instancetype)initWithElectronDFS:(BOOL)dfs
	adfs:(BOOL)adfs
	ap6:(BOOL)ap6
	sidewaysRAM:(BOOL)sidewaysRAM
{
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

- (instancetype)initWithEnterpriseModel:(CSMachineEnterpriseModel)model
	speed:(CSMachineEnterpriseSpeed)speed
	exosVersion:(CSMachineEnterpriseEXOS)exosVersion
	basicVersion:(CSMachineEnterpriseBASIC)basicVersion
	dos:(CSMachineEnterpriseDOS)dos
	exposedLocalPath:(nullable NSURL *)path
{
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

		if(path) {
			const auto bundle = std::make_shared<Storage::FileBundle::LocalFSFileBundle>(path.path.UTF8String);
			bundle->set_permission_delegate(&permission_delegate);
			target->media.file_bundles.push_back(std::move(bundle));
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

- (instancetype)initWithMSXModel:(CSMachineMSXModel)model
	region:(CSMachineMSXRegion)region
	hasDiskDrive:(BOOL)hasDiskDrive
	hasMSXMUSIC:(BOOL)hasMSXMUSIC
{
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

- (instancetype)initWithOricModel:(CSMachineOricModel)model
	diskInterface:(CSMachineOricDiskInterface)diskInterface
{
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


- (instancetype)initWithPCCompatibleSpeed:(CSPCCompatibleSpeed)speed
	videoAdaptor:(CSPCCompatibleVideoAdaptor)adaptor
{
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

- (instancetype)initWithVic20Region:(CSMachineVic20Region)region
	memorySize:(Kilobytes)memorySize
	hasC1540:(BOOL)hasC1540
{
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

- (instancetype)initWithZX80MemorySize:(Kilobytes)memorySize
	useZX81ROM:(BOOL)useZX81ROM
{
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
