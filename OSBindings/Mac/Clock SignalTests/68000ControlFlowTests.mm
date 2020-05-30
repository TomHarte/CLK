//
//  68000ControlFlowTests.m
//  Clock SignalTests
//
//  Created by Thomas Harte on 28/06/2019.
//
//  Largely ported from the tests of the Portable 68k Emulator.
//

#import <XCTest/XCTest.h>

#include "TestRunner68000.hpp"


@interface M68000ControlFlowTests : XCTestCase
@end

@implementation M68000ControlFlowTests {
	std::unique_ptr<RAM68000> _machine;
}

- (void)setUp {
	_machine = std::make_unique<RAM68000>();
}

- (void)tearDown {
	_machine.reset();
}

// MARK: Bcc

- (void)performBccb:(uint16_t)opcode {
	_machine->set_program({
		uint16_t(opcode | 6)	// Bcc.b +6
	});

	_machine->run_for_instructions(1);
}

- (void)performBccw:(uint16_t)opcode {
	_machine->set_program({
		opcode, 0x0006			// Bcc.w +6
	});

	_machine->run_for_instructions(1);
}

- (void)testBHIb {
	[self performBccb:0x6200];

	const auto state = _machine->get_processor_state();
	XCTAssertEqual(state.program_counter, 0x1008 + 4);
	XCTAssertEqual(_machine->get_cycle_count(), 10);
}

- (void)testBLOb {
	[self performBccb:0x6500];

	const auto state = _machine->get_processor_state();
	XCTAssertEqual(state.program_counter, 0x1002 + 4);
	XCTAssertEqual(_machine->get_cycle_count(), 8);
}

- (void)testBHIw {
	[self performBccw:0x6200];

	const auto state = _machine->get_processor_state();
	XCTAssertEqual(state.program_counter, 0x1008 + 4);
	XCTAssertEqual(_machine->get_cycle_count(), 10);
}

- (void)testBLOw {
	[self performBccw:0x6500];

	const auto state = _machine->get_processor_state();
	XCTAssertEqual(state.program_counter, 0x1004 + 4);
	XCTAssertEqual(_machine->get_cycle_count(), 12);
}

// MARK: BRA

- (void)testBRAb {
	_machine->set_program({
		0x6004		// BRA.b +4
	});

	_machine->run_for_instructions(1);

	const auto state = _machine->get_processor_state();
	XCTAssertEqual(state.program_counter, 0x1006 + 4);
	XCTAssertEqual(_machine->get_cycle_count(), 10);
}

- (void)testBRAw {
	_machine->set_program({
		0x6000, 0x0004		// BRA.b +4
	});

	_machine->run_for_instructions(1);

	const auto state = _machine->get_processor_state();
	XCTAssertEqual(state.program_counter, 0x1006 + 4);
	XCTAssertEqual(_machine->get_cycle_count(), 10);
}

// MARK: BSR

- (void)testBSRw {
	_machine->set_program({
		0x6100, 0x0006		// BSR.w $1008
	});
	_machine->set_initial_stack_pointer(0x3000);

	_machine->run_for_instructions(1);

	const auto state = _machine->get_processor_state();
	XCTAssertEqual(state.program_counter, 0x1008 + 4);
	XCTAssertEqual(state.stack_pointer(), 0x2ffc);
	XCTAssertEqual(state.status & Flag::ConditionCodes, 0);
	XCTAssertEqual(*_machine->ram_at(0x2ffc), 0);
	XCTAssertEqual(*_machine->ram_at(0x2ffe), 0x1004);

	XCTAssertEqual(_machine->get_cycle_count(), 18);
}

- (void)testBSRb {
	_machine->set_program({
		0x6106		// BSR.b $1008
	});
	_machine->set_initial_stack_pointer(0x3000);

	_machine->run_for_instructions(1);

	const auto state = _machine->get_processor_state();
	XCTAssertEqual(state.program_counter, 0x1008 + 4);
	XCTAssertEqual(state.stack_pointer(), 0x2ffc);
	XCTAssertEqual(state.status & Flag::ConditionCodes, 0);
	XCTAssertEqual(*_machine->ram_at(0x2ffc), 0);
	XCTAssertEqual(*_machine->ram_at(0x2ffe), 0x1002);

	XCTAssertEqual(_machine->get_cycle_count(), 18);
}

// MARK: CHK

- (void)performCHKd1:(uint32_t)d1 d2:(uint32_t)d2 {
	_machine->set_program({
		0x4581		// CHK D1, D2
	});
	auto state = _machine->get_processor_state();
	state.data[1] = d1;
	state.data[2] = d2;
	state.status |= Flag::ConditionCodes;

	_machine->set_initial_stack_pointer(0);
	_machine->set_processor_state(state);
	_machine->run_for_instructions(1);

	state = _machine->get_processor_state();
	XCTAssertEqual(state.data[1], d1);
	XCTAssertEqual(state.data[2], d2);
}

