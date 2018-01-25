//
//  CSStaticAnalyser.h
//  Clock Signal
//
//  Created by Thomas Harte on 31/08/2016.
//  Copyright © 2016 Thomas Harte. All rights reserved.
//

#import <Foundation/Foundation.h>

@class CSMachine;

@interface CSStaticAnalyser : NSObject

- (instancetype)initWithFileAtURL:(NSURL *)url;

@property(nonatomic, readonly) NSString *optionsPanelNibName;
@property(nonatomic, readonly) NSString *displayName;

- (void)applyToMachine:(CSMachine *)machine;

@end

@interface CSMediaSet : NSObject

- (instancetype)initWithFileAtURL:(NSURL *)url;
- (void)applyToMachine:(CSMachine *)machine;

@end
