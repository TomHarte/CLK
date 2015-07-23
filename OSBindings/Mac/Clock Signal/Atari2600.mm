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
		printf("\n\n===\n\n");
		int c = 0;
		for(int run = 0; run < frame->number_of_runs; run++)
		{
			char character = ' ';
			switch(frame->runs[run].type)
			{
				case Outputs::CRTRun::Type::Sync:	character = '<'; break;
				case Outputs::CRTRun::Type::Level:	character = '_'; break;
				case Outputs::CRTRun::Type::Data:	character = '-'; break;
				case Outputs::CRTRun::Type::Blank:	character = ' '; break;
			}

			if(frame->runs[run].start_point.dst_x < 1.0 / 224.0)
			{
				printf("\n[%0.2f]: ", frame->runs[run].start_point.dst_y);
				c++;
			}

			printf("(%0.2f): ", frame->runs[run].start_point.dst_x);
			float length = fabsf(frame->runs[run].end_point.dst_x - frame->runs[run].start_point.dst_x);
			int iLength = (int)(length * 64.0);
			for(int c = 0; c < iLength; c++)
			{
				putc(character, stdout);
			}
		}

		printf("\n\n[%d]\n\n", c);
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
