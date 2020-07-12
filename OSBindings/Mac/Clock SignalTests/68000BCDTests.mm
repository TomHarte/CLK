//
//  68000BCDTests.m
//  Clock SignalTests
//
//  Created by Thomas Harte on 29/06/2019.
//
//  Largely ported from the tests of the Portable 68k Emulator.
//

#import <XCTest/XCTest.h>

#include "TestRunner68000.hpp"

@interface M68000BCDTests : XCTestCase
@end

@implementation M68000BCDTests {
	std::unique_ptr<RAM68000> _machine;
}

- (void)setUp {
	_machine = std::make_unique<RAM68000>();
}

- (void)tearDown {
	_machine.reset();
}

// MARK: ABCD

- (void)testABCD {
	_machine->set_program({
		0xc302,		// ABCD D2, D1
	});
	auto state = _machine->get_processor_state();
	state.data[1] = 0x1234567a;
	state.data[2] = 0xf745ff78;
	_machine->set_processor_state(state);
	_machine->run_for_instructions(1);

	state = _machine->get_processor_state();
	XCTAssert(state.status & Flag::Carry);
	XCTAssertEqual(state.data[1], 0x12345658);
	XCTAssertEqual(state.data[2], 0xf745ff78);
}

- (void)testABCDZero {
	_machine->set_program({
		0xc302,		// ABCD D2, D1
	});
	auto state = _machine->get_processor_state();
	state.data[1] = 0x12345600;
	state.data[2] = 0x12345600;
	state.status = Flag::Zero;
	_machine->set_processor_state(state);
	_machine->run_for_instructions(1);

	state = _machine->get_processor_state();
	XCTAssert(state.status & Flag::Zero);
	XCTAssertEqual(state.data[1], 0x12345600);
	XCTAssertEqual(state.data[2], 0x12345600);
}

- (void)testABCDNegative {
	_machine->set_program({
		0xc302,		// ABCD D2, D1
	});
	auto state = _machine->get_processor_state();
	state.data[1] = 0x12345645;
	state.data[2] = 0x12345654;
	state.status = Flag::Zero;
	_machine->set_processor_state(state);
	_machine->run_for_instructions(1);

	state = _machine->get_processor_state();
	XCTAssert(state.status & Flag::Negative);
	XCTAssertEqual(state.data[1], 0x12345699);
	XCTAssertEqual(state.data[2], 0x12345654);
}

- (void)testABCDWithX {
	_machine->set_program({
		0xc302,		// ABCD D2, D1
	});
	auto state = _machine->get_processor_state();
	state.data[1] = 0x12345645;
	state.data[2] = 0x12345654;
	state.status = Flag::Extend;
	_machine->set_processor_state(state);
	_machine->run_for_instructions(1);

	state = _machine->get_processor_state();
	XCTAssert(state.status & Flag::Carry);
	XCTAssertEqual(state.data[1], 0x12345600);
	XCTAssertEqual(state.data[2], 0x12345654);
}

- (void)testABCDOverflow {
	_machine->set_program({
		0xc302,		// ABCD D2, D1
	});
	auto state = _machine->get_processor_state();
	state.data[1] = 0x1234563e;
	state.data[2] = 0x1234563e;
	state.status = Flag::Extend;
	_machine->set_processor_state(state);
	_machine->run_for_instructions(1);

	state = _machine->get_processor_state();
	XCTAssert(state.status & Flag::Overflow);
	XCTAssertEqual(state.data[1], 0x12345683);
	XCTAssertEqual(state.data[2], 0x1234563e);
}

- (void)testABCDPredecDifferent {
	_machine->set_program({
		0xc30a,		// ABCD -(A2), -(A1)
	});
	*_machine->ram_at(0x3000) = 0xa200;
	*_machine->ram_at(0x4000) = 0x1900;

	auto state = _machine->get_processor_state();
	state.address[1] = 0x3001;
	state.address[2] = 0x4001;
	state.status = Flag::Extend;
	_machine->set_processor_state(state);
	_machine->run_for_instructions(1);

	state = _machine->get_processor_state();
	XCTAssert(state.status & Flag::Carry);
	XCTAssert(state.status & Flag::Extend);
	XCTAssertEqual(state.address[1], 0x3000);
	XCTAssertEqual(state.address[2], 0x4000);
	XCTAssertEqual(*_machine->ram_at(0x3000), 0x2200);
	XCTAssertEqual(*_machine->ram_at(0x4000), 0x1900);
}

- (void)testABCDPredecSame {
	_machine->set_program({
		0xc309,		// ABCD -(A1), -(A1)
	});
	*_machine->ram_at(0x3000) = 0x19a2;

	auto state = _machine->get_processor_state();
	state.address[1] = 0x3002;
	state.status = Flag::Extend;
	_machine->set_processor_state(state);
	_machine->run_for_instructions(1);

	state = _machine->get_processor_state();
	XCTAssert(state.status & Flag::Carry);
	XCTAssert(state.status & Flag::Extend);
	XCTAssertEqual(state.address[1], 0x3000);
	XCTAssertEqual(*_machine->ram_at(0x3000), 0x22a2);
}

// MARK: NBCD

