//
//  C1540Bridge.m
//  Clock Signal
//
//  Created by Thomas Harte on 09/07/2016.
//  Copyright 2016 Thomas Harte. All rights reserved.
//

#import "C1540Bridge.h"
#include "C1540.hpp"
#include "NSData+StdVector.h"
#include "CSROMFetcher.hpp"

#include <memory>

class VanillaSerialPort: public Commodore::Serial::Port {
	public:
		void set_input(Commodore::Serial::Line line, Commodore::Serial::LineLevel value) {
			_input_line_levels[(int)line] = value;
		}

		Commodore::Serial::LineLevel _input_line_levels[5];
};

@implementation C1540Bridge {
	std::unique_ptr<Commodore::C1540::Machine> _c1540;
	std::shared_ptr<Commodore::Serial::Bus> _serialBus;
	std::shared_ptr<VanillaSerialPort> _serialPort;
}

- (instancetype)init {
	self = [super init];
	if(self) {
		_serialBus = std::make_shared<::Commodore::Serial::Bus>();
		_serialPort = std::make_shared<VanillaSerialPort>();

		auto rom_fetcher = CSROMFetcher();
		_c1540 = std::make_unique<Commodore::C1540::Machine>(Commodore::C1540::Personality::C1540, rom_fetcher);
		_c1540->set_serial_bus(_serialBus);
		Commodore::Serial::AttachPortAndBus(_serialPort, _serialBus);
	}
	return self;
}

- (void)runForCycles:(NSUInteger)numberOfCycles {
	_c1540->run_for(Cycles((int)numberOfCycles));
}

- (void)setAttentionLine:(BOOL)attentionLine {
	_serialPort->set_output(Commodore::Serial::Line::Attention, attentionLine ? Commodore::Serial::LineLevel::High : Commodore::Serial::LineLevel::Low);
}

- (BOOL)attentionLine {
	return _serialPort->_input_line_levels[Commodore::Serial::Line::Attention];
}

- (void)setDataLine:(BOOL)dataLine {
	_serialPort->set_output(Commodore::Serial::Line::Data, dataLine ? Commodore::Serial::LineLevel::High : Commodore::Serial::LineLevel::Low);
}

- (BOOL)dataLine {
	return _serialPort->_input_line_levels[Commodore::Serial::Line::Data];
}

- (void)setClockLine:(BOOL)clockLine {
	_serialPort->set_output(Commodore::Serial::Line::Clock, clockLine ? Commodore::Serial::LineLevel::High : Commodore::Serial::LineLevel::Low);
}

- (BOOL)clockLine {
	return _serialPort->_input_line_levels[Commodore::Serial::Line::Clock];
}

@end
