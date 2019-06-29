//
//  68000RollShiftTests.m
//  Clock SignalTests
//
//  Created by Thomas Harte on 28/06/2019.
//
//  Largely ported from the tests of the Portable 68k Emulator.
//

#import <XCTest/XCTest.h>

#include "TestRunner68000.hpp"


@interface M68000RollShiftTests : XCTestCase
@end

@implementation M68000RollShiftTests {
	std::unique_ptr<RAM68000> _machine;
}

- (void)setUp {
    _machine.reset(new RAM68000());
}

- (void)tearDown {
	_machine.reset();
}

// MARK: ASL

- (void)testASLb_Dn_2 {
	_machine->set_program({
		0xe521		// ASL.B D2, D1
	});
	auto state = _machine->get_processor_state();
	state.data[1] = 0xce3dd567;
	state.data[2] = 2;

	_machine->set_processor_state(state);
	_machine->run_for_instructions(1);

	state = _machine->get_processor_state();
	XCTAssertEqual(state.data[1], 0xce3dd59c);
	XCTAssertEqual(state.data[2], 2);
	XCTAssertEqual(state.status & Flag::ConditionCodes, Flag::Extend | Flag::Negative | Flag::Overflow | Flag::Carry);
	XCTAssertEqual(10, _machine->get_cycle_count());
}

- (void)testASLb_Dn_105 {
	_machine->set_program({
		0xe521		// ASL.B D2, D1
	});
	auto state = _machine->get_processor_state();
	state.data[1] = 0xce3dd567;
	state.data[2] = 105;

	_machine->set_processor_state(state);
	_machine->run_for_instructions(1);

	state = _machine->get_processor_state();
	XCTAssertEqual(state.data[1], 0xce3dd500);
	XCTAssertEqual(state.data[2], 105);
	XCTAssertEqual(state.status & Flag::ConditionCodes, Flag::Overflow | Flag::Zero);
	XCTAssertEqual(88, _machine->get_cycle_count());
}

- (void)testASLw_Dn_0 {
	_machine->set_program({
		0xe561		// ASL.w D2, D1
	});
	auto state = _machine->get_processor_state();
	state.data[1] = 0xce3dd567;
	state.data[2] = 0;

	_machine->set_processor_state(state);
	_machine->run_for_instructions(1);

	state = _machine->get_processor_state();
	XCTAssertEqual(state.data[1], 0xce3dd567);
	XCTAssertEqual(state.data[2], 0);
	XCTAssertEqual(state.status & Flag::ConditionCodes, Flag::Negative);
	XCTAssertEqual(6, _machine->get_cycle_count());
}

- (void)testASLw_Dn_0b {
	_machine->set_program({
		0xe561		// ASL.w D2, D1
	});
	auto state = _machine->get_processor_state();
	state.data[1] = 0xce3dd567;
	state.data[2] = 0xb;

	_machine->set_processor_state(state);
	_machine->run_for_instructions(1);

	state = _machine->get_processor_state();
	XCTAssertEqual(state.data[1], 0xce3d3800);
	XCTAssertEqual(state.data[2], 0xb);
	XCTAssertEqual(state.status & Flag::ConditionCodes, Flag::Extend | Flag::Overflow | Flag::Carry);
	XCTAssertEqual(28, _machine->get_cycle_count());
}

- (void)testASLl_Dn {
	_machine->set_program({
		0xe5a1		// ASL.l D2, D1
	});
	auto state = _machine->get_processor_state();
	state.data[1] = 0xce3dd567;
	state.data[2] = 0x20;

	_machine->set_processor_state(state);
	_machine->run_for_instructions(1);

	state = _machine->get_processor_state();
	XCTAssertEqual(state.data[1], 0);
	XCTAssertEqual(state.data[2], 0x20);
	XCTAssertEqual(state.status & Flag::ConditionCodes, Flag::Extend | Flag::Overflow | Flag::Carry | Flag::Zero);
	XCTAssertEqual(72, _machine->get_cycle_count());
}

- (void)testASLl_Imm {
	_machine->set_program({
		0xe181		// ASL.l #8, D1
	});
	auto state = _machine->get_processor_state();
	state.data[1] = 0xce3dd567;
	state.data[2] = 0x20;

	_machine->set_processor_state(state);
	_machine->run_for_instructions(1);

	state = _machine->get_processor_state();
	XCTAssertEqual(state.data[1], 0x3dd56700);
	XCTAssertEqual(state.status & Flag::ConditionCodes, Flag::Overflow);
	XCTAssertEqual(24, _machine->get_cycle_count());
}

- (void)testASLw_XXXw_8ccc {
	_machine->set_program({
		0xe1f8, 0x3000		// ASL ($3000).w
	});
	*_machine->ram_at(0x3000) = 0x8ccc;

	_machine->run_for_instructions(1);

	const auto state = _machine->get_processor_state();
	XCTAssertEqual(*_machine->ram_at(0x3000), 0x1998);
	XCTAssertEqual(state.status & Flag::ConditionCodes, Flag::Overflow | Flag::Extend | Flag::Carry);
	XCTAssertEqual(16, _machine->get_cycle_count());
}

