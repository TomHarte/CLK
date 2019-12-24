//
//  MOS6522Bridge.m
//  Clock Signal
//
//  Created by Thomas Harte on 18/06/2016.
//  Copyright 2016 Thomas Harte. All rights reserved.
//

#import "MOS6522Bridge.h"

#include "6522.hpp"
#include <memory>

@class MOS6522Bridge;

class VanillaVIAPortHandler: public MOS::MOS6522::PortHandler {
	public:
		MOS6522Bridge *bridge;
		bool irq_line;
		uint8_t port_a_value;
		uint8_t port_b_value;
		bool control_line_values[2][2];

		/*
			All methods below here are to replace those defined by
			MOS::MOS6522::PortHandler.
		*/
		void set_interrupt_status(bool new_status) {
			irq_line = new_status;
		}

		uint8_t get_port_input(MOS::MOS6522::Port port) {
			return port ? port_b_value : port_a_value;
		}

		void set_control_line_output(MOS::MOS6522::Port port, MOS::MOS6522::Line line, bool value) {
			control_line_values[int(port)][int(line)] = value;
		}
};

@implementation MOS6522Bridge {
	VanillaVIAPortHandler _viaPortHandler;
	std::unique_ptr<MOS::MOS6522::MOS6522<VanillaVIAPortHandler>> _via;
}

- (instancetype)init {
	self = [super init];
	if(self) {
		_via = std::make_unique<MOS::MOS6522::MOS6522<VanillaVIAPortHandler>(_viaPortHandler);
		_viaPortHandler.bridge = self;
	}
	return self;
}

- (void)setValue:(uint8_t)value forRegister:(NSUInteger)registerNumber {
	_via->set_register((int)registerNumber, value);
}

- (uint8_t)valueForRegister:(NSUInteger)registerNumber {
	return _via->get_register((int)registerNumber);
}

- (void)runForHalfCycles:(NSUInteger)numberOfHalfCycles {
	_via->run_for(HalfCycles((int)numberOfHalfCycles));
}

- (BOOL)irqLine {
	return _viaPortHandler.irq_line;
}

- (void)setPortAInput:(uint8_t)portAInput {
	_viaPortHandler.port_a_value = portAInput;
}

- (uint8_t)portAInput {
	return _viaPortHandler.port_a_value;
}

- (void)setPortBInput:(uint8_t)portBInput {
	_viaPortHandler.port_b_value = portBInput;
}

- (uint8_t)portBInput {
	return _viaPortHandler.port_b_value;
}

- (BOOL)valueForControlLine:(MOS6522BridgeLine)line port:(MOS6522BridgePort)port {
	return _viaPortHandler.control_line_values[port][line];
}

@end
