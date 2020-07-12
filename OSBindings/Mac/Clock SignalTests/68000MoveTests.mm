//
//  68000ArithmeticTests.m
//  Clock SignalTests
//
//  Created by Thomas Harte on 28/06/2019.
//
//  Largely ported from the tests of the Portable 68k Emulator.
//

#import <XCTest/XCTest.h>

#include "TestRunner68000.hpp"

@interface M68000MoveTests : XCTestCase
@end

@implementation M68000MoveTests {
	std::unique_ptr<RAM68000> _machine;
}

- (void)setUp {
	_machine = std::make_unique<RAM68000>();
}

- (void)tearDown {
	_machine.reset();
}

// MARK: CLR

- (void)testCLRw {
	_machine->set_program({
		0x4244		// CLR.w D4
	});
	auto state = _machine->get_processor_state();
	state.data[4] = 0x9853abcd;
	state.status |= Flag::Extend | Flag::Negative | Flag::Overflow | Flag::Carry;

	_machine->set_processor_state(state);
	_machine->run_for_instructions(1);

	state = _machine->get_processor_state();
	XCTAssertEqual(state.data[4], 0x98530000);
	XCTAssertEqual(state.status & Flag::ConditionCodes, Flag::Extend | Flag::Zero);
	XCTAssertEqual(4, _machine->get_cycle_count());
}

- (void)testCLRl_Dn {
	_machine->set_program({
		0x4284		// CLR.l D4
	});
	auto state = _machine->get_processor_state();
	state.data[4] = 0x9853abcd;
	state.status |= Flag::Extend | Flag::Negative | Flag::Overflow | Flag::Carry;

	_machine->set_processor_state(state);
	_machine->run_for_instructions(1);

	state = _machine->get_processor_state();
	XCTAssertEqual(state.data[4], 0x0);
	XCTAssertEqual(state.status & Flag::ConditionCodes, Flag::Extend | Flag::Zero);
	XCTAssertEqual(6, _machine->get_cycle_count());
}

- (void)testCLRl_XXXl {
	_machine->set_program({
		0x42b9, 0x0001, 0x86a0		// CLR.l ($186a0).l
	});
	*_machine->ram_at(0x186a0) = 0x9853;
	*_machine->ram_at(0x186a2) = 0xabcd;
	auto state = _machine->get_processor_state();
	state.status |= Flag::Extend | Flag::Negative | Flag::Overflow | Flag::Carry;

	_machine->set_processor_state(state);
	_machine->run_for_instructions(1);

	state = _machine->get_processor_state();
	XCTAssertEqual(*_machine->ram_at(0x186a0), 0x0);
	XCTAssertEqual(*_machine->ram_at(0x186a2), 0x0);
	XCTAssertEqual(state.status & Flag::ConditionCodes, Flag::Extend | Flag::Zero);
	XCTAssertEqual(28, _machine->get_cycle_count());
}

- (void)testCLRb_XXXl {
	_machine->set_program({
		0x4239, 0x0001, 0x86a0		// CLR.b ($186a0).l
	});
	*_machine->ram_at(0x186a0) = 0x9853;
	*_machine->ram_at(0x186a2) = 0xabcd;
	auto state = _machine->get_processor_state();
	state.status |= Flag::Extend | Flag::Negative | Flag::Overflow | Flag::Carry;

	_machine->set_processor_state(state);
	_machine->run_for_instructions(1);

	state = _machine->get_processor_state();
	XCTAssertEqual(*_machine->ram_at(0x186a0), 0x0053);
	XCTAssertEqual(*_machine->ram_at(0x186a2), 0xabcd);
	XCTAssertEqual(state.status & Flag::ConditionCodes, Flag::Extend | Flag::Zero);
	XCTAssertEqual(20, _machine->get_cycle_count());
}

// MARK: EXG

- (void)testEXG_D1D2 {
	_machine->set_program({
		0xc342		// EXG D1, D2
	});

	auto state = _machine->get_processor_state();
	state.data[1] = 0x11111111;
	state.data[2] = 0x22222222;

	_machine->set_processor_state(state);
	_machine->run_for_instructions(1);

	state = _machine->get_processor_state();
	XCTAssertEqual(state.data[1], 0x22222222);
	XCTAssertEqual(state.data[2], 0x11111111);
	XCTAssertEqual(6, _machine->get_cycle_count());
}

- (void)testEXG_A1A2 {
	_machine->set_program({
		0xc34a		// EXG A1, A2
	});

	auto state = _machine->get_processor_state();
	state.address[1] = 0x11111111;
	state.address[2] = 0x22222222;

	_machine->set_processor_state(state);
	_machine->run_for_instructions(1);

	state = _machine->get_processor_state();
	XCTAssertEqual(state.address[1], 0x22222222);
	XCTAssertEqual(state.address[2], 0x11111111);
	XCTAssertEqual(6, _machine->get_cycle_count());
}

- (void)testEXG_A1D1 {
	_machine->set_program({
		0xc389		// EXG A1, D1
	});

	auto state = _machine->get_processor_state();
	state.data[1] = 0x11111111;
	state.address[1] = 0x22222222;

	_machine->set_processor_state(state);
	_machine->run_for_instructions(1);

	state = _machine->get_processor_state();
	XCTAssertEqual(state.data[1], 0x22222222);
	XCTAssertEqual(state.address[1], 0x11111111);
	XCTAssertEqual(6, _machine->get_cycle_count());
}

// MARK: LEA

- (void)testLEA_w {
	_machine->set_program({
		0x41f8, 0x000c		// LEA ($12).w, A0
	});

	_machine->run_for_instructions(1);

	const auto state = _machine->get_processor_state();
	XCTAssertEqual(state.address[0], 0xc);
	XCTAssertEqual(8, _machine->get_cycle_count());
}

