//
//  CSStaticAnalyser.h
//  Clock Signal
//
//  Created by Thomas Harte on 31/08/2016.
//  Copyright © 2016 Thomas Harte. All rights reserved.
//

#import <Foundation/Foundation.h>

@class CSMachine;

typedef NS_ENUM(NSInteger, CSMachineCPCModel) {
	CSMachineCPCModel464,
	CSMachineCPCModel664,
	CSMachineCPCModel6128
};

typedef NS_ENUM(NSInteger, CSMachineOricModel) {
	CSMachineOricModelOric1,
	CSMachineOricModelOricAtmos
};

typedef NS_ENUM(NSInteger, CSMachineVic20Region) {
	CSMachineVic20RegionAmerican,
	CSMachineVic20RegionEuropean,
	CSMachineVic20RegionDanish,
	CSMachineVic20RegionSwedish,
	CSMachineVic20RegionJapanese,
};

typedef int Kilobytes;

@interface CSStaticAnalyser : NSObject

- (instancetype)initWithFileAtURL:(NSURL *)url;

- (instancetype)initWithElectronDFS:(BOOL)dfs adfs:(BOOL)adfs;
- (instancetype)initWithAmstradCPCModel:(CSMachineCPCModel)model;
- (instancetype)initWithMSXHasDiskDrive:(BOOL)hasDiskDrive;
- (instancetype)initWithOricModel:(CSMachineOricModel)model hasMicrodrive:(BOOL)hasMicrodrive;
- (instancetype)initWithVic20Region:(CSMachineVic20Region)region memorySize:(Kilobytes)memorySize hasC1540:(BOOL)hasC1540;
- (instancetype)initWithZX80MemorySize:(Kilobytes)memorySize useZX81ROM:(BOOL)useZX81ROM;
- (instancetype)initWithZX81MemorySize:(Kilobytes)memorySize;
- (instancetype)initWithAppleII;

@property(nonatomic, readonly) NSString *optionsPanelNibName;
@property(nonatomic, readonly) NSString *displayName;

@end

@interface CSMediaSet : NSObject

- (instancetype)initWithFileAtURL:(NSURL *)url;
- (void)applyToMachine:(CSMachine *)machine;

@end
