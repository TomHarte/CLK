//
//  CSScanTarget+C__ScanTarget.h
//  Clock Signal
//
//  Created by Thomas Harte on 08/08/2020.
//  Copyright © 2020 Thomas Harte. All rights reserved.
//

#import "CSScanTarget.h"
#include "ScanTarget.hpp"

@interface CSScanTarget (CppScanTarget)

@property (nonatomic, readonly, nonnull) Outputs::Display::ScanTarget *scanTarget;

@end