- (void)testLEA_l {
	_machine->set_program({
		0x41f9, 0x000c, 0x000d		// LEA ($c000d).w, A0
	});

	_machine->run_for_instructions(1);

	const auto state = _machine->get_processor_state();
	XCTAssertEqual(state.address[0], 0xc000d);
	XCTAssertEqual(12, _machine->get_cycle_count());
}

- (void)testLEA_An {
	_machine->set_program({
		0x43d2,		// LEA (A2), A1
	});

	auto state = _machine->get_processor_state();
	state.address[2] = 0xc000d;

	_machine->set_processor_state(state);
	_machine->run_for_instructions(1);

	state = _machine->get_processor_state();
	XCTAssertEqual(state.address[1], 0xc000d);
	XCTAssertEqual(state.address[2], 0xc000d);
	XCTAssertEqual(4, _machine->get_cycle_count());
}

- (void)testLEA_dAn {
	_machine->set_program({
		0x43ea, 0xffff		// LEA (-1,A2), A1
	});

	auto state = _machine->get_processor_state();
	state.address[2] = 0xc000d;

	_machine->set_processor_state(state);
	_machine->run_for_instructions(1);

	state = _machine->get_processor_state();
	XCTAssertEqual(state.address[1], 0xc000c);
	XCTAssertEqual(state.address[2], 0xc000d);
	XCTAssertEqual(8, _machine->get_cycle_count());
}

- (void)testLEA_dAnDnw {
	_machine->set_program({
		0x43f2, 0x7002		// LEA (2,A2,D7.W), A1
	});

	auto state = _machine->get_processor_state();
	state.address[2] = 0xc000d;
	state.data[7] = 0x10000022;

	_machine->set_processor_state(state);
	_machine->run_for_instructions(1);

	state = _machine->get_processor_state();
	XCTAssertEqual(state.address[1], 0xc0031);
	XCTAssertEqual(state.address[2], 0xc000d);
	XCTAssertEqual(state.data[7], 0x10000022);
	XCTAssertEqual(12, _machine->get_cycle_count());
}

- (void)testLEA_dAnDnl {
	_machine->set_program({
		0x43f2, 0x7802		// LEA (2,A2,D7.l), A1
	});

	auto state = _machine->get_processor_state();
	state.address[2] = 0xc000d;
	state.data[7] = 0x10000022;

	_machine->set_processor_state(state);
	_machine->run_for_instructions(1);

	state = _machine->get_processor_state();
	XCTAssertEqual(state.address[1], 0x100c0031);
	XCTAssertEqual(state.address[2], 0xc000d);
	XCTAssertEqual(state.data[7], 0x10000022);
	XCTAssertEqual(12, _machine->get_cycle_count());
}

- (void)testLEA_dPC {
	_machine->set_program({
		0x43fa, 0xeff8		// LEA	(-6,PC), A1
	});

	auto state = _machine->get_processor_state();
	state.address[2] = 0xc000d;

	_machine->set_processor_state(state);
	_machine->run_for_instructions(1);

	state = _machine->get_processor_state();
	XCTAssertEqual(state.address[1], 0xFFFFFFFA);
	XCTAssertEqual(8, _machine->get_cycle_count());
}

- (void)testLEA_dPCDn {
	_machine->set_program({
		0x43fb, 0x30fe		// LEA (-6,PC,D3), A1
	});

	auto state = _machine->get_processor_state();
	state.data[3] = 0x2;

	_machine->set_processor_state(state);
	_machine->run_for_instructions(1);

	state = _machine->get_processor_state();
	XCTAssertEqual(state.address[1], 0x1002);
	XCTAssertEqual(state.data[3], 0x2);
	XCTAssertEqual(12, _machine->get_cycle_count());
}

// MARK: LINK

- (void)testLINKA1_5 {
	_machine->set_program({
		0x4e51, 0x0005		// LINK a1, #5
	});
	auto state = _machine->get_processor_state();
	state.address[1] = 0x11111111;
	_machine->set_initial_stack_pointer(0x22222222);

	_machine->set_processor_state(state);
	_machine->run_for_instructions(1);

	state = _machine->get_processor_state();
	XCTAssertEqual(state.address[1], 0x2222221e);
	XCTAssertEqual(state.supervisor_stack_pointer, 0x22222223);
	XCTAssertEqual(*_machine->ram_at(0x2222221e), 0x1111);
	XCTAssertEqual(*_machine->ram_at(0x22222220), 0x1111);
	XCTAssertEqual(16, _machine->get_cycle_count());
}

- (void)testLINKA7_5 {
	_machine->set_program({
		0x4e57, 0x0005		// LINK a7, #5
	});
	_machine->set_initial_stack_pointer(0x22222222);

	_machine->run_for_instructions(1);

	const auto state = _machine->get_processor_state();
	XCTAssertEqual(state.supervisor_stack_pointer, 0x22222223);
	XCTAssertEqual(*_machine->ram_at(0x2222221e), 0x2222);
	XCTAssertEqual(*_machine->ram_at(0x22222220), 0x221e);
	XCTAssertEqual(16, _machine->get_cycle_count());
}

- (void)testLINKA1_8000 {
	_machine->set_program({
		0x4e51, 0x8000		// LINK a1, #$8000
	});
	auto state = _machine->get_processor_state();
	state.address[1] = 0x11111111;
	_machine->set_initial_stack_pointer(0x22222222);

	_machine->set_processor_state(state);
	_machine->run_for_instructions(1);

	state = _machine->get_processor_state();
	XCTAssertEqual(state.address[1], 0x2222221e);
	XCTAssertEqual(state.supervisor_stack_pointer, 0x2221a21e);
	XCTAssertEqual(*_machine->ram_at(0x2222221e), 0x1111);
	XCTAssertEqual(*_machine->ram_at(0x22222220), 0x1111);
	XCTAssertEqual(16, _machine->get_cycle_count());
}