- (void)testASLw_XXXw_45780782 {
	_machine->set_program({
		0xe1f8, 0x3000		// ASL ($3000).w
	});
	*_machine->ram_at(0x3000) = 0x4578;
	*_machine->ram_at(0x3002) = 0x0782;

	_machine->run_for_instructions(1);

	const auto state = _machine->get_processor_state();
	XCTAssertEqual(*_machine->ram_at(0x3000), 0x8af0);
	XCTAssertEqual(*_machine->ram_at(0x3002), 0x0782);
	XCTAssertEqual(state.status & Flag::ConditionCodes, Flag::Overflow | Flag::Negative);
	XCTAssertEqual(16, _machine->get_cycle_count());
}

// MARK: ASR

- (void)testASRb_Dn_2 {
	_machine->set_program({
		0xe421		// ASR.B D2, D1
	});
	auto state = _machine->get_processor_state();
	state.data[1] = 0xce3dd567;
	state.data[2] = 2;

	_machine->set_processor_state(state);
	_machine->run_for_instructions(1);

	state = _machine->get_processor_state();
	XCTAssertEqual(state.data[1], 0xce3dd519);
	XCTAssertEqual(state.data[2], 2);
	XCTAssertEqual(state.status & Flag::ConditionCodes, Flag::Extend | Flag::Carry);
	XCTAssertEqual(10, _machine->get_cycle_count());
}

- (void)testASRb_Dn_105 {
	_machine->set_program({
		0xe421		// ASR.B D2, D1
	});
	auto state = _machine->get_processor_state();
	state.data[1] = 0xce3dd567;
	state.data[2] = 105;

	_machine->set_processor_state(state);
	_machine->run_for_instructions(1);

	state = _machine->get_processor_state();
	XCTAssertEqual(state.data[1], 0xce3dd500);
	XCTAssertEqual(state.data[2], 105);
	XCTAssertEqual(state.status & Flag::ConditionCodes, Flag::Zero);
	XCTAssertEqual(88, _machine->get_cycle_count());
}

- (void)testASRw_Dn_0 {
	_machine->set_program({
		0xe461		// ASR.w D2, D1
	});
	auto state = _machine->get_processor_state();
	state.data[1] = 0xce3dd567;
	state.data[2] = 0;

	_machine->set_processor_state(state);
	_machine->run_for_instructions(1);

	state = _machine->get_processor_state();
	XCTAssertEqual(state.data[1], 0xce3dd567);
	XCTAssertEqual(state.data[2], 0);
	XCTAssertEqual(state.status & Flag::ConditionCodes, Flag::Negative);
	XCTAssertEqual(6, _machine->get_cycle_count());
}

- (void)testASRw_Dn_0b {
	_machine->set_program({
		0xe461		// ASR.w D2, D1
	});
	auto state = _machine->get_processor_state();
	state.data[1] = 0xce3dd567;
	state.data[2] = 0xb;

	_machine->set_processor_state(state);
	_machine->run_for_instructions(1);

	state = _machine->get_processor_state();
	XCTAssertEqual(state.data[1], 0xce3dfffa);
	XCTAssertEqual(state.data[2], 0xb);
	XCTAssertEqual(state.status & Flag::ConditionCodes, Flag::Extend | Flag::Negative | Flag::Carry);
	XCTAssertEqual(28, _machine->get_cycle_count());
}

- (void)testASRl_Dn {
	_machine->set_program({
		0xe4a1		// ASR.l D2, D1
	});
	auto state = _machine->get_processor_state();
	state.data[1] = 0xce3dd567;
	state.data[2] = 0x20;

	_machine->set_processor_state(state);
	_machine->run_for_instructions(1);

	state = _machine->get_processor_state();
	XCTAssertEqual(state.data[1], 0xffffffff);
	XCTAssertEqual(state.data[2], 0x20);
	XCTAssertEqual(state.status & Flag::ConditionCodes, Flag::Extend | Flag::Negative | Flag::Carry);
	XCTAssertEqual(72, _machine->get_cycle_count());
}

- (void)testASRl_Imm {
	_machine->set_program({
		0xe081		// ASR.l #8, D1
	});
	auto state = _machine->get_processor_state();
	state.data[1] = 0xce3dd567;
	state.data[2] = 0x20;

	_machine->set_processor_state(state);
	_machine->run_for_instructions(1);

	state = _machine->get_processor_state();
	XCTAssertEqual(state.data[1], 0xffce3dd5);
	XCTAssertEqual(state.status & Flag::ConditionCodes, Flag::Negative);
	XCTAssertEqual(24, _machine->get_cycle_count());
}

- (void)testASRw_XXXw_8ccc {
	_machine->set_program({
		0xe0f8, 0x3000		// ASR ($3000).w
	});
	*_machine->ram_at(0x3000) = 0x8ccc;

	_machine->run_for_instructions(1);

	const auto state = _machine->get_processor_state();
	XCTAssertEqual(*_machine->ram_at(0x3000), 0xc666);
	XCTAssertEqual(state.status & Flag::ConditionCodes, Flag::Negative);
	XCTAssertEqual(16, _machine->get_cycle_count());
}

