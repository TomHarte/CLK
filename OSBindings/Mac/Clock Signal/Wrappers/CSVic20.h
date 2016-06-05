//
//  CSVic20.h
//  Clock Signal
//
//  Created by Thomas Harte on 04/06/2016.
//  Copyright © 2016 Thomas Harte. All rights reserved.
//

#import "CSMachine.h"
#import "CSKeyboardMachine.h"

@interface CSVic20 : CSMachine <CSKeyboardMachine>

- (void)setKernelROM:(nonnull NSData *)rom;
- (void)setBASICROM:(nonnull NSData *)rom;
- (void)setCharactersROM:(nonnull NSData *)rom;
- (void)setPRG:(nonnull NSData *)rom;

@end
