//
//  CSElectron.h
//  Clock Signal
//
//  Created by Thomas Harte on 04/01/2016.
//  Copyright © 2016 Thomas Harte. All rights reserved.
//

#import "CSMachine.h"
#import "CSKeyboardMachine.h"
#import "CSFastLoading.h"

@class CSStaticAnalyser;

@interface CSElectron : CSMachine <CSKeyboardMachine, CSFastLoading>

- (void)setOSROM:(nonnull NSData *)rom;
- (void)setBASICROM:(nonnull NSData *)rom;
- (void)setDFSROM:(nonnull NSData *)rom;
- (void)setADFSROM:(nonnull NSData *)rom;

@property (nonatomic, assign) BOOL useFastLoadingHack;
@property (nonatomic, assign) BOOL useTelevisionOutput;

@end
