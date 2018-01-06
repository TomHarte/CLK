//
//  C1540Bridge.h
//  Clock Signal
//
//  Created by Thomas Harte on 09/07/2016.
//  Copyright © 2016 Thomas Harte. All rights reserved.
//

#import <Foundation/Foundation.h>

@interface C1540Bridge : NSObject

@property (nonatomic) BOOL attentionLine;
@property (nonatomic) BOOL dataLine;
@property (nonatomic) BOOL clockLine;

- (void)runForCycles:(NSUInteger)numberOfCycles;

@end