- (void)testASRw_XXXw_45780782 {
	_machine->set_program({
		0xe0f8, 0x3000		// ASR ($3000).w
	});
	*_machine->ram_at(0x3000) = 0x8578;
	*_machine->ram_at(0x3002) = 0x0782;

	_machine->run_for_instructions(1);

	const auto state = _machine->get_processor_state();
	XCTAssertEqual(*_machine->ram_at(0x3000), 0xc2bc);
	XCTAssertEqual(*_machine->ram_at(0x3002), 0x0782);
	XCTAssertEqual(state.status & Flag::ConditionCodes, Flag::Negative);
	XCTAssertEqual(16, _machine->get_cycle_count());
}

// MARK: LSL

- (void)testLSLb_Dn_2 {
	_machine->set_program({
		0xe529		// LSL.b D2, D1
	});
	auto state = _machine->get_processor_state();
	state.data[1] = 0xce3dd567;
	state.data[2] = 2;

	_machine->set_processor_state(state);
	_machine->run_for_instructions(1);

	state = _machine->get_processor_state();
	XCTAssertEqual(state.data[1], 0xce3dd59c);
	XCTAssertEqual(state.data[2], 2);
	XCTAssertEqual(state.status & Flag::ConditionCodes, Flag::Extend | Flag::Negative | Flag::Carry);
	XCTAssertEqual(10, _machine->get_cycle_count());
}

- (void)testLSLb_Dn_69 {
	_machine->set_program({
		0xe529		// LSL.b D2, D1
	});
	auto state = _machine->get_processor_state();
	state.data[1] = 0xce3dd567;
	state.data[2] = 0x69;

	_machine->set_processor_state(state);
	_machine->run_for_instructions(1);

	state = _machine->get_processor_state();
	XCTAssertEqual(state.data[1], 0xce3dd500);
	XCTAssertEqual(state.data[2], 0x69);
	XCTAssertEqual(state.status & Flag::ConditionCodes, Flag::Zero);
	XCTAssertEqual(88, _machine->get_cycle_count());
}

- (void)testLSLw_Dn_0 {
	_machine->set_program({
		0xe569		// LSL.w D2, D1
	});
	auto state = _machine->get_processor_state();
	state.data[1] = 0xce3dd567;
	state.data[2] = 0;

	_machine->set_processor_state(state);
	_machine->run_for_instructions(1);

	state = _machine->get_processor_state();
	XCTAssertEqual(state.data[1], 0xce3dd567);
	XCTAssertEqual(state.data[2], 0);
	XCTAssertEqual(state.status & Flag::ConditionCodes, Flag::Negative);
	XCTAssertEqual(6, _machine->get_cycle_count());
}

- (void)testLSLw_Dn_b {
	_machine->set_program({
		0xe569		// LSL.w D2, D1
	});
	auto state = _machine->get_processor_state();
	state.data[1] = 0xce3dd567;
	state.data[2] = 0xb;

	_machine->set_processor_state(state);
	_machine->run_for_instructions(1);

	state = _machine->get_processor_state();
	XCTAssertEqual(state.data[1], 0xce3d3800);
	XCTAssertEqual(state.data[2], 0xb);
	XCTAssertEqual(state.status & Flag::ConditionCodes, Flag::Extend | Flag::Carry);
	XCTAssertEqual(28, _machine->get_cycle_count());
}

- (void)testLSLl_Dn {
	_machine->set_program({
		0xe5a9		// LSL.l D2, D1
	});
	auto state = _machine->get_processor_state();
	state.data[1] = 0xce3dd567;
	state.data[2] = 0x20;

	_machine->set_processor_state(state);
	_machine->run_for_instructions(1);

	state = _machine->get_processor_state();
	XCTAssertEqual(state.data[1], 0);
	XCTAssertEqual(state.data[2], 0x20);
	XCTAssertEqual(state.status & Flag::ConditionCodes, Flag::Extend | Flag::Carry | Flag::Zero);
	XCTAssertEqual(72, _machine->get_cycle_count());
}

- (void)testLSLl_Imm {
	_machine->set_program({
		0xe189		// LSL.l #8, D1
	});
	auto state = _machine->get_processor_state();
	state.data[1] = 0xce3dd567;

	_machine->set_processor_state(state);
	_machine->run_for_instructions(1);

	state = _machine->get_processor_state();
	XCTAssertEqual(state.data[1], 0x3dd56700);
	XCTAssertEqual(state.status & Flag::ConditionCodes, 0);
	XCTAssertEqual(24, _machine->get_cycle_count());
}

- (void)testLSL_XXXw {
	_machine->set_program({
		0xe3f8, 0x3000		// LSL.l ($3000).w
	});
	*_machine->ram_at(0x3000) = 0x8ccc;

	_machine->run_for_instructions(1);

	const auto state = _machine->get_processor_state();
	XCTAssertEqual(*_machine->ram_at(0x3000), 0x1998);
	XCTAssertEqual(state.status & Flag::ConditionCodes, Flag::Carry | Flag::Extend);
	XCTAssertEqual(16, _machine->get_cycle_count());
}

