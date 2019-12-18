//
//  CSStaticAnalyser.h
//  Clock Signal
//
//  Created by Thomas Harte on 31/08/2016.
//  Copyright 2016 Thomas Harte. All rights reserved.
//

#import <Foundation/Foundation.h>

@class CSMachine;

typedef NS_ENUM(NSInteger, CSMachineAppleIIModel) {
	CSMachineAppleIIModelAppleII,
	CSMachineAppleIIModelAppleIIPlus,
	CSMachineAppleIIModelAppleIIe,
	CSMachineAppleIIModelAppleEnhancedIIe
};

typedef NS_ENUM(NSInteger, CSMachineAppleIIDiskController) {
	CSMachineAppleIIDiskControllerNone,
	CSMachineAppleIIDiskControllerSixteenSector,
	CSMachineAppleIIDiskControllerThirteenSector
};

typedef NS_ENUM(NSInteger, CSMachineAtariSTModel) {
	CSMachineAtariSTModel512k,
};

typedef NS_ENUM(NSInteger, CSMachineCPCModel) {
	CSMachineCPCModel464,
	CSMachineCPCModel664,
	CSMachineCPCModel6128
};

typedef NS_ENUM(NSInteger, CSMachineMacintoshModel) {
	CSMachineMacintoshModel128k,
	CSMachineMacintoshModel512k,
	CSMachineMacintoshModel512ke,
	CSMachineMacintoshModelPlus,
};

typedef NS_ENUM(NSInteger, CSMachineOricModel) {
	CSMachineOricModelOric1,
	CSMachineOricModelOricAtmos,
	CSMachineOricModelPravetz
};

typedef NS_ENUM(NSInteger, CSMachineOricDiskInterface) {
	CSMachineOricDiskInterfaceNone,
	CSMachineOricDiskInterfaceMicrodisc,
	CSMachineOricDiskInterfacePravetz
};

typedef NS_ENUM(NSInteger, CSMachineVic20Region) {
	CSMachineVic20RegionAmerican,
	CSMachineVic20RegionEuropean,
	CSMachineVic20RegionDanish,
	CSMachineVic20RegionSwedish,
	CSMachineVic20RegionJapanese,
};

typedef NS_ENUM(NSInteger, CSMachineMSXRegion) {
	CSMachineMSXRegionAmerican,
	CSMachineMSXRegionEuropean,
	CSMachineMSXRegionJapanese,
};

typedef int Kilobytes;

@interface CSStaticAnalyser : NSObject

- (instancetype)initWithFileAtURL:(NSURL *)url;

- (instancetype)initWithElectronDFS:(BOOL)dfs adfs:(BOOL)adfs;
- (instancetype)initWithAmstradCPCModel:(CSMachineCPCModel)model;
- (instancetype)initWithMSXRegion:(CSMachineMSXRegion)region hasDiskDrive:(BOOL)hasDiskDrive;
- (instancetype)initWithOricModel:(CSMachineOricModel)model diskInterface:(CSMachineOricDiskInterface)diskInterface;
- (instancetype)initWithVic20Region:(CSMachineVic20Region)region memorySize:(Kilobytes)memorySize hasC1540:(BOOL)hasC1540;
- (instancetype)initWithZX80MemorySize:(Kilobytes)memorySize useZX81ROM:(BOOL)useZX81ROM;
- (instancetype)initWithZX81MemorySize:(Kilobytes)memorySize;
- (instancetype)initWithAppleIIModel:(CSMachineAppleIIModel)model diskController:(CSMachineAppleIIDiskController)diskController;
- (instancetype)initWithMacintoshModel:(CSMachineMacintoshModel)model;
- (instancetype)initWithAtariSTModel:(CSMachineAtariSTModel)model;

@property(nonatomic, readonly) NSString *optionsPanelNibName;
@property(nonatomic, readonly) NSString *displayName;

@end

@interface CSMediaSet : NSObject

- (instancetype)initWithFileAtURL:(NSURL *)url;
- (void)applyToMachine:(CSMachine *)machine;

@end
