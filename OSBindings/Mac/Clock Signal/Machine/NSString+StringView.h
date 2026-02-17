//
//  NSString+StringView.h
//  Clock Signal
//
//  Created by Thomas Harte on 17/02/2026.
//  Copyright © 2026 Thomas Harte. All rights reserved.
//

#import <Foundation/Foundation.h>

#include <string_view>

@interface NSString (StringView)

- (nonnull instancetype)initWithStringView:(std::string_view)view;

@end
