//
//  CSVic20.h
//  Clock Signal
//
//  Created by Thomas Harte on 04/06/2016.
//  Copyright © 2016 Thomas Harte. All rights reserved.
//

#import "CSMachine.h"
#import "CSKeyboardMachine.h"
#import "CSFastLoading.h"

typedef NS_ENUM(NSInteger, CSVic20Region)
{
	CSVic20RegionPAL,
	CSVic20RegionNTSC
};

typedef NS_ENUM(NSInteger, CSVic20MemorySize)
{
	CSVic20MemorySize5Kb,
	CSVic20MemorySize8Kb,
	CSVic20MemorySize32Kb,
};

@interface CSVic20 : CSMachine <CSKeyboardMachine, CSFastLoading>

- (void)setKernelROM:(nonnull NSData *)rom;
- (void)setBASICROM:(nonnull NSData *)rom;
- (void)setCharactersROM:(nonnull NSData *)rom;
- (void)setDriveROM:(nonnull NSData *)rom;

@property (nonatomic, assign) BOOL useFastLoadingHack;
@property (nonatomic, assign) BOOL shouldLoadAutomatically;
@property (nonatomic, assign) CSVic20Region region;
@property (nonatomic, assign) CSVic20MemorySize memorySize;

@end
