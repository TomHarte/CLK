//
//  ScanTarget.h
//  Clock Signal
//
//  Created by Thomas Harte on 02/08/2020.
//  Copyright © 2020 Thomas Harte. All rights reserved.
//

#import <Foundation/Foundation.h>
#import <MetalKit/MetalKit.h>

/*!
	Provides a ScanTarget that uses Metal as its back-end.
*/
@interface CSScanTarget : NSObject <MTKViewDelegate>

- (nonnull instancetype)initWithView:(nonnull MTKView *)view;

// Draws all scans currently residing at the scan target to the backing store,
// ready for output when next requested.
- (void)updateFrameBuffer;

- (nonnull NSBitmapImageRep *)imageRepresentation;

- (void)willChangeOwner;

@end
