//
//  OpenGLView.m
//  ElectrEm
//
//  Created by Thomas Harte on 16/07/2015.
//  Copyright © 2015 Thomas Harte. All rights reserved.
//

#import "OpenGLView.h"
@import CoreVideo;

@implementation CSOpenGLView {
	CVDisplayLinkRef displayLink;
}

- (instancetype)initWithCoder:(nonnull NSCoder *)coder
{
	self = [super initWithCoder:coder];

	if(self)
	{
	}

	return self;
}

@end
