//
//  ScanTarget.h
//  Clock Signal
//
//  Created by Thomas Harte on 02/08/2020.
//  Copyright © 2020 Thomas Harte. All rights reserved.
//

#import <Foundation/Foundation.h>

/*!
	Provides a ScanTarget that uses Metal as its back-end.
*/
@interface CSScanTarget : NSObject

- (nonnull instancetype)init;

@end
