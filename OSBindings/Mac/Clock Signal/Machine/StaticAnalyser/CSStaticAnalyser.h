//
//  CSStaticAnalyser.h
//  Clock Signal
//
//  Created by Thomas Harte on 31/08/2016.
//  Copyright 2016 Thomas Harte. All rights reserved.
//

#import <Foundation/Foundation.h>

NS_ASSUME_NONNULL_BEGIN

@class CSMachine;

@interface CSMediaSet : NSObject

- (instancetype)initWithFileAtURL:(NSURL *)url;
- (void)applyToMachine:(CSMachine *)machine;
- (void)addPermissionHandler;

@property(nonatomic, readonly) BOOL empty;

@end

typedef NS_ENUM(NSInteger, CSMachineAmigaModel) {
	CSMachineAmigaModelA500,
};

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

typedef NS_ENUM(NSInteger, CSMachineAppleIIgsModel) {
	CSMachineAppleIIgsModelROM00,
	CSMachineAppleIIgsModelROM01,
	CSMachineAppleIIgsModelROM03,
};

typedef NS_ENUM(NSInteger, CSMachineArchimedesModel) {
	CSMachineArchimedesModelFirstGeneration,
};

typedef NS_ENUM(NSInteger, CSMachineBBCMicroSecondProcessor) {
	CSMachineBBCMicroSecondProcessorNone,
	CSMachineBBCMicroSecondProcessor65C02,
	CSMachineBBCMicroSecondProcessorZ80,
};

typedef NS_ENUM(NSInteger, CSMachineCommodoreTEDModel) {
	CSMachineCommodoreTEDModelC16,
	CSMachineCommodoreTEDModelPlus4,
};

typedef NS_ENUM(NSInteger, CSMachineCPCModel) {
	CSMachineCPCModel464,
	CSMachineCPCModel664,
	CSMachineCPCModel6128
};

typedef NS_ENUM(NSInteger, CSMachineEnterpriseModel) {
	CSMachineEnterpriseModel64,
	CSMachineEnterpriseModel128,
	CSMachineEnterpriseModel256,
};

typedef NS_ENUM(NSInteger, CSMachineEnterpriseSpeed) {
	CSMachineEnterpriseSpeed4MHz,
	CSMachineEnterpriseSpeed6MHz
};

typedef NS_ENUM(NSInteger, CSMachineEnterpriseEXOS) {
	CSMachineEnterpriseEXOSVersion21,
	CSMachineEnterpriseEXOSVersion20,
	CSMachineEnterpriseEXOSVersion10,
};

typedef NS_ENUM(NSInteger, CSMachineEnterpriseBASIC) {
	CSMachineEnterpriseBASICVersion21,
	CSMachineEnterpriseBASICVersion11,
	CSMachineEnterpriseBASICVersion10,
	CSMachineEnterpriseBASICNone,
};

typedef NS_ENUM(NSInteger, CSMachineEnterpriseDOS) {
	CSMachineEnterpriseDOSEXDOS,
	CSMachineEnterpriseDOSNone,
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
	CSMachineOricDiskInterfacePravetz,
	CSMachineOricDiskInterfaceJasmin,
	CSMachineOricDiskInterfaceBD500
};

typedef NS_ENUM(NSInteger, CSMachineSpectrumModel) {
	CSMachineSpectrumModelSixteenK,
	CSMachineSpectrumModelFortyEightK,
	CSMachineSpectrumModelOneTwoEightK,
	CSMachineSpectrumModelPlus2,
	CSMachineSpectrumModelPlus2a,
	CSMachineSpectrumModelPlus3,
};

typedef NS_ENUM(NSInteger, CSMachineVic20Region) {
	CSMachineVic20RegionAmerican,
	CSMachineVic20RegionEuropean,
	CSMachineVic20RegionDanish,
	CSMachineVic20RegionSwedish,
	CSMachineVic20RegionJapanese,
};

typedef NS_ENUM(NSInteger, CSMachineMSXModel) {
	CSMachineMSXModelMSX1,
	CSMachineMSXModelMSX2,
};

