//
//  NSString+StringView.m
//  Clock Signal
//
//  Created by Thomas Harte on 17/02/2026.
//  Copyright © 2026 Thomas Harte. All rights reserved.
//

#import "NSString+StringView.h"

@implementation NSString (StringView)

- (nonnull instancetype)initWithStringView:(std::string_view)view {
	return [self initWithBytes:view.data() length:view.size() encoding:NSUTF8StringEncoding];
}

@end