- (void)testCHK_1111v1111 {
	[self performCHKd1:0x1111 d2:0x1111];

	const auto state = _machine->get_processor_state();
	XCTAssertEqual(state.program_counter, 0x1002 + 4);
	XCTAssertEqual(state.status & Flag::ConditionCodes, Flag::Extend);
	XCTAssertEqual(10, _machine->get_cycle_count());
}

- (void)testCHK_1111v0000 {
	[self performCHKd1:0x1111 d2:0x0000];

	const auto state = _machine->get_processor_state();
	XCTAssertEqual(state.program_counter, 0x1002 + 4);
	XCTAssertEqual(state.status & Flag::ConditionCodes, Flag::Extend | Flag::Zero);
	XCTAssertEqual(10, _machine->get_cycle_count());
}

- (void)testCHK_8000v8001 {
	[self performCHKd1:0x8000 d2:0x8001];

	const auto state = _machine->get_processor_state();
	XCTAssertNotEqual(state.program_counter, 0x1002 + 4);
	XCTAssertEqual(state.stack_pointer(), 0xfffffffa);
	XCTAssertEqual(state.status & Flag::ConditionCodes, Flag::Extend);
	XCTAssertEqual(42, _machine->get_cycle_count());
}

- (void)testCHK_8000v8000 {
	[self performCHKd1:0x8000 d2:0x8000];

	const auto state = _machine->get_processor_state();
	XCTAssertNotEqual(state.program_counter, 0x1002 + 4);
	XCTAssertEqual(state.status & Flag::ConditionCodes, Flag::Extend | Flag::Negative);
	XCTAssertEqual(44, _machine->get_cycle_count());
}

// MARK: DBcc

- (void)performDBccTestOpcode:(uint16_t)opcode status:(uint16_t)status d2Outcome:(uint32_t)d2Output {
	_machine->set_program({
		opcode, 0x0008		// DBcc D2, +8
	});
	auto state = _machine->get_processor_state();
	state.status = status;
	state.data[2] = 1;

	_machine->set_processor_state(state);
	_machine->run_for_instructions(1);

	state = _machine->get_processor_state();
	XCTAssertEqual(state.data[2], d2Output);
	XCTAssertEqual(state.status, status);
}

- (void)testDBT {
	[self performDBccTestOpcode:0x50ca status:0 d2Outcome:1];
}

- (void)testDBF {
	[self performDBccTestOpcode:0x51ca status:0 d2Outcome:0];
}

- (void)testDBHI {
	[self performDBccTestOpcode:0x52ca status:0 d2Outcome:1];
}

- (void)testDBHI_Carry {
	[self performDBccTestOpcode:0x52ca status:Flag::Carry d2Outcome:0];
}

- (void)testDBHI_Zero {
	[self performDBccTestOpcode:0x52ca status:Flag::Zero d2Outcome:0];
}

- (void)testDBLS_CarryOverflow {
	[self performDBccTestOpcode:0x53ca status:Flag::Carry | Flag::Overflow d2Outcome:1];
}

- (void)testDBLS_Carry {
	[self performDBccTestOpcode:0x53ca status:Flag::Carry d2Outcome:1];
}

- (void)testDBLS_Overflow {
	[self performDBccTestOpcode:0x53ca status:Flag::Overflow d2Outcome:0];
}

- (void)testDBCC_Carry {
	[self performDBccTestOpcode:0x54ca status:Flag::Carry d2Outcome:0];
}

- (void)testDBCC {
	[self performDBccTestOpcode:0x54ca status:0 d2Outcome:1];
}

- (void)testDBCS {
	[self performDBccTestOpcode:0x55ca status:0 d2Outcome:0];
}

- (void)testDBCS_Carry {
	[self performDBccTestOpcode:0x55ca status:Flag::Carry d2Outcome:1];
}

- (void)testDBNE {
	[self performDBccTestOpcode:0x56ca status:0 d2Outcome:1];
}

- (void)testDBNE_Zero {
	[self performDBccTestOpcode:0x56ca status:Flag::Zero d2Outcome:0];
}

- (void)testDBEQ {
	[self performDBccTestOpcode:0x57ca status:0 d2Outcome:0];
}

- (void)testDBEQ_Zero {
	[self performDBccTestOpcode:0x57ca status:Flag::Zero d2Outcome:1];
}

- (void)testDBVC {
	[self performDBccTestOpcode:0x58ca status:0 d2Outcome:1];
}

- (void)testDBVC_Overflow {
	[self performDBccTestOpcode:0x58ca status:Flag::Overflow d2Outcome:0];
}