// MARK: LSR

- (void)testLSRb_Dn_2 {
	_machine->set_program({
		0xe429		// LSR.b D2, D1
	});
	auto state = _machine->get_processor_state();
	state.data[1] = 0xce3dd567;
	state.data[2] = 2;

	_machine->set_processor_state(state);
	_machine->run_for_instructions(1);

	state = _machine->get_processor_state();
	XCTAssertEqual(state.data[1], 0xce3dd519);
	XCTAssertEqual(state.data[2], 2);
	XCTAssertEqual(state.status & Flag::ConditionCodes, Flag::Extend | Flag::Carry);
	XCTAssertEqual(10, _machine->get_cycle_count());
}

- (void)testLSRb_Dn_69 {
	_machine->set_program({
		0xe429		// LSR.b D2, D1
	});
	auto state = _machine->get_processor_state();
	state.data[1] = 0xce3dd567;
	state.data[2] = 0x69;

	_machine->set_processor_state(state);
	_machine->run_for_instructions(1);

	state = _machine->get_processor_state();
	XCTAssertEqual(state.data[1], 0xce3dd500);
	XCTAssertEqual(state.data[2], 0x69);
	XCTAssertEqual(state.status & Flag::ConditionCodes, Flag::Zero);
	XCTAssertEqual(88, _machine->get_cycle_count());
}

- (void)testLSRw_Dn_0 {
	_machine->set_program({
		0xe469		// LSR.w D2, D1
	});
	auto state = _machine->get_processor_state();
	state.data[1] = 0xce3dd567;
	state.data[2] = 0;

	_machine->set_processor_state(state);
	_machine->run_for_instructions(1);

	state = _machine->get_processor_state();
	XCTAssertEqual(state.data[1], 0xce3dd567);
	XCTAssertEqual(state.data[2], 0);
	XCTAssertEqual(state.status & Flag::ConditionCodes, Flag::Negative);
	XCTAssertEqual(6, _machine->get_cycle_count());
}

- (void)testLSRw_Dn_b {
	_machine->set_program({
		0xe469		// LSR.w D2, D1
	});
	auto state = _machine->get_processor_state();
	state.data[1] = 0xce3dd567;
	state.data[2] = 0xb;

	_machine->set_processor_state(state);
	_machine->run_for_instructions(1);

	state = _machine->get_processor_state();
	XCTAssertEqual(state.data[1], 0xce3d001a);
	XCTAssertEqual(state.data[2], 0xb);
	XCTAssertEqual(state.status & Flag::ConditionCodes, Flag::Extend | Flag::Carry);
	XCTAssertEqual(28, _machine->get_cycle_count());
}

- (void)testLSRl_Dn {
	_machine->set_program({
		0xe4a9		// LSR.l D2, D1
	});
	auto state = _machine->get_processor_state();
	state.data[1] = 0xce3dd567;
	state.data[2] = 0x20;

	_machine->set_processor_state(state);
	_machine->run_for_instructions(1);

	state = _machine->get_processor_state();
	XCTAssertEqual(state.data[1], 0);
	XCTAssertEqual(state.data[2], 0x20);
	XCTAssertEqual(state.status & Flag::ConditionCodes, Flag::Extend | Flag::Carry | Flag::Zero);
	XCTAssertEqual(72, _machine->get_cycle_count());
}

- (void)testLSRl_Imm {
	_machine->set_program({
		0xe089		// LSR.L #8, D1
	});
	auto state = _machine->get_processor_state();
	state.data[1] = 0xce3dd567;

	_machine->set_processor_state(state);
	_machine->run_for_instructions(1);

	state = _machine->get_processor_state();
	XCTAssertEqual(state.data[1], 0xce3dd5);
	XCTAssertEqual(state.status & Flag::ConditionCodes, 0);
	XCTAssertEqual(24, _machine->get_cycle_count());
}

- (void)testLSR_XXXw {
	_machine->set_program({
		0xe2f8, 0x3000		// LSR.l ($3000).w
	});
	*_machine->ram_at(0x3000) = 0x8ccc;

	_machine->run_for_instructions(1);

	const auto state = _machine->get_processor_state();
	XCTAssertEqual(*_machine->ram_at(0x3000), 0x4666);
	XCTAssertEqual(state.status & Flag::ConditionCodes, 0);
	XCTAssertEqual(16, _machine->get_cycle_count());
}

// MARK: ROL

- (void)testROLb_8 {
	_machine->set_program({
		0xe118		// ROL.B #8, D0
	});
	auto state = _machine->get_processor_state();
	state.data[0] = 0xce3dd567;

	_machine->set_processor_state(state);
	_machine->run_for_instructions(1);

	state = _machine->get_processor_state();
	XCTAssertEqual(state.data[0], 0xce3dd567);
	XCTAssertEqual(state.status & Flag::ConditionCodes, Flag::Carry);
	XCTAssertEqual(22, _machine->get_cycle_count());
}

