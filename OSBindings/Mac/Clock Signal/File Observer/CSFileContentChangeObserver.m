//
//  CSFileObserver.m
//  Clock Signal
//
//  Created by Thomas Harte on 22/02/2025.
//  Copyright © 2025 Thomas Harte. All rights reserved.
//

#import "CSFileContentChangeObserver.h"

@implementation CSFileContentChangeObserver {
	int _fileDescriptor;
	dispatch_source_t _source;
}

- (nullable instancetype)initWithURL:(nonnull NSURL *)url handler:(nonnull dispatch_block_t)handler {
	if(!url.isFileURL) {
		return nil;
	}

	self = [super init];
	if(self) {
		_fileDescriptor = open(url.fileSystemRepresentation, O_EVTONLY);
		if(_fileDescriptor <= 0) {
			return nil;
		}

		_source = dispatch_source_create(
			DISPATCH_SOURCE_TYPE_VNODE,
			(uintptr_t)_fileDescriptor,
			DISPATCH_VNODE_WRITE,
			dispatch_get_global_queue(QOS_CLASS_DEFAULT, 0)
		);
		dispatch_source_set_event_handler(_source, handler);
		dispatch_resume(_source);
	}
	return self;
}

- (void)dealloc {
	if(_fileDescriptor) {
		close(_fileDescriptor);
		dispatch_source_cancel(_source);
	}
}

@end
