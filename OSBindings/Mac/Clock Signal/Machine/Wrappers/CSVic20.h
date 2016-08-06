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

@interface CSVic20 : CSMachine <CSKeyboardMachine, CSFastLoading>

- (void)setKernelROM:(nonnull NSData *)rom;
- (void)setBASICROM:(nonnull NSData *)rom;
- (void)setCharactersROM:(nonnull NSData *)rom;
- (void)setDriveROM:(nonnull NSData *)rom;

- (void)setPRG:(nonnull NSData *)prg;
- (BOOL)openTAPAtURL:(nonnull NSURL *)URL;
- (BOOL)openG64AtURL:(nonnull NSURL *)URL;
- (BOOL)openD64AtURL:(nonnull NSURL *)URL;

@property (nonatomic, assign) BOOL useFastLoadingHack;
@property (nonatomic, assign) BOOL shouldLoadAutomatically;

@end