- (void)testROLb_1 {
	_machine->set_program({
		0xe318		// ROL.B #1, D0
	});
	auto state = _machine->get_processor_state();
	state.data[0] = 0xce3dd567;

	_machine->set_processor_state(state);
	_machine->run_for_instructions(1);

	state = _machine->get_processor_state();
	XCTAssertEqual(state.data[0], 0xce3dd5ce);
	XCTAssertEqual(state.status & Flag::ConditionCodes, Flag::Negative);
	XCTAssertEqual(8, _machine->get_cycle_count());
}

- (void)testROLb_2 {
	_machine->set_program({
		0xe518		// ROL.B #2, D0
	});
	auto state = _machine->get_processor_state();
	state.data[0] = 0xce3dd567;
	state.status = Flag::ConditionCodes;

	_machine->set_processor_state(state);
	_machine->run_for_instructions(1);

	state = _machine->get_processor_state();
	XCTAssertEqual(state.data[0], 0xce3dd59d);
	XCTAssertEqual(state.status & Flag::ConditionCodes, Flag::Negative | Flag::Extend | Flag::Carry);
	XCTAssertEqual(10, _machine->get_cycle_count());
}

- (void)testROLb_7 {
	_machine->set_program({
		0xef18		// ROL.B #7, D0
	});
	auto state = _machine->get_processor_state();
	state.data[0] = 0xce3dd567;
	state.status = Flag::ConditionCodes;

	_machine->set_processor_state(state);
	_machine->run_for_instructions(1);

	state = _machine->get_processor_state();
	XCTAssertEqual(state.data[0], 0xce3dd5b3);
	XCTAssertEqual(state.status & Flag::ConditionCodes, Flag::Negative | Flag::Extend | Flag::Carry);
	XCTAssertEqual(20, _machine->get_cycle_count());
}

- (void)testROLw_8 {
	_machine->set_program({
		0xe158		// ROL.w #7, D0
	});
	auto state = _machine->get_processor_state();
	state.data[0] = 0xce3dd567;
	state.status = Flag::ConditionCodes;

	_machine->set_processor_state(state);
	_machine->run_for_instructions(1);

	state = _machine->get_processor_state();
	XCTAssertEqual(state.data[0], 0xce3d67d5);
	XCTAssertEqual(state.status & Flag::ConditionCodes, Flag::Extend | Flag::Carry);
	XCTAssertEqual(22, _machine->get_cycle_count());
}

- (void)testROLl_3 {
	_machine->set_program({
		0xe798		// ROL.l #3, D0
	});
	auto state = _machine->get_processor_state();
	state.data[0] = 0xce3dd567;
	state.status = Flag::ConditionCodes;

	_machine->set_processor_state(state);
	_machine->run_for_instructions(1);

	state = _machine->get_processor_state();
	XCTAssertEqual(state.data[0], 0x71eeab3e);
	XCTAssertEqual(state.status & Flag::ConditionCodes, Flag::Extend);
	XCTAssertEqual(14, _machine->get_cycle_count());
}

- (void)performROLw_D1D0d1:(uint32_t)d1 {
	_machine->set_program({
		0xe378		// ROL.l D1, D0
	});
	auto state = _machine->get_processor_state();
	state.data[0] = 0xce3dd567;
	state.data[1] = d1;
	state.status = Flag::ConditionCodes;

	_machine->set_processor_state(state);
	_machine->run_for_instructions(1);
}

- (void)testROLw_D1D0_20 {
	[self performROLw_D1D0d1:20];
	const auto state = _machine->get_processor_state();
	XCTAssertEqual(state.data[0], 0xce3d567d);
	XCTAssertEqual(state.data[1], 20);
	XCTAssertEqual(state.status & Flag::ConditionCodes, Flag::Extend | Flag::Carry);
	XCTAssertEqual(46, _machine->get_cycle_count());
}

- (void)testROLw_D1D0_36 {
	[self performROLw_D1D0d1:36];
	const auto state = _machine->get_processor_state();
	XCTAssertEqual(state.data[0], 0xce3d567d);
	XCTAssertEqual(state.data[1], 36);
	XCTAssertEqual(state.status & Flag::ConditionCodes, Flag::Extend | Flag::Carry);
	XCTAssertEqual(78, _machine->get_cycle_count());
}

- (void)testROLw_D1D0_0 {
	[self performROLw_D1D0d1:0];
	const auto state = _machine->get_processor_state();
	XCTAssertEqual(state.data[0], 0xce3dd567);
	XCTAssertEqual(state.data[1], 0);
	XCTAssertEqual(state.status & Flag::ConditionCodes, Flag::Extend | Flag::Negative);
	XCTAssertEqual(6, _machine->get_cycle_count());
}