// MARK: MOVEM

- (void)testMOVEMl_fromD0D1 {
	_machine->set_program({
		0x48e1, 0xc000		// MOVEM.L D0-D1, -(A1)
	});
	auto state = _machine->get_processor_state();
	state.address[1] = 0x3000;
	state.data[0] = 0x12345678;
	state.data[1] = 0x87654321;

	_machine->set_processor_state(state);
	_machine->run_for_instructions(1);

	state = _machine->get_processor_state();
	XCTAssertEqual(state.address[1], 0x2ff8);
	XCTAssertEqual(state.data[0], 0x12345678);
	XCTAssertEqual(state.data[1], 0x87654321);
	XCTAssertEqual(*_machine->ram_at(0x2ff8), 0x1234);
	XCTAssertEqual(*_machine->ram_at(0x2ffa), 0x5678);
	XCTAssertEqual(*_machine->ram_at(0x2ffc), 0x8765);
	XCTAssertEqual(*_machine->ram_at(0x2ffe), 0x4321);
	XCTAssertEqual(24, _machine->get_cycle_count());
}

- (void)testMOVEMl_fromD0D1A1 {
	_machine->set_program({
		0x48e1, 0xc040		// MOVEM.L D0-D1/A1, -(A1)
	});
	auto state = _machine->get_processor_state();
	state.address[1] = 0x3000;
	state.data[0] = 0x12345678;
	state.data[1] = 0x87654321;

	_machine->set_processor_state(state);
	_machine->run_for_instructions(1);

	state = _machine->get_processor_state();
	XCTAssertEqual(state.address[1], 0x2ff4);
	XCTAssertEqual(state.data[0], 0x12345678);
	XCTAssertEqual(state.data[1], 0x87654321);
	XCTAssertEqual(*_machine->ram_at(0x2ff4), 0x1234);
	XCTAssertEqual(*_machine->ram_at(0x2ff6), 0x5678);
	XCTAssertEqual(*_machine->ram_at(0x2ff8), 0x8765);
	XCTAssertEqual(*_machine->ram_at(0x2ffa), 0x4321);
	XCTAssertEqual(*_machine->ram_at(0x2ffc), 0x0000);
	XCTAssertEqual(*_machine->ram_at(0x2ffe), 0x3000);
	XCTAssertEqual(32, _machine->get_cycle_count());
}

- (void)testMOVEMl_fromEverything {
	_machine->set_program({
		0x48e4, 0xffff		// MOVEM.L D0-D7/A0-A7, -(A4)
	});
	auto state = _machine->get_processor_state();
	for(int c = 0; c < 8; ++c)
		state.data[c] = (c+1) * 0x11111111;
	for(int c = 0; c < 7; ++c)
		state.address[c] = ((c < 4) ? (c + 9) : (c + 8)) * 0x11111111;
	state.address[4] = 0x4000;
	_machine->set_initial_stack_pointer(0xffffffff);

	_machine->set_processor_state(state);
	_machine->run_for_instructions(1);

	state = _machine->get_processor_state();
	XCTAssertEqual(state.address[4], 0x3fc0);

	const uint32_t expected_values[] = {
		0xffffffff, 0xeeeeeeee, 0xdddddddd, 0x00004000,
		0xcccccccc, 0xbbbbbbbb, 0xaaaaaaaa, 0x99999999,
		0x88888888, 0x77777777, 0x66666666, 0x55555555,
		0x44444444, 0x33333333, 0x22222222, 0x11111111,
	};
	const uint32_t *expected_value = expected_values;
	for(uint32_t address = 0x3ffc; address <= 0x3fc0; address += 4) {
		XCTAssertEqual(*_machine->ram_at(address), (*expected_value >> 16));
		XCTAssertEqual(*_machine->ram_at(address + 2), (*expected_value & 0xffff));
		++expected_value;
	}

	XCTAssertEqual(136, _machine->get_cycle_count());
}

- (void)testMOVEMw_fromD4 {
	_machine->set_program({
		0x48a4, 0x0800		// MOVEM.W D4, -(A4)
	});
	auto state = _machine->get_processor_state();
	state.address[4] = 0x4000;
	state.data[4] = 0x111a1111;
	state.data[0] = 0xffffffff;

	_machine->set_processor_state(state);
	_machine->run_for_instructions(1);

	state = _machine->get_processor_state();

	XCTAssertEqual(state.address[4], 0x3ffe);
	XCTAssertEqual(state.data[0], 0xffffffff);
	XCTAssertEqual(state.data[4], 0x111a1111);

	XCTAssertEqual(*_machine->ram_at(0x3ffe), 0x1111);
	XCTAssertEqual(*_machine->ram_at(0x3ffc), 0x0000);

	XCTAssertEqual(12, _machine->get_cycle_count());
}

// TODO: port MOVEM.W D4/D0, -(A4), which tests bus error response.