- (void)testDBVS {
	[self performDBccTestOpcode:0x59ca status:0 d2Outcome:0];
}

- (void)testDBVS_Overflow {
	[self performDBccTestOpcode:0x59ca status:Flag::Overflow d2Outcome:1];
}

- (void)testDBPL {
	[self performDBccTestOpcode:0x5aca status:0 d2Outcome:1];
}

- (void)testDBPL_Negative {
	[self performDBccTestOpcode:0x5aca status:Flag::Negative d2Outcome:0];
}

- (void)testDBMI {
	[self performDBccTestOpcode:0x5bca status:0 d2Outcome:0];
}

- (void)testDBMI_Negative {
	[self performDBccTestOpcode:0x5bca status:Flag::Negative d2Outcome:1];
}

- (void)testDBGE_NegativeOverflow {
	[self performDBccTestOpcode:0x5cca status:Flag::Negative | Flag::Overflow d2Outcome:1];
}

- (void)testDBGE {
	[self performDBccTestOpcode:0x5cca status:0 d2Outcome:1];
}

- (void)testDBGE_Negative {
	[self performDBccTestOpcode:0x5cca status:Flag::Negative d2Outcome:0];
}

- (void)testDBGE_Overflow {
	[self performDBccTestOpcode:0x5cca status:Flag::Overflow d2Outcome:0];
}

- (void)testDBLT_NegativeOverflow {
	[self performDBccTestOpcode:0x5dca status:Flag::Negative | Flag::Overflow d2Outcome:0];
}

- (void)testDBLT {
	[self performDBccTestOpcode:0x5dca status:0 d2Outcome:0];
}

- (void)testDBLT_Negative {
	[self performDBccTestOpcode:0x5dca status:Flag::Negative d2Outcome:1];
}

- (void)testDBLT_Overflow {
	[self performDBccTestOpcode:0x5dca status:Flag::Overflow d2Outcome:1];
}

- (void)testDBGT {
	[self performDBccTestOpcode:0x5eca status:0 d2Outcome:1];
}

- (void)testDBGT_ZeroNegativeOverflow {
	[self performDBccTestOpcode:0x5eca status:Flag::Zero | Flag::Negative | Flag::Overflow d2Outcome:0];
}

- (void)testDBGT_NegativeOverflow {
	[self performDBccTestOpcode:0x5eca status:Flag::Negative | Flag::Overflow d2Outcome:1];
}

- (void)testDBGT_Zero {
	[self performDBccTestOpcode:0x5eca status:Flag::Zero d2Outcome:0];
}

- (void)testDBLE {
	[self performDBccTestOpcode:0x5fca status:0 d2Outcome:0];
}

- (void)testDBLE_Zero {
	[self performDBccTestOpcode:0x5fca status:Flag::Zero d2Outcome:1];
}

- (void)testDBLE_Negative {
	[self performDBccTestOpcode:0x5fca status:Flag::Negative d2Outcome:1];
}

- (void)testDBLE_NegativeOverflow {
	[self performDBccTestOpcode:0x5fca status:Flag::Negative | Flag::Overflow d2Outcome:0];
}

/* Further DBF tests omitted; they seemed to be duplicative, assuming I'm not suffering a failure of comprehension. */

// MARK: JMP

- (void)testJMP_A1 {
	_machine->set_program({
		0x4ed1		// JMP (A1)
	});

	auto state = _machine->get_processor_state();
	state.address[1] = 0x3000;

	_machine->set_processor_state(state);
	_machine->run_for_instructions(1);

	state = _machine->get_processor_state();
	XCTAssertEqual(state.address[1], 0x3000);
	XCTAssertEqual(state.program_counter, 0x3000 + 4);
	XCTAssertEqual(8, _machine->get_cycle_count());
}

- (void)testJMP_PC {
	_machine->set_program({
		0x4efa, 0x000a		// JMP PC+a (i.e. to 0x100c)
	});

	_machine->run_for_instructions(1);

	const auto state = _machine->get_processor_state();
	XCTAssertEqual(state.program_counter, 0x100c + 4);
	XCTAssertEqual(10, _machine->get_cycle_count());
}

// MARK: JSR

- (void)testJSR_PC {
	_machine->set_program({
		0x4eba, 0x000a		// JSR (+a)PC		; JSR to $100c
	});
	_machine->set_initial_stack_pointer(0x2000);

	_machine->run_for_instructions(1);

	const auto state = _machine->get_processor_state();
	XCTAssertEqual(state.stack_pointer(), 0x1ffc);
	XCTAssertEqual(state.program_counter, 0x100c + 4);
	XCTAssertEqual(*_machine->ram_at(0x1ffc), 0x0000);
	XCTAssertEqual(*_machine->ram_at(0x1ffe), 0x1004);
	XCTAssertEqual(18, _machine->get_cycle_count());
}

