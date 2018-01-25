//
//  CSStaticAnalyser+ResultVector.h
//  Clock Signal
//
//  Created by Thomas Harte on 24/01/2018.
//  Copyright © 2018 Thomas Harte. All rights reserved.
//

#import "CSStaticAnalyser.h"

@interface CSStaticAnalyser (ResultVector)

- (std::vector<Analyser::Static::Target> &)targets;

@end