- (void)performNBCDd1:(uint32_t)d1 ccr:(uint8_t)ccr {
	_machine->set_program({
		0x4801		// NBCD D1
	});
	auto state = _machine->get_processor_state();
	state.status |= ccr;
	state.data[1] = d1;

	_machine->set_processor_state(state);
	_machine->run_for_instructions(1);

	XCTAssertEqual(6, _machine->get_cycle_count());
}

- (void)testNBCD_Dn {
	[self performNBCDd1:0x7a ccr:0];

	const auto state = _machine->get_processor_state();
	XCTAssertEqual(state.data[1], 0x20);
	XCTAssertEqual(state.status & Flag::ConditionCodes, Flag::Extend | Flag::Carry | Flag::Overflow);
}

- (void)testNBCD_Dn_extend {
	[self performNBCDd1:0x1234567a ccr:Flag::Extend | Flag::Zero];

	const auto state = _machine->get_processor_state();
	XCTAssertEqual(state.data[1], 0x1234561f);
	XCTAssertEqual(state.status & Flag::ConditionCodes, Flag::Extend | Flag::Carry | Flag::Overflow);
}

- (void)testNBCD_Dn_zero {
	[self performNBCDd1:0x12345600 ccr:Flag::Zero];

	const auto state = _machine->get_processor_state();
	XCTAssertEqual(state.data[1], 0x12345600);
	XCTAssertEqual(state.status & Flag::ConditionCodes, Flag::Zero);
}

- (void)testNBCD_Dn_negative {
	[self performNBCDd1:0x123456ff ccr:Flag::Extend | Flag::Zero];

	const auto state = _machine->get_processor_state();
	XCTAssertEqual(state.data[1], 0x1234569a);
	XCTAssertEqual(state.status & Flag::ConditionCodes, Flag::Extend | Flag::Carry | Flag::Negative);
}

- (void)testNBCD_Dn_XXXw {
	_machine->set_program({
		0x4838, 0x3000		// NBCD ($3000).w
	});
	*_machine->ram_at(0x3000) = 0x0100;

	_machine->run_for_instructions(1);

	const auto state = _machine->get_processor_state();
	XCTAssertEqual(16, _machine->get_cycle_count());
	XCTAssertEqual(*_machine->ram_at(0x3000), 0x9900);
	XCTAssertEqual(state.status & Flag::ConditionCodes, Flag::Extend | Flag::Carry | Flag::Negative);
}

// MARK: SBCD

- (void)performSBCDd1:(uint32_t)d1 d2:(uint32_t)d2 ccr:(uint8_t)ccr {
	_machine->set_program({
		0x8302		// SBCD D2, D1
	});
	auto state = _machine->get_processor_state();
	state.status |= ccr;
	state.data[1] = d1;
	state.data[2] = d2;

	_machine->set_processor_state(state);
	_machine->run_for_instructions(1);

	state = _machine->get_processor_state();
	XCTAssertEqual(6, _machine->get_cycle_count());
	XCTAssertEqual(state.data[2], d2);
}

- (void)testSBCD_Dn {
	[self performSBCDd1:0x12345689 d2:0xf745ff78 ccr:Flag::Zero];

	const auto state = _machine->get_processor_state();
	XCTAssertEqual(state.data[1], 0x12345611);
	XCTAssertEqual(state.status & Flag::ConditionCodes, 0);
}

- (void)testSBCD_Dn_zero {
	[self performSBCDd1:0x123456ff d2:0xf745ffff ccr:Flag::Zero];

	const auto state = _machine->get_processor_state();
	XCTAssertEqual(state.data[1], 0x12345600);
	XCTAssertEqual(state.status & Flag::ConditionCodes, Flag::Zero);
}

- (void)testSBCD_Dn_negative {
	[self performSBCDd1:0x12345634 d2:0xf745ff45 ccr:Flag::Extend];

	const auto state = _machine->get_processor_state();
	XCTAssertEqual(state.data[1], 0x12345688);
	XCTAssertEqual(state.status & Flag::ConditionCodes, Flag::Extend | Flag::Carry | Flag::Negative);
}

- (void)testSBCD_Dn_overflow {
	[self performSBCDd1:0x123456a9 d2:0xf745ffff ccr:Flag::Extend];

	const auto state = _machine->get_processor_state();
	XCTAssertEqual(state.data[1], 0x12345643);
	XCTAssertEqual(state.status & Flag::ConditionCodes, Flag::Extend | Flag::Carry | Flag::Overflow);
}

- (void)testSBCD_Dn_PreDec {
	_machine->set_program({
		0x830a		// SBCD -(A2), -(A1)
	});
	*_machine->ram_at(0x3000) = 0xa200;
	*_machine->ram_at(0x4000) = 0x1900;
	auto state = _machine->get_processor_state();
	state.address[1] = 0x3001;
	state.address[2] = 0x4001;
	state.status |= Flag::Extend;

	_machine->set_processor_state(state);
	_machine->run_for_instructions(1);

	state = _machine->get_processor_state();
	XCTAssertEqual(18, _machine->get_cycle_count());
	XCTAssertEqual(*_machine->ram_at(0x3000), 0x8200);
	XCTAssertEqual(*_machine->ram_at(0x4000), 0x1900);
	XCTAssertEqual(state.status & Flag::ConditionCodes, Flag::Negative);
}

@end
