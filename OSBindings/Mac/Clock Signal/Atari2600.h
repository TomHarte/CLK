//
//  Atari2600.h
//  Clock Signal
//
//  Created by Thomas Harte on 14/07/2015.
//  Copyright © 2015 Thomas Harte. All rights reserved.
//

#import <Foundation/Foundation.h>

@interface CSAtari2600 : NSObject

- (void)runForNumberOfCycles:(int)cycles;
- (void)setROM:(NSData *)rom;

@end
