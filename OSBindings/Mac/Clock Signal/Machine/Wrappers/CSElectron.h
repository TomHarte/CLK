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

@interface CSElectron : CSMachine <CSKeyboardMachine, CSFastLoading>

- (void)setOSROM:(nonnull NSData *)rom;
- (void)setBASICROM:(nonnull NSData *)rom;
- (void)setROM:(nonnull NSData *)rom slot:(int)slot;
- (BOOL)openUEFAtURL:(nonnull NSURL *)URL;

- (void)analyse:(NSURL *)url;

@property (nonatomic, assign) BOOL useFastLoadingHack;
@property (nonatomic, assign) BOOL useTelevisionOutput;

@end