- (void)testJSR_XXXl {
	_machine->set_program({
		0x4eb9, 0x0000, 0x1008		// JSR ($1008).l
	});
	_machine->set_initial_stack_pointer(0x2000);

	_machine->run_for_instructions(1);

	const auto state = _machine->get_processor_state();
	XCTAssertEqual(state.stack_pointer(), 0x1ffc);
	XCTAssertEqual(state.program_counter, 0x1008 + 4);
	XCTAssertEqual(*_machine->ram_at(0x1ffc), 0x0000);
	XCTAssertEqual(*_machine->ram_at(0x1ffe), 0x1006);
	XCTAssertEqual(20, _machine->get_cycle_count());
}

// MARK: NOP

- (void)testNOP {
	_machine->set_program({
		0x4e71		// NOP
	});
	_machine->run_for_instructions(1);
	XCTAssertEqual(4, _machine->get_cycle_count());
}

// MARK: RTR

- (void)testRTR {
	_machine->set_program({
		0x4e77		// RTR
	});
	_machine->set_initial_stack_pointer(0x2000);
	*_machine->ram_at(0x2000) = 0x7fff;
	*_machine->ram_at(0x2002) = 0;
	*_machine->ram_at(0x2004) = 0xc;

	_machine->run_for_instructions(1);

	const auto state = _machine->get_processor_state();
	XCTAssertEqual(state.stack_pointer(), 0x2006);
	XCTAssertEqual(state.program_counter, 0x10);
	XCTAssertEqual(state.status & Flag::ConditionCodes, Flag::ConditionCodes);
	XCTAssertEqual(20, _machine->get_cycle_count());
}

// MARK: RTS

- (void)testRTS {
	_machine->set_program({
		0x4e75		// RTS
	});
	_machine->set_initial_stack_pointer(0x2000);
	*_machine->ram_at(0x2000) = 0x0000;
	*_machine->ram_at(0x2002) = 0x000c;

	_machine->run_for_instructions(1);

	const auto state = _machine->get_processor_state();
	XCTAssertEqual(state.stack_pointer(), 0x2004);
	XCTAssertEqual(state.program_counter, 0x000c + 4);
	XCTAssertEqual(16, _machine->get_cycle_count());
}

// MARK: TRAP

- (void)testTRAP {
	_machine->set_program({
		0x4e41		// TRAP #1
	});
	auto state = _machine->get_processor_state();
	state.status = 0x700;
	state.user_stack_pointer = 0x200;
	state.supervisor_stack_pointer = 0x206;
	*_machine->ram_at(0x84) = 0xfffe;
	*_machine->ram_at(0xfffe) = 0x4e71;

	_machine->set_processor_state(state);
	_machine->run_for_instructions(1);

	state = _machine->get_processor_state();
	XCTAssertEqual(state.status, 0x2700);
	XCTAssertEqual(*_machine->ram_at(0x200), 0x700);
	XCTAssertEqual(*_machine->ram_at(0x202), 0x0000);
	XCTAssertEqual(*_machine->ram_at(0x204), 0x1002);
	XCTAssertEqual(state.supervisor_stack_pointer, 0x200);
	XCTAssertEqual(34, _machine->get_cycle_count());
}

// MARK: TRAPV

- (void)testTRAPV_taken {
	_machine->set_program({
		0x4e76		// TRAPV
	});
	_machine->set_initial_stack_pointer(0x206);

	auto state = _machine->get_processor_state();
	state.status = 0x702;
	state.supervisor_stack_pointer = 0x206;
	*_machine->ram_at(0x1e) = 0xfffe;
	*_machine->ram_at(0xfffe) = 0x4e71;

	_machine->set_processor_state(state);
	_machine->run_for_instructions(1);

	state = _machine->get_processor_state();
	XCTAssertEqual(state.status, 0x2702);
	XCTAssertEqual(state.stack_pointer(), 0x200);
	XCTAssertEqual(*_machine->ram_at(0x202), 0x0000);
	XCTAssertEqual(*_machine->ram_at(0x204), 0x1002);
	XCTAssertEqual(*_machine->ram_at(0x200), 0x702);
	XCTAssertEqual(34, _machine->get_cycle_count());
}

- (void)testTRAPV_untaken {
	_machine->set_program({
		0x4e76		// TRAPV
	});

	_machine->run_for_instructions(1);

	const auto state = _machine->get_processor_state();
	XCTAssertEqual(state.program_counter, 0x1002 + 4);
	XCTAssertEqual(4, _machine->get_cycle_count());
}

@end
