//
//  CSElectron.h
//  Clock Signal
//
//  Created by Thomas Harte on 04/01/2016.
//  Copyright © 2016 Thomas Harte. All rights reserved.
//

#include "CSMachine.h"
#import "KeyCodes.h"
#import "CSKeyboardMachine.h"

@interface CSElectron : CSMachine <CSKeyboardMachine>

- (void)setOSROM:(nonnull NSData *)rom;
- (void)setBASICROM:(nonnull NSData *)rom;
- (void)setROM:(nonnull NSData *)rom slot:(int)slot;
- (BOOL)openUEFAtURL:(nonnull NSURL *)URL;

@property (nonatomic, assign) BOOL useFastLoadingHack;
@property (nonatomic, assign) BOOL useTelevisionOutput;

@end
