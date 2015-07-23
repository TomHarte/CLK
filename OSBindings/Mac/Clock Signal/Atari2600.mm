//
//  Atari2600.m
//  ElectrEm
//
//  Created by Thomas Harte on 14/07/2015.
//  Copyright © 2015 Thomas Harte. All rights reserved.
//

#import "Atari2600.h"
#import "Atari2600.hpp"

class Atari2600CRTDelegate: public Outputs::CRT::CRTDelegate {
	void crt_did_end_frame(Outputs::CRT *crt, Outputs::CRTFrame *frame)
	{
//		printf("===\n\n");
//		for(int run = 0; run < runs_to_draw; run++)
//		{
//			char character = ' ';
//			switch(runs[run].type)
//			{
//				case Outputs::CRTRun::Type::Sync:	character = '<'; break;
//				case Outputs::CRTRun::Type::Level:	character = '_'; break;
//				case Outputs::CRTRun::Type::Data:	character = '-'; break;
//				case Outputs::CRTRun::Type::Blank:	character = ' '; break;
//			}
//
//			if(runs[run].start_point.dst_x > runs[run].end_point.dst_x)
//			{
//				printf("\n");
//			}
//
//			float length = fabsf(runs[run].end_point.dst_x - runs[run].start_point.dst_x);
//			int iLength = (int)(length * 64.0);
//			for(int c = 0; c < iLength; c++)
//			{
//				putc(character, stdout);
//			}
//
//			if (runs[run].type == Outputs::CRTRun::Type::Sync) printf("\n");
//		}

		crt->return_frame();
	}

};

@implementation CSAtari2600 {
	Atari2600::Machine _atari2600;
	Atari2600CRTDelegate _crtDelegate;
}

- (void)runForNumberOfCycles:(int)cycles {
	_atari2600.run_for_cycles(cycles);
}

- (void)setROM:(NSData *)rom {
	_atari2600.set_rom(rom.length, (const uint8_t *)rom.bytes);
}

- (instancetype)init {
	self = [super init];

	if (self) {
		_atari2600.get_crt()->set_delegate(&_crtDelegate);
	}

	return self;
}

@end