- (void)testROLl_D1D0_200 {
	_machine->set_program({
		0xe3b8		// ROL.l D1, D0
	});
	auto state = _machine->get_processor_state();
	state.data[0] = 0xce3dd567;
	state.data[1] = 200;
	state.status = Flag::ConditionCodes;

	_machine->set_processor_state(state);
	_machine->run_for_instructions(1);

	state = _machine->get_processor_state();
	XCTAssertEqual(state.data[0], 0x3dd567ce);
	XCTAssertEqual(state.data[1], 200);
	XCTAssertEqual(state.status & Flag::ConditionCodes, Flag::Extend);
	XCTAssertEqual(24, _machine->get_cycle_count());
}

- (void)performROLw_3000:(uint16_t)storedValue {
	_machine->set_program({
		0xe7f8, 0x3000		// ROL.w ($3000).w
	});
	*_machine->ram_at(0x3000) = storedValue;

	_machine->run_for_instructions(1);

	XCTAssertEqual(16, _machine->get_cycle_count());
}

- (void)testROLm_d567 {
	[self performROLw_3000:0xd567];

	const auto state = _machine->get_processor_state();
	XCTAssertEqual(*_machine->ram_at(0x3000), 0xaacf);
	XCTAssertEqual(state.status & Flag::ConditionCodes, Flag::Negative | Flag::Carry);
}

- (void)testROLm_0 {
	[self performROLw_3000:0];

	const auto state = _machine->get_processor_state();
	XCTAssertEqual(*_machine->ram_at(0x3000), 0);
	XCTAssertEqual(state.status & Flag::ConditionCodes, Flag::Zero);
}

// MARK: ROR

- (void)performRORbIMM:(uint16_t)immediate {
	if(immediate == 8) immediate = 0;
	_machine->set_program({
		uint16_t(0xe018 | (immediate << 9))		// ROR.b #, D0
	});
	auto state = _machine->get_processor_state();
	state.data[0] = 0xce3dd599;

	_machine->set_processor_state(state);
	_machine->run_for_instructions(1);
}

- (void)testRORb_IMM_8 {
	[self performRORbIMM:8];

	const auto state = _machine->get_processor_state();
	XCTAssertEqual(state.data[0], 0xce3dd599);
	XCTAssertEqual(state.status & Flag::ConditionCodes, Flag::Carry | Flag::Negative);
	XCTAssertEqual(22, _machine->get_cycle_count());
}

- (void)testRORb_IMM_1 {
	[self performRORbIMM:1];

	const auto state = _machine->get_processor_state();
	XCTAssertEqual(state.data[0], 0xce3dd5cc);
	XCTAssertEqual(state.status & Flag::ConditionCodes, Flag::Carry | Flag::Negative);
	XCTAssertEqual(8, _machine->get_cycle_count());
}

- (void)testRORb_IMM_4 {
	[self performRORbIMM:4];

	const auto state = _machine->get_processor_state();
	XCTAssertEqual(state.data[0], 0xce3dd599);
	XCTAssertEqual(state.status & Flag::ConditionCodes, Flag::Carry | Flag::Negative);
	XCTAssertEqual(14, _machine->get_cycle_count());
}

- (void)testRORb_IMM_7 {
	[self performRORbIMM:7];

	const auto state = _machine->get_processor_state();
	XCTAssertEqual(state.data[0], 0xce3dd533);
	XCTAssertEqual(state.status & Flag::ConditionCodes, 0);
	XCTAssertEqual(20, _machine->get_cycle_count());
}

- (void)testRORw_IMM {
	_machine->set_program({
		0xec58		// ROR.w #6, D0
	});
	auto state = _machine->get_processor_state();
	state.data[0] = 0xce3dd599;

	_machine->set_processor_state(state);
	_machine->run_for_instructions(1);

	state = _machine->get_processor_state();
	XCTAssertEqual(state.data[0], 0xce3d6756);
	XCTAssertEqual(state.status & Flag::ConditionCodes, 0);
	XCTAssertEqual(18, _machine->get_cycle_count());
}

- (void)testRORl_IMM {
	_machine->set_program({
		0xea98		// ROR.l #5, D0
	});
	auto state = _machine->get_processor_state();
	state.data[0] = 0xce3dd599;

	_machine->set_processor_state(state);
	_machine->run_for_instructions(1);

	state = _machine->get_processor_state();
	XCTAssertEqual(state.data[0], 0xce71eeac);
	XCTAssertEqual(state.status & Flag::ConditionCodes, Flag::Carry | Flag::Negative);
	XCTAssertEqual(18, _machine->get_cycle_count());
}

- (void)testRORb_Dn {
	_machine->set_program({
		0xe238		// ROR.b D1, D0
	});
	auto state = _machine->get_processor_state();
	state.data[0] = 0xce3dd599;
	state.data[1] = 20;

	_machine->set_processor_state(state);
	_machine->run_for_instructions(1);

	state = _machine->get_processor_state();
	XCTAssertEqual(state.data[0], 0xce3dd599);
	XCTAssertEqual(state.data[1], 20);
	XCTAssertEqual(state.status & Flag::ConditionCodes, Flag::Carry | Flag::Negative);
	XCTAssertEqual(46, _machine->get_cycle_count());
}