- (void)testMOVEMl_toD1D2A1A2 {
	_machine->set_program({
		0x4cd9, 0x0606		// MOVEM.l (A1)+, D1-D2/A1-A2
	});
	auto state = _machine->get_processor_state();
	state.address[1] = 0x4000;
	*_machine->ram_at(0x4000) = 0x1111;
	*_machine->ram_at(0x4002) = 0x1111;
	*_machine->ram_at(0x4004) = 0x2222;
	*_machine->ram_at(0x4006) = 0x2222;
	*_machine->ram_at(0x400c) = 0x3333;
	*_machine->ram_at(0x400e) = 0x3333;

	_machine->set_processor_state(state);
	_machine->run_for_instructions(1);

	state = _machine->get_processor_state();

	XCTAssertEqual(state.data[1], 0x11111111);
	XCTAssertEqual(state.data[2], 0x22222222);
	XCTAssertEqual(state.address[1], 0x4010);
	XCTAssertEqual(state.address[2], 0x33333333);

	XCTAssertEqual(44, _machine->get_cycle_count());
}

- (void)testMOVEMw_signExtend {
	_machine->set_program({
		0x4c99, 0x0002		// MOVEM.w (A1)+, D1
	});
	auto state = _machine->get_processor_state();
	state.address[1] = 0x4000;
	*_machine->ram_at(0x4000) = 0x8000;

	_machine->set_processor_state(state);
	_machine->run_for_instructions(1);

	state = _machine->get_processor_state();

	XCTAssertEqual(state.data[1], 0xffff8000);
	XCTAssertEqual(state.address[1], 0x4002);

	XCTAssertEqual(16, _machine->get_cycle_count());
}

- (void)testMOVEMw_fromIndirect {
	_machine->set_program({
		0x4c91, 0x0206		// MOVEM.w (A1), A1/D1-D2
	});
	auto state = _machine->get_processor_state();
	state.address[1] = 0x4000;
	state.data[2] = 0xffffffff;
	*_machine->ram_at(0x4000) = 0x8000;
	*_machine->ram_at(0x4002) = 0x2222;
	*_machine->ram_at(0x4004) = 0x3333;

	_machine->set_processor_state(state);
	_machine->run_for_instructions(1);

	state = _machine->get_processor_state();

	XCTAssertEqual(state.data[1], 0xffff8000);
	XCTAssertEqual(state.data[2], 0x00002222);
	XCTAssertEqual(state.address[1], 0x3333);

	XCTAssertEqual(24, _machine->get_cycle_count());
}

- (void)testMOVEMw_toIndirect {
	_machine->set_program({
		0x4891, 0x0206		// MOVEM.w A1/D1-D2, (A1)
	});
	auto state = _machine->get_processor_state();
	state.address[1] = 0x4000;
	state.data[1] = 0x11111111;
	state.data[2] = 0x22222222;

	_machine->set_processor_state(state);
	_machine->run_for_instructions(1);

	state = _machine->get_processor_state();

	XCTAssertEqual(*_machine->ram_at(0x4000), 0x1111);
	XCTAssertEqual(*_machine->ram_at(0x4002), 0x2222);
	XCTAssertEqual(*_machine->ram_at(0x4004), 0x4000);
	XCTAssertEqual(state.address[1], 0x4000);

	XCTAssertEqual(20, _machine->get_cycle_count());
}

// MARK: MOVE

- (void)testMOVEb_DnDn {
	_machine->set_program({
		0x1401		// MOVE.b D1, D2
	});
	auto state = _machine->get_processor_state();
	state.data[1] = 0x12345678;

	_machine->set_processor_state(state);
	_machine->run_for_instructions(1);

	state = _machine->get_processor_state();
	XCTAssertEqual(state.data[1], 0x12345678);
	XCTAssertEqual(state.data[2], 0x00000078);
	XCTAssertEqual(state.status & Flag::ConditionCodes, 0);
	XCTAssertEqual(4, _machine->get_cycle_count());
}

- (void)testMOVEl_ImmDn {
	_machine->set_program({
		0x243c, 0x8090, 0xfea1		// MOVE.l #$8090fea1, D2
	});

	_machine->run_for_instructions(1);

	const auto state = _machine->get_processor_state();
	XCTAssertEqual(state.data[2], 0x8090fea1);
	XCTAssertEqual(state.status & Flag::ConditionCodes, Flag::Negative);
	XCTAssertEqual(12, _machine->get_cycle_count());
}

- (void)testMOVEs_ImmInd {
	_machine->set_program({
		0x34bc, 0x0000		// MOVE #$0, (A2)
	});
	auto state = _machine->get_processor_state();
	state.address[2] = 0x3000;
	*_machine->ram_at(0x3000) = 0x1234;

	_machine->set_processor_state(state);
	_machine->run_for_instructions(1);

	state = _machine->get_processor_state();
	XCTAssertEqual(state.address[2], 0x3000);
	XCTAssertEqual(state.status & Flag::ConditionCodes, Flag::Zero);
	XCTAssertEqual(*_machine->ram_at(0x3000), 0);
	XCTAssertEqual(12, _machine->get_cycle_count());
}

- (void)testMOVEl_PostIncPostInc {
	_machine->set_program({
		0x24da		// MOVE.l (A2)+, (A2)+
	});
	auto state = _machine->get_processor_state();
	state.address[2] = 0x3000;
	state.status = Flag::Negative;
	*_machine->ram_at(0x3000) = 0xaaaa;
	*_machine->ram_at(0x3002) = 0xbbbb;

	_machine->set_processor_state(state);
	_machine->run_for_instructions(1);

	state = _machine->get_processor_state();
	XCTAssertEqual(state.address[2], 0x3008);
	XCTAssertEqual(state.status & Flag::ConditionCodes, Flag::Negative);
	XCTAssertEqual(*_machine->ram_at(0x3000), 0xaaaa);
	XCTAssertEqual(*_machine->ram_at(0x3002), 0xbbbb);
	XCTAssertEqual(*_machine->ram_at(0x3004), 0xaaaa);
	XCTAssertEqual(*_machine->ram_at(0x3006), 0xbbbb);
	XCTAssertEqual(20, _machine->get_cycle_count());
}