typedef NS_ENUM(NSInteger, CSMachineMSXRegion) {
	CSMachineMSXRegionAmerican,
	CSMachineMSXRegionEuropean,
	CSMachineMSXRegionJapanese,
};

typedef NS_ENUM(NSInteger, CSPCCompatibleSpeed) {
	CSPCCompatibleSpeedOriginal,
	CSPCCompatibleSpeedTurbo,
};

typedef NS_ENUM(NSInteger, CSPCCompatibleVideoAdaptor) {
	CSPCCompatibleVideoAdaptorMDA,
	CSPCCompatibleVideoAdaptorCGA,
};

typedef int Kilobytes;

@interface CSStaticAnalyser : NSObject

- (nullable instancetype)initWithFileAtURL:(NSURL *)url;

- (instancetype)initWithAmigaModel:(CSMachineAmigaModel)model
	chipMemorySize:(Kilobytes)chipMemorySize
	fastMemorySize:(Kilobytes)fastMemorySize;

- (instancetype)initWithAmstradCPCModel:(CSMachineCPCModel)model;

- (instancetype)initWithAppleIIModel:(CSMachineAppleIIModel)model
	diskController:(CSMachineAppleIIDiskController)diskController
	hasMockingboard:(BOOL)hasMockingboard;

- (instancetype)initWithAppleIIgsModel:(CSMachineAppleIIgsModel)model
	memorySize:(Kilobytes)memorySize;

- (instancetype)initWithArchimedesModel:(CSMachineArchimedesModel)model;

- (instancetype)initWithAtariSTMemorySize:(Kilobytes)memorySize;

- (instancetype)initWithBBCMicroDFS:(BOOL)dfs
	adfs:(BOOL)adfs
	sidewaysRAM:(BOOL)sidewaysRAM
	beebSID:(BOOL)beebSID
	secondProcessor:(CSMachineBBCMicroSecondProcessor)secondProcessor;

- (instancetype)initWithCommodoreTEDModel:(CSMachineCommodoreTEDModel)model
	hasC1541:(BOOL)hasC1541;

- (instancetype)initWithElectronDFS:(BOOL)dfs
	adfs:(BOOL)adfs
	ap6:(BOOL)ap6
	sidewaysRAM:(BOOL)sidewaysRAM;

- (instancetype)initWithEnterpriseModel:(CSMachineEnterpriseModel)model
	speed:(CSMachineEnterpriseSpeed)speed
	exosVersion:(CSMachineEnterpriseEXOS)exosVersion
	basicVersion:(CSMachineEnterpriseBASIC)basicVersion
	dos:(CSMachineEnterpriseDOS)dos;

- (instancetype)initWithMacintoshModel:(CSMachineMacintoshModel)model;

- (instancetype)initWithMSXModel:(CSMachineMSXModel)model
	region:(CSMachineMSXRegion)region
	hasDiskDrive:(BOOL)hasDiskDrive
	hasMSXMUSIC:(BOOL)hasMSXMUSIC;

- (instancetype)initWithOricModel:(CSMachineOricModel)model
	diskInterface:(CSMachineOricDiskInterface)diskInterface;

- (instancetype)initWithSpectrumModel:(CSMachineSpectrumModel)model;

- (instancetype)initWithVic20Region:(CSMachineVic20Region)region
	memorySize:(Kilobytes)memorySize
	hasC1540:(BOOL)hasC1540;

- (instancetype)initWithZX80MemorySize:(Kilobytes)memorySize
	useZX81ROM:(BOOL)useZX81ROM;

- (instancetype)initWithZX81MemorySize:(Kilobytes)memorySize;

- (instancetype)initWithPCCompatibleSpeed:(CSPCCompatibleSpeed)speed
	videoAdaptor:(CSPCCompatibleVideoAdaptor)adaptor;

@property(nonatomic, readonly, nullable) NSString *optionsNibName;
@property(nonatomic, readonly) NSString *displayName;
@property(nonatomic, readonly, nonnull) CSMediaSet *mediaSet;

@end

NS_ASSUME_NONNULL_END
