//
//  NSString+NSString_StringView.h
//  Clock Signal
//
//  Created by Thomas Harte on 07/10/2026.
//  Copyright © 2026 Thomas Harte. All rights reserved.
//

#import <Foundation/Foundation.h>

#include <string_view>

@interface NSString (NSString_StringView)

+ (nonnull NSString *)stringWithView:(std::string_view)view;
+ (nonnull NSString *)stringWithViewNoCopy:(std::string_view)view;

@end