- (void)testMOVEl_PostIncPreDec {
	_machine->set_program({
		0x251a		// MOVE.l (A2)+, -(A2)
	});
	auto state = _machine->get_processor_state();
	state.address[2] = 0x3000;
	state.status = Flag::Negative;
	*_machine->ram_at(0x3000) = 0xaaaa;
	*_machine->ram_at(0x3002) = 0xbbbb;

	_machine->set_processor_state(state);
	_machine->run_for_instructions(1);

	state = _machine->get_processor_state();
	XCTAssertEqual(state.address[2], 0x3000);
	XCTAssertEqual(state.status & Flag::ConditionCodes, Flag::Negative);
	XCTAssertEqual(*_machine->ram_at(0x3000), 0xaaaa);
	XCTAssertEqual(*_machine->ram_at(0x3002), 0xbbbb);
	XCTAssertEqual(*_machine->ram_at(0x3004), 0);
	XCTAssertEqual(*_machine->ram_at(0x3006), 0);
	XCTAssertEqual(20, _machine->get_cycle_count());
}

- (void)testMOVEl_PreDecD16An {
	_machine->set_program({
		0x25a2, 0x1004		// MOVE.L -(A2), 4(A2,D1)
	});
	auto state = _machine->get_processor_state();
	state.address[2] = 0x3004;
	state.data[1] = 0;
	state.status = Flag::Negative;
	*_machine->ram_at(0x3000) = 0xaaaa;
	*_machine->ram_at(0x3002) = 0xbbbb;

	_machine->set_processor_state(state);
	_machine->run_for_instructions(1);

	state = _machine->get_processor_state();
	XCTAssertEqual(state.address[2], 0x3000);
	XCTAssertEqual(state.data[1], 0);
	XCTAssertEqual(state.status & Flag::ConditionCodes, Flag::Negative);
	XCTAssertEqual(*_machine->ram_at(0x3000), 0xaaaa);
	XCTAssertEqual(*_machine->ram_at(0x3002), 0xbbbb);
	XCTAssertEqual(*_machine->ram_at(0x3004), 0xaaaa);
	XCTAssertEqual(*_machine->ram_at(0x3006), 0xbbbb);
	XCTAssertEqual(28, _machine->get_cycle_count());
}

- (void)testMOVEl_DnXXXl {
	_machine->set_program({
		0x33c1, 0x0000, 0x3000		// MOVE.W D1, ($3000).L
	});
	auto state = _machine->get_processor_state();
	state.data[1] = 0x5678;

	_machine->set_processor_state(state);
	_machine->run_for_instructions(1);

	state = _machine->get_processor_state();
	XCTAssertEqual(state.data[1], 0x5678);
	XCTAssertEqual(state.status & Flag::ConditionCodes, 0);
	XCTAssertEqual(*_machine->ram_at(0x3000), 0x5678);
	XCTAssertEqual(*_machine->ram_at(0x3002), 0);
	XCTAssertEqual(16, _machine->get_cycle_count());
}

- (void)testMOVEl_XXXlXXXl {
	_machine->set_program({
		0x23f9, 0x0000, 0x3000, 0x0000, 0x3004		// MOVE.L ($3000).L, ($3004).L
	});
	*_machine->ram_at(0x3002) = 0xeeee;

	_machine->run_for_instructions(1);

	const auto state = _machine->get_processor_state();
	XCTAssertEqual(state.status & Flag::ConditionCodes, 0);	/* !! 8 !! */
	XCTAssertEqual(*_machine->ram_at(0x3002), 0xeeee);
	XCTAssertEqual(*_machine->ram_at(0x3006), 0xeeee);
	XCTAssertEqual(36, _machine->get_cycle_count());
}

// MARK: MOVEA

- (void)testMOVEAl_An {
	_machine->set_program({
		0x244a		// MOVEA.l A2, A2
	});
	auto state = _machine->get_processor_state();
	state.address[2] = 0xffffffff;

	_machine->set_processor_state(state);
	_machine->run_for_instructions(1);

	state = _machine->get_processor_state();
	XCTAssertEqual(state.address[2], 0xffffffff);
	XCTAssertEqual(state.status & Flag::ConditionCodes, 0);
	XCTAssertEqual(4, _machine->get_cycle_count());
}

- (void)testMOVEAw_Dn_positive {
	_machine->set_program({
		0x3442		// MOVEA.w D2, A2
	});
	auto state = _machine->get_processor_state();
	state.address[2] = 0xffffffff;
	state.data[2] = 0x12345678;

	_machine->set_processor_state(state);
	_machine->run_for_instructions(1);

	state = _machine->get_processor_state();
	XCTAssertEqual(state.address[2], 0x00005678);
	XCTAssertEqual(state.data[2], 0x12345678);
	XCTAssertEqual(state.status & Flag::ConditionCodes, 0);
	XCTAssertEqual(4, _machine->get_cycle_count());
}

- (void)testMOVEAw_Dn_negative {
	_machine->set_program({
		0x3442		// MOVEA.w D2, A2
	});
	auto state = _machine->get_processor_state();
	state.address[2] = 0xffffffff;
	state.data[2] = 0x12348756;

	_machine->set_processor_state(state);
	_machine->run_for_instructions(1);

	state = _machine->get_processor_state();
	XCTAssertEqual(state.address[2], 0xffff8756);
	XCTAssertEqual(state.data[2], 0x12348756);
	XCTAssertEqual(state.status & Flag::ConditionCodes, 0);
	XCTAssertEqual(4, _machine->get_cycle_count());
}

