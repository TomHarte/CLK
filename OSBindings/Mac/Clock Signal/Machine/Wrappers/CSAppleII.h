//
//  CSAppleII.h
//  Clock Signal
//
//  Created by Thomas Harte on 07/06/2021.
//  Copyright © 2021 Thomas Harte. All rights reserved.
//

@class CSAppleII;
#import "CSMachine.h"

@interface CSAppleII : NSObject

- (instancetype)initWithAppleII:(void *)appleII owner:(CSMachine *)machine;

@property (nonatomic, assign) BOOL useSquarePixels;

@end
