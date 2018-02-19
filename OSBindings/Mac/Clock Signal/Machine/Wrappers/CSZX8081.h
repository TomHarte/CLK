//
//  CSZX8081.h
//  Clock Signal
//
//  Created by Thomas Harte on 04/06/2017.
//  Copyright © 2017 Thomas Harte. All rights reserved.
//

@class CSZX8081;
#import "CSMachine.h"

@interface CSZX8081 : NSObject

- (instancetype)initWithZX8081:(void *)zx8081 owner:(CSMachine *)machine;

@property (nonatomic, assign) BOOL tapeIsPlaying;

@end