- (void)testMOVEAl_Imm {
	_machine->set_program({
		0x247c, 0x0000, 0x0001		// MOVEA.L #$1, A2
	});
	auto state = _machine->get_processor_state();
	state.address[2] = 0xffffffff;

	_machine->set_processor_state(state);
	_machine->run_for_instructions(1);

	state = _machine->get_processor_state();
	XCTAssertEqual(state.address[2], 1);
	XCTAssertEqual(state.status & Flag::ConditionCodes, 0);
	XCTAssertEqual(12, _machine->get_cycle_count());
}

// MARK: MOVEP

- (void)testMOVEPw_toDn {
	_machine->set_program({
		0x030e, 0x0004		// MOVEP.w 4(A6), D1
	});
	auto state = _machine->get_processor_state();
	state.address[6] = 0x3000;
	*_machine->ram_at(0x3004) = 0x1200;
	*_machine->ram_at(0x3006) = 0x3400;

	_machine->set_processor_state(state);
	_machine->run_for_instructions(1);

	state = _machine->get_processor_state();
	XCTAssertEqual(state.address[6], 0x3000);
	XCTAssertEqual(state.data[1], 0x1234);
	XCTAssertEqual(state.status & Flag::ConditionCodes, 0);
	XCTAssertEqual(16, _machine->get_cycle_count());
}

- (void)testMOVEPl_toDn {
	_machine->set_program({
		0x034e, 0x0002		// MOVEP.l 2(A6), D1
	});
	auto state = _machine->get_processor_state();
	state.address[6] = 0x3000;
	*_machine->ram_at(0x3002) = 0x1200;
	*_machine->ram_at(0x3004) = 0x3400;
	*_machine->ram_at(0x3006) = 0x5600;
	*_machine->ram_at(0x3008) = 0x7800;

	_machine->set_processor_state(state);
	_machine->run_for_instructions(1);

	state = _machine->get_processor_state();
	XCTAssertEqual(state.address[6], 0x3000);
	XCTAssertEqual(state.data[1], 0x12345678);
	XCTAssertEqual(state.status & Flag::ConditionCodes, 0);
	XCTAssertEqual(24, _machine->get_cycle_count());
}

- (void)testMOVEPw_fromDn {
	_machine->set_program({
		0x038e, 0x0002		// MOVEP.w D1, 2(A6)
	});
	auto state = _machine->get_processor_state();
	state.address[6] = 0x3000;
	state.data[1] = 0x12345678;

	_machine->set_processor_state(state);
	_machine->run_for_instructions(1);

	state = _machine->get_processor_state();
	XCTAssertEqual(state.address[6], 0x3000);
	XCTAssertEqual(*_machine->ram_at(0x3002), 0x5600);
	XCTAssertEqual(*_machine->ram_at(0x3004), 0x7800);

	XCTAssertEqual(state.status & Flag::ConditionCodes, 0);
	XCTAssertEqual(16, _machine->get_cycle_count());
}

- (void)testMOVEPl_fromDn {
	_machine->set_program({
		0x03ce, 0x0002		// MOVEP.l D1, 2(A6)
	});
	auto state = _machine->get_processor_state();
	state.address[6] = 0x3000;
	state.data[1] = 0x12345678;

	_machine->set_processor_state(state);
	_machine->run_for_instructions(1);

	state = _machine->get_processor_state();
	XCTAssertEqual(state.address[6], 0x3000);

	XCTAssertEqual(*_machine->ram_at(0x3002), 0x1200);
	XCTAssertEqual(*_machine->ram_at(0x3004), 0x3400);
	XCTAssertEqual(*_machine->ram_at(0x3006), 0x5600);
	XCTAssertEqual(*_machine->ram_at(0x3008), 0x7800);

	XCTAssertEqual(state.status & Flag::ConditionCodes, 0);
	XCTAssertEqual(24, _machine->get_cycle_count());
}

// MARK: MOVEQ

- (void)testMOVEQ_1 {
	_machine->set_program({
		0x7201		// MOVEQ #1, D1
	});
	auto state = _machine->get_processor_state();
	state.data[1] = 0xffffffff;

	_machine->set_processor_state(state);
	_machine->run_for_instructions(1);

	state = _machine->get_processor_state();
	XCTAssertEqual(state.data[1], 0x1);
	XCTAssertEqual(state.status & Flag::ConditionCodes, 0);
	XCTAssertEqual(4, _machine->get_cycle_count());
}

- (void)testMOVEQ_ff {
	_machine->set_program({
		0x72ff		// MOVEQ #-1, D1
	});
	auto state = _machine->get_processor_state();
	state.status |= Flag::Extend | Flag::Carry | Flag::Overflow;

	_machine->set_processor_state(state);

	_machine->run_for_instructions(1);

	state = _machine->get_processor_state();
	XCTAssertEqual(state.data[1], 0xffffffff);
	XCTAssertEqual(state.status & Flag::ConditionCodes, Flag::Extend | Flag::Negative);
	XCTAssertEqual(4, _machine->get_cycle_count());
}

- (void)testMOVEQ_80 {
	_machine->set_program({
		0x7280		// MOVEQ #$80, D1
	});
	auto state = _machine->get_processor_state();
	state.status |= Flag::Extend | Flag::Carry | Flag::Overflow;
	state.data[1] = 0x12345678;

	_machine->set_processor_state(state);
	_machine->run_for_instructions(1);

	state = _machine->get_processor_state();
	XCTAssertEqual(state.data[1], 0xffffff80);
	XCTAssertEqual(state.status & Flag::ConditionCodes, Flag::Extend | Flag::Negative);
	XCTAssertEqual(4, _machine->get_cycle_count());
}

