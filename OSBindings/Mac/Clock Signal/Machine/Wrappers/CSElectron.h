//
//  CSElectron.h
//  Clock Signal
//
//  Created by Thomas Harte on 04/01/2016.
//  Copyright © 2016 Thomas Harte. All rights reserved.
//

#import "CSMachine.h"
#import "CSFastLoading.h"

@interface CSElectron : CSMachine <CSFastLoading>

- (instancetype)init;

@end