- (void)testRORl_Dn {
	_machine->set_program({
		0xe2b8		// ROR.l D1, D0
	});
	auto state = _machine->get_processor_state();
	state.data[0] = 0xce3dd599;
	state.data[1] = 26;

	_machine->set_processor_state(state);
	_machine->run_for_instructions(1);

	state = _machine->get_processor_state();
	XCTAssertEqual(state.data[0], 0x8f756673);
	XCTAssertEqual(state.data[1], 26);
	XCTAssertEqual(state.status & Flag::ConditionCodes, Flag::Carry | Flag::Negative);
	XCTAssertEqual(60, _machine->get_cycle_count());
}

- (void)performRORw_3000:(uint16_t)storedValue {
	_machine->set_program({
		0xe6f8, 0x3000		// ROR.w ($3000).w
	});
	*_machine->ram_at(0x3000) = storedValue;

	_machine->run_for_instructions(1);

	XCTAssertEqual(16, _machine->get_cycle_count());
}

- (void)testRORm_d567 {
	[self performRORw_3000:0xd567];

	const auto state = _machine->get_processor_state();
	XCTAssertEqual(*_machine->ram_at(0x3000), 0xeab3);
	XCTAssertEqual(state.status & Flag::ConditionCodes, Flag::Negative | Flag::Carry);
}

- (void)testRORm_d560 {
	[self performRORw_3000:0xd560];

	const auto state = _machine->get_processor_state();
	XCTAssertEqual(*_machine->ram_at(0x3000), 0x6ab0);
	XCTAssertEqual(state.status & Flag::ConditionCodes, 0);
}

// MARK: ROXL

- (void)performROXLb_Dnccr:(uint16_t)ccr {
	_machine->set_program({
		0xe330		// ROXL.b D1, D0
	});
	auto state = _machine->get_processor_state();
	state.data[0] = 0xce3dd567;
	state.data[1] = 9;
	state.status |= ccr;

	_machine->set_processor_state(state);
	_machine->run_for_instructions(1);

	state = _machine->get_processor_state();
	XCTAssertEqual(24, _machine->get_cycle_count());
	XCTAssertEqual(state.data[0], 0xce3dd567);
	XCTAssertEqual(state.data[1], 9);
}

- (void)testROXLb_extend {
	[self performROXLb_Dnccr:Flag::ConditionCodes];

	const auto state = _machine->get_processor_state();
	XCTAssertEqual(state.status & Flag::ConditionCodes, Flag::Carry | Flag::Extend);
}

- (void)testROXLb {
	[self performROXLb_Dnccr:0];

	const auto state = _machine->get_processor_state();
	XCTAssertEqual(state.status & Flag::ConditionCodes, 0);
}

- (void)performROXLw_Dnd1:(uint32_t)d1 ccr:(uint16_t)ccr {
	_machine->set_program({
		0xe370		// ROXL.w D1, D0
	});
	auto state = _machine->get_processor_state();
	state.data[0] = 0xce3dd567;
	state.data[1] = d1;
	state.status |= ccr;

	_machine->set_processor_state(state);
	_machine->run_for_instructions(1);

	state = _machine->get_processor_state();
	XCTAssertEqual(state.data[1], d1);
}

- (void)testROXLw_17 {
	[self performROXLw_Dnd1:17 ccr:Flag::Carry];

	const auto state = _machine->get_processor_state();
	XCTAssertEqual(state.data[0], 0xce3dd567);
	XCTAssertEqual(state.status & Flag::ConditionCodes, Flag::Negative);
	XCTAssertEqual(40, _machine->get_cycle_count());
}

- (void)testROXLw_5 {
	[self performROXLw_Dnd1:5 ccr:Flag::Extend];

	const auto state = _machine->get_processor_state();
	XCTAssertEqual(state.data[0], 0xce3dacfd);
	XCTAssertEqual(state.status & Flag::ConditionCodes, Flag::Negative);
	XCTAssertEqual(16, _machine->get_cycle_count());
}

- (void)testROXLw_22 {
	[self performROXLw_Dnd1:22 ccr:Flag::Extend];

	const auto state = _machine->get_processor_state();
	XCTAssertEqual(state.data[0], 0xce3dacfd);
	XCTAssertEqual(state.status & Flag::ConditionCodes, Flag::Negative);
	XCTAssertEqual(50, _machine->get_cycle_count());
}

- (void)testROXLl_Dn {
	_machine->set_program({
		0xe3b0		// ROXL.l D1, D0
	});
	auto state = _machine->get_processor_state();
	state.data[0] = 0xce3dd567;
	state.data[1] = 33;
	state.status |= Flag::Extend;

	_machine->set_processor_state(state);
	_machine->run_for_instructions(1);

	state = _machine->get_processor_state();
	XCTAssertEqual(state.data[0], 0xce3dd567);
	XCTAssertEqual(state.status & Flag::ConditionCodes, Flag::Negative | Flag::Carry | Flag::Extend);
	XCTAssertEqual(74, _machine->get_cycle_count());
}