- (void)testMOVEQ_00 {
	_machine->set_program({
		0x7200		// MOVEQ #00, D1
	});
	auto state = _machine->get_processor_state();
	state.status |= Flag::Extend | Flag::Carry | Flag::Overflow;
	state.data[1] = 0x12345678;

	_machine->set_processor_state(state);
	_machine->run_for_instructions(1);

	state = _machine->get_processor_state();
	XCTAssertEqual(state.data[1], 0);
	XCTAssertEqual(state.status & Flag::ConditionCodes, Flag::Extend | Flag::Zero);
	XCTAssertEqual(4, _machine->get_cycle_count());
}

// MARK: MOVE from SR

- (void)testMoveFromSR {
	_machine->set_program({
		0x40c1		// MOVE SR, D1
	});
	auto state = _machine->get_processor_state();
	state.status = 0x271f;

	_machine->set_processor_state(state);
	_machine->run_for_instructions(1);

	state = _machine->get_processor_state();
	XCTAssertEqual(state.data[1], 0x271f);
	XCTAssertEqual(state.status & Flag::ConditionCodes, Flag::ConditionCodes);
	XCTAssertEqual(6, _machine->get_cycle_count());
}

// MARK: MOVE to CCR

- (void)testMoveToCCR {
	_machine->set_program({
		0x44fc, 0x001f		// MOVE #$1f, CCR
	});
	auto state = _machine->get_processor_state();
	state.status = 0;	// i.e. not even supervisor.

	_machine->set_processor_state(state);
	_machine->run_for_instructions(1);

	state = _machine->get_processor_state();
	XCTAssertEqual(state.status & Flag::ConditionCodes, 0x1f);
	XCTAssertEqual(16, _machine->get_cycle_count());
}

// MARK: MOVE to SR

- (void)testMoveToSR {
	_machine->set_program({
		0x46fc, 0x0700		// MOVE #$700, SR
	});
	auto state = _machine->get_processor_state();
	state.supervisor_stack_pointer = 0x3000;
	state.user_stack_pointer = 0;

	_machine->set_processor_state(state);
	_machine->run_for_instructions(1);

	state = _machine->get_processor_state();
	XCTAssertEqual(state.stack_pointer(), 0);
	XCTAssertEqual(state.status & Flag::ConditionCodes, 0);
	XCTAssertEqual(16, _machine->get_cycle_count());
}

// MARK: MOVE USP

- (void)testMoveUSP {
	_machine->set_program({
		0x4e69		// MOVE USP, A1
	});
	auto state = _machine->get_processor_state();
	state.address[1] = 0x12348756;

	_machine->set_processor_state(state);
	_machine->run_for_instructions(1);

	state = _machine->get_processor_state();
	XCTAssertEqual(state.address[1], 0);
}

// MARK: PEA

- (void)testPEA_A1 {
	_machine->set_program({
		0x4851		// PEA (A1)
	});
	auto state = _machine->get_processor_state();
	state.address[1] = 0x3000ffff;
	_machine->set_initial_stack_pointer(0x1996);

	_machine->set_processor_state(state);
	_machine->run_for_instructions(1);

	state = _machine->get_processor_state();
	XCTAssertEqual(state.address[1], 0x3000ffff);
	XCTAssertEqual(state.stack_pointer(), 0x1992);
	XCTAssertEqual(*_machine->ram_at(0x1992), 0x3000);
	XCTAssertEqual(*_machine->ram_at(0x1994), 0xffff);
	XCTAssertEqual(12, _machine->get_cycle_count());
}

- (void)testPEA_A7 {
	_machine->set_program({
		0x4857		// PEA (A7)
	});
	_machine->set_initial_stack_pointer(0x1012);

	_machine->run_for_instructions(1);

	const auto state = _machine->get_processor_state();
	XCTAssertEqual(state.stack_pointer(), 0x100e);
	XCTAssertEqual(*_machine->ram_at(0x1010), 0x1012);
	XCTAssertEqual(*_machine->ram_at(0x1008), 0x0000);
	XCTAssertEqual(12, _machine->get_cycle_count());
}

- (void)testPEA_4A7 {
	_machine->set_program({
		0x486f, 0x0004		// PEA 4(A7)
	});
	_machine->set_initial_stack_pointer(0x1012);

	_machine->run_for_instructions(1);

	const auto state = _machine->get_processor_state();
	XCTAssertEqual(state.stack_pointer(), 0x100e);
	XCTAssertEqual(*_machine->ram_at(0x1010), 0x1016);
	XCTAssertEqual(*_machine->ram_at(0x1008), 0x0000);
	XCTAssertEqual(16, _machine->get_cycle_count());
}

- (void)testPEA_XXXw {
	_machine->set_program({
		0x4878, 0x3000		// PEA ($3000).w
	});
	_machine->set_initial_stack_pointer(0x1996);

	_machine->run_for_instructions(1);

	const auto state = _machine->get_processor_state();
	XCTAssertEqual(state.stack_pointer(), 0x1992);
	XCTAssertEqual(*_machine->ram_at(0x1992), 0x0000);
	XCTAssertEqual(*_machine->ram_at(0x1994), 0x3000);
	XCTAssertEqual(16, _machine->get_cycle_count());
}

- (void)testPEA_XXXl {
	_machine->set_program({
		0x4879, 0x1234, 0x5678		// PEA ($12345678)
	});
	_machine->set_initial_stack_pointer(0x1996);

	_machine->run_for_instructions(1);

	const auto state = _machine->get_processor_state();
	XCTAssertEqual(state.stack_pointer(), 0x1992);
	XCTAssertEqual(*_machine->ram_at(0x1992), 0x1234);
	XCTAssertEqual(*_machine->ram_at(0x1994), 0x5678);
	XCTAssertEqual(20, _machine->get_cycle_count());
}

// MARK: Scc

