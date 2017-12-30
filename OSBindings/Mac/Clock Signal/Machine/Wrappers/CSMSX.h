//
//  CSMSX.h
//  Clock Signal
//
//  Created by Thomas Harte on 03/12/2017.
//  Copyright © 2017 Thomas Harte. All rights reserved.
//

#import "CSMachine.h"
#import "CSFastLoading.h"

@interface CSMSX : CSMachine <CSFastLoading>

- (instancetype)init;

@end