- (void)testROXLw_Imm {
	_machine->set_program({
		0xe950		// ROXL.w #4, D0
	});
	auto state = _machine->get_processor_state();
	state.data[0] = 0xce3d3600;
	state.status |= Flag::Extend;

	_machine->set_processor_state(state);
	_machine->run_for_instructions(1);

	state = _machine->get_processor_state();
	XCTAssertEqual(state.data[0], 0xce3d6009);
	XCTAssertEqual(state.status & Flag::ConditionCodes, Flag::Extend | Flag::Carry);
	XCTAssertEqual(14, _machine->get_cycle_count());
}

- (void)testROXLw_XXXw {
	_machine->set_program({
		0xe5f8, 0x3000		// ROXL.W ($3000).W
	});
	*_machine->ram_at(0x3000) = 0xd567;

	_machine->run_for_instructions(1);

	const auto state = _machine->get_processor_state();
	XCTAssertEqual(*_machine->ram_at(0x3000), 0xaace);
	XCTAssertEqual(state.status & Flag::ConditionCodes, Flag::Carry | Flag::Extend | Flag::Negative);
	XCTAssertEqual(16, _machine->get_cycle_count());
}

// MARK: ROXR

- (void)performROXRb_Dnccr:(uint16_t)ccr {
	_machine->set_program({
		0xe230		// ROXR.b D1, D0
	});
	auto state = _machine->get_processor_state();
	state.data[0] = 0xce3dd567;
	state.data[1] = 9;
	state.status |= ccr;

	_machine->set_processor_state(state);
	_machine->run_for_instructions(1);

	state = _machine->get_processor_state();
	XCTAssertEqual(24, _machine->get_cycle_count());
	XCTAssertEqual(state.data[0], 0xce3dd567);
	XCTAssertEqual(state.data[1], 9);
}

- (void)testROXRb_extend {
	[self performROXRb_Dnccr:Flag::ConditionCodes];

	const auto state = _machine->get_processor_state();
	XCTAssertEqual(state.status & Flag::ConditionCodes, Flag::Carry | Flag::Extend);
}

- (void)testROXRb {
	[self performROXRb_Dnccr:0];

	const auto state = _machine->get_processor_state();
	XCTAssertEqual(state.status & Flag::ConditionCodes, 0);
}

- (void)performROXRw_Dnd1:(uint32_t)d1 ccr:(uint16_t)ccr {
	_machine->set_program({
		0xe270		// ROXR.w D1, D0
	});
	auto state = _machine->get_processor_state();
	state.data[0] = 0xce3dd567;
	state.data[1] = d1;
	state.status |= ccr;

	_machine->set_processor_state(state);
	_machine->run_for_instructions(1);

	state = _machine->get_processor_state();
	XCTAssertEqual(state.data[1], d1);
}

- (void)testROXRw_17 {
	[self performROXRw_Dnd1:17 ccr:Flag::Carry];

	const auto state = _machine->get_processor_state();
	XCTAssertEqual(state.data[0], 0xce3dd567);
	XCTAssertEqual(state.status & Flag::ConditionCodes, Flag::Negative);
	XCTAssertEqual(40, _machine->get_cycle_count());
}

- (void)testROXRw_5 {
	[self performROXRw_Dnd1:5 ccr:Flag::Extend];

	const auto state = _machine->get_processor_state();
	XCTAssertEqual(state.data[0], 0xce3d7eab);
	XCTAssertEqual(state.status & Flag::ConditionCodes, 0);
	XCTAssertEqual(16, _machine->get_cycle_count());
}

- (void)testROXRw_22 {
	[self performROXRw_Dnd1:22 ccr:Flag::Extend];

	const auto state = _machine->get_processor_state();
	XCTAssertEqual(state.data[0], 0xce3d7eab);
	XCTAssertEqual(state.status & Flag::ConditionCodes, 0);
	XCTAssertEqual(50, _machine->get_cycle_count());
}

- (void)testROXRl {
	_machine->set_program({
		0xe890		// ROXR.L #4, D0
	});
	auto state = _machine->get_processor_state();
	state.data[0] = 0xce3d3600;
	state.status |= Flag::Extend;

	_machine->set_processor_state(state);
	_machine->run_for_instructions(1);

	state = _machine->get_processor_state();
	XCTAssertEqual(state.data[0], 0x1ce3d360);
	XCTAssertEqual(state.status & Flag::ConditionCodes, 0);
	XCTAssertEqual(16, _machine->get_cycle_count());
}

- (void)testROXRw_XXXw {
	_machine->set_program({
		0xe4f8, 0x3000		// ROXR.W ($3000).W
	});
	*_machine->ram_at(0x3000) = 0xd567;
	auto state = _machine->get_processor_state();
	state.status |= Flag::Extend;

	_machine->set_processor_state(state);
	_machine->run_for_instructions(1);

	state = _machine->get_processor_state();
	XCTAssertEqual(*_machine->ram_at(0x3000), 0xeab3);
	XCTAssertEqual(state.status & Flag::ConditionCodes, Flag::Carry | Flag::Extend | Flag::Negative);
	XCTAssertEqual(16, _machine->get_cycle_count());
}

@end