- (void)testSFDn {
	_machine->set_program({
		0x51c0		// SF D0
	});
	auto state = _machine->get_processor_state();
	state.data[0] = 0x12345678;
	state.status = Flag::Extend;

	_machine->set_processor_state(state);
	_machine->run_for_instructions(1);

	state = _machine->get_processor_state();
	XCTAssertEqual(state.data[0], 0x12345600);
	XCTAssertEqual(state.status & Flag::ConditionCodes, Flag::Extend);
}

- (void)testSTDn {
	_machine->set_program({
		0x50c0		// ST D0
	});
	auto state = _machine->get_processor_state();
	state.data[0] = 0x12345678;
	state.status = Flag::Extend;

	_machine->set_processor_state(state);
	_machine->run_for_instructions(1);

	state = _machine->get_processor_state();
	XCTAssertEqual(state.data[0], 0x123456ff);
	XCTAssertEqual(state.status & Flag::ConditionCodes, Flag::Extend);
}

- (void)testSLSDn {
	_machine->set_program({
		0x53c0		// SLS D0
	});
	auto state = _machine->get_processor_state();
	state.data[0] = 0x12345678;
	state.status = Flag::ConditionCodes;

	_machine->set_processor_state(state);
	_machine->run_for_instructions(1);

	state = _machine->get_processor_state();
	XCTAssertEqual(state.data[0], 0x123456ff);
	XCTAssertEqual(state.status & Flag::ConditionCodes, Flag::ConditionCodes);
}

- (void)testSGTAnXTrue {
	_machine->set_program({
		0x5ee8, 0x0002		// SGT 2(a0)
	});
	auto state = _machine->get_processor_state();
	state.address[0] = 0x3000;
	*_machine->ram_at(0x3002) = 0x8800;
	state.status = Flag::Extend;

	_machine->set_processor_state(state);
	_machine->run_for_instructions(1);

	state = _machine->get_processor_state();
	XCTAssertEqual(*_machine->ram_at(0x3002), 0xff00);
	XCTAssertEqual(state.status & Flag::ConditionCodes, Flag::Extend);
}

- (void)testSGTAnXFalse {
	_machine->set_program({
		0x5ee8, 0x0002		// SGT 2(a0)
	});
	auto state = _machine->get_processor_state();
	state.address[0] = 0x3000;
	*_machine->ram_at(0x3002) = 0x8800;
	state.status = Flag::ConditionCodes;

	_machine->set_processor_state(state);
	_machine->run_for_instructions(1);

	state = _machine->get_processor_state();
	XCTAssertEqual(*_machine->ram_at(0x3002), 0x0000);
	XCTAssertEqual(state.status & Flag::ConditionCodes, Flag::ConditionCodes);
}


// MARK: SWAP

- (void)testSwap {
	_machine->set_program({
		0x4841		// SWAP D1
	});
	auto state = _machine->get_processor_state();
	state.data[1] = 0x12348756;

	_machine->set_processor_state(state);
	_machine->run_for_instructions(1);

	state = _machine->get_processor_state();
	XCTAssertEqual(state.data[1], 0x87561234);
	XCTAssertEqual(state.status & Flag::ConditionCodes, Flag::Negative);
}

// MARK: TST

- (void)testTSTw_Dn {
	_machine->set_program({
		0x4a44		// TST.w D4
	});
	auto state = _machine->get_processor_state();
	state.status |= Flag::Extend | Flag::Carry | Flag::Overflow;
	state.data[4] = 0xfff1;

	_machine->set_processor_state(state);
	_machine->run_for_instructions(1);

	state = _machine->get_processor_state();
	XCTAssertEqual(state.status & Flag::ConditionCodes, Flag::Negative | Flag::Extend);
	XCTAssertEqual(state.data[4], 0xfff1);
	XCTAssertEqual(4, _machine->get_cycle_count());
}

- (void)testTSTl_Dn {
	_machine->set_program({
		0x4a84		// TST.l D4
	});
	auto state = _machine->get_processor_state();
	state.status |= Flag::Extend | Flag::Carry | Flag::Overflow;
	state.data[4] = 0;

	_machine->set_processor_state(state);
	_machine->run_for_instructions(1);

	state = _machine->get_processor_state();
	XCTAssertEqual(state.status & Flag::ConditionCodes, Flag::Zero | Flag::Extend);
	XCTAssertEqual(state.data[4], 0);
	XCTAssertEqual(4, _machine->get_cycle_count());
}

// Omitted: test that tst.w A0 doesn't decode.

// MARK: UNLINK

- (void)testUNLINK_A6 {
	_machine->set_program({
		0x4e5e		// UNLNK A6
	});

	auto state = _machine->get_processor_state();
	state.address[6] = 0x3000;
	*_machine->ram_at(0x3000) = 0x0000;
	*_machine->ram_at(0x3002) = 0x4000;

	_machine->set_processor_state(state);
	_machine->run_for_instructions(1);

	state = _machine->get_processor_state();
	XCTAssertEqual(state.address[6], 0x4000);
	XCTAssertEqual(state.supervisor_stack_pointer, 0x3004);
	XCTAssertEqual(12, _machine->get_cycle_count());
}

- (void)testUNLINK_A7 {
	_machine->set_program({
		0x4e5f		// UNLNK A7
	});
	_machine->set_initial_stack_pointer(0x3000);
	*_machine->ram_at(0x3000) = 0x0000;
	*_machine->ram_at(0x3002) = 0x4000;

	_machine->run_for_instructions(1);

	const auto state = _machine->get_processor_state();
	XCTAssertEqual(state.supervisor_stack_pointer, 0x4000);
	XCTAssertEqual(12, _machine->get_cycle_count());
}

@end
