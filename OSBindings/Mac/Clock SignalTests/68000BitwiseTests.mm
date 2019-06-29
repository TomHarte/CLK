//
//  68000Bitwise.m
//  Clock SignalTests
//
//  Created by Thomas Harte on 28/06/2019.
//
//  Largely ported from the tests of the Portable 68k Emulator.
//

#import <XCTest/XCTest.h>

#include "TestRunner68000.hpp"

@interface M68000BitwiseTests : XCTestCase
@end

@implementation M68000BitwiseTests {
	std::unique_ptr<RAM68000> _machine;
}

- (void)setUp {
    _machine.reset(new RAM68000());
}

- (void)tearDown {
	_machine.reset();
}

// MARK: BCHG

- (void)performBCHGD0D1:(uint32_t)d1 {
	_machine->set_program({
		0x0340		// BCHG D1, D0
	});
	auto state = _machine->get_processor_state();
	state.data[0] = 0x12345678;
	state.data[1] = d1;

	_machine->set_processor_state(state);
	_machine->run_for_instructions(1);

	state = _machine->get_processor_state();
	XCTAssertEqual(state.data[1], d1);
}

- (void)testBCHG_D0D1_0 {
	[self performBCHGD0D1:0];

	const auto state = _machine->get_processor_state();
	XCTAssertEqual(state.data[0], 0x12345679);
	XCTAssertEqual(state.status & Flag::ConditionCodes, Flag::Zero);
	XCTAssertEqual(_machine->get_cycle_count(), 6);
}

- (void)testBCHG_D0D1_10 {
	[self performBCHGD0D1:10];

	const auto state = _machine->get_processor_state();
	XCTAssertEqual(state.data[0], 0x12345278);
	XCTAssertEqual(state.status & Flag::ConditionCodes, 0);
	XCTAssertEqual(_machine->get_cycle_count(), 6);
}

- (void)testBCHG_D0D1_48 {
	[self performBCHGD0D1:48];

	const auto state = _machine->get_processor_state();
	XCTAssertEqual(state.data[0], 0x12355678);
	XCTAssertEqual(state.status & Flag::ConditionCodes, Flag::Zero);
	XCTAssertEqual(_machine->get_cycle_count(), 8);
}

- (void)performBCHGD1Ind:(uint32_t)d1 {
	_machine->set_program({
		0x0350		// BCHG D1, (A0)
	});
	auto state = _machine->get_processor_state();
	state.address[0] = 0x3000;
	state.data[1] = d1;
	*_machine->ram_at(0x3000) = 0x7800;

	_machine->set_processor_state(state);
	_machine->run_for_instructions(1);

	state = _machine->get_processor_state();
	XCTAssertEqual(state.data[1], d1);
	XCTAssertEqual(state.address[0], 0x3000);
	XCTAssertEqual(_machine->get_cycle_count(), 12);
}

- (void)testBCHG_D1Ind_48 {
	[self performBCHGD1Ind:48];

	const auto state = _machine->get_processor_state();
	XCTAssertEqual(state.status & Flag::ConditionCodes, Flag::Zero);
	XCTAssertEqual(*_machine->ram_at(0x3000), 0x7900);
}

- (void)testBCHG_D1Ind_7 {
	[self performBCHGD1Ind:7];

	const auto state = _machine->get_processor_state();
	XCTAssertEqual(state.status & Flag::ConditionCodes, Flag::Zero);
	XCTAssertEqual(*_machine->ram_at(0x3000), 0xf800);
}

- (void)performBCHGImm:(uint16_t)immediate {
	_machine->set_program({
		0x0840, immediate		// BCHG #, D0
	});
	auto state = _machine->get_processor_state();
	state.data[0] = 0x12345678;

	_machine->set_processor_state(state);
	_machine->run_for_instructions(1);
}

- (void)testBCHG_Imm_31 {
	[self performBCHGImm:31];

	const auto state = _machine->get_processor_state();
	XCTAssertEqual(state.data[0], 0x92345678);
	XCTAssertEqual(state.status & Flag::ConditionCodes, Flag::Zero);
	XCTAssertEqual(_machine->get_cycle_count(), 12);
}

- (void)testBCHG_Imm_4 {
	[self performBCHGImm:4];

	const auto state = _machine->get_processor_state();
	XCTAssertEqual(state.data[0], 0x12345668);
	XCTAssertEqual(state.status & Flag::ConditionCodes, 0);
	XCTAssertEqual(_machine->get_cycle_count(), 10);
}

- (void)testBCHG_ImmWWWx {
	_machine->set_program({
		0x0878, 0x0006, 0x3000		// BCHG #6, ($3000).W
	});
	*_machine->ram_at(0x3000) = 0x7800;

	_machine->run_for_instructions(1);

	const auto state = _machine->get_processor_state();
	XCTAssertEqual(state.status & Flag::ConditionCodes, 0);
	XCTAssertEqual(_machine->get_cycle_count(), 20);
	XCTAssertEqual(*_machine->ram_at(0x3000), 0x3800);
}

// MARK: BCLR

- (void)performBCLRD0D1:(uint32_t)d1 {
	_machine->set_program({
		0x0380		// BCLR D1, D0
	});
	auto state = _machine->get_processor_state();
	state.data[0] = 0x12345678;
	state.data[1] = d1;

	_machine->set_processor_state(state);
	_machine->run_for_instructions(1);

	state = _machine->get_processor_state();
	XCTAssertEqual(state.data[1], d1);
}

- (void)testBCLR_D0D1_0 {
	[self performBCLRD0D1:0];

	const auto state = _machine->get_processor_state();
	XCTAssertEqual(state.data[0], 0x12345678);
	XCTAssertEqual(state.status & Flag::ConditionCodes, Flag::Zero);
	XCTAssertEqual(_machine->get_cycle_count(), 8);
}

- (void)testBCLR_D0D1_10 {
	[self performBCLRD0D1:10];

	const auto state = _machine->get_processor_state();
	XCTAssertEqual(state.data[0], 0x12345278);
	XCTAssertEqual(state.status & Flag::ConditionCodes, 0);
	XCTAssertEqual(_machine->get_cycle_count(), 8);
}

- (void)testBCLR_D0D1_50 {
	[self performBCLRD0D1:50];

	const auto state = _machine->get_processor_state();
	XCTAssertEqual(state.data[0], 0x12305678);
	XCTAssertEqual(state.status & Flag::ConditionCodes, 0);
	XCTAssertEqual(_machine->get_cycle_count(), 10);
}

- (void)performBCLRD1Ind:(uint32_t)d1 {
	_machine->set_program({
		0x0390		// BCLR D1, (A0)
	});
	auto state = _machine->get_processor_state();
	state.address[0] = 0x3000;
	state.data[1] = d1;
	*_machine->ram_at(0x3000) = 0x7800;

	_machine->set_processor_state(state);
	_machine->run_for_instructions(1);

	state = _machine->get_processor_state();
	XCTAssertEqual(state.data[1], d1);
	XCTAssertEqual(state.address[0], 0x3000);
	XCTAssertEqual(_machine->get_cycle_count(), 12);
}

- (void)testBCLR_D1Ind_50 {
	[self performBCLRD1Ind:50];

	const auto state = _machine->get_processor_state();
	XCTAssertEqual(state.status & Flag::ConditionCodes, Flag::Zero);
	XCTAssertEqual(*_machine->ram_at(0x3000), 0x7800);
}

- (void)testBCLR_D1Ind_3 {
	[self performBCLRD1Ind:3];

	const auto state = _machine->get_processor_state();
	XCTAssertEqual(state.status & Flag::ConditionCodes, 0);
	XCTAssertEqual(*_machine->ram_at(0x3000), 0x7000);
}

- (void)performBCLRImm:(uint16_t)immediate {
	_machine->set_program({
		0x0880, immediate		// BCLR #, D0
	});
	auto state = _machine->get_processor_state();
	state.data[0] = 0x12345678;

	_machine->set_processor_state(state);
	_machine->run_for_instructions(1);

	state = _machine->get_processor_state();
	XCTAssertEqual(state.status & Flag::ConditionCodes, 0);
}

- (void)testBCLR_Imm_28 {
	[self performBCLRImm:28];

	const auto state = _machine->get_processor_state();
	XCTAssertEqual(state.data[0], 0x02345678);
	XCTAssertEqual(_machine->get_cycle_count(), 14);
}

- (void)testBCLR_Imm_4 {
	[self performBCLRImm:4];

	const auto state = _machine->get_processor_state();
	XCTAssertEqual(state.data[0], 0x12345668);
	XCTAssertEqual(_machine->get_cycle_count(), 12);
}

- (void)testBCLR_ImmWWWx {
	_machine->set_program({
		0x08b8, 0x0006, 0x3000		// BCLR #6, ($3000).W
	});
	*_machine->ram_at(0x3000) = 0x7800;

	_machine->run_for_instructions(1);

	const auto state = _machine->get_processor_state();
	XCTAssertEqual(state.status & Flag::ConditionCodes, 0);
	XCTAssertEqual(_machine->get_cycle_count(), 20);
	XCTAssertEqual(*_machine->ram_at(0x3000), 0x3800);
}

// MARK: BSET

- (void)performBSETD0D1:(uint32_t)d1 {
	_machine->set_program({
		0x03c0		// BSET D1, D0
	});
	auto state = _machine->get_processor_state();
	state.data[0] = 0x12345678;
	state.data[1] = d1;

	_machine->set_processor_state(state);
	_machine->run_for_instructions(1);

	state = _machine->get_processor_state();
	XCTAssertEqual(state.data[1], d1);
}

- (void)testBSET_D0D1_0 {
	[self performBSETD0D1:0];

	const auto state = _machine->get_processor_state();
	XCTAssertEqual(state.data[0], 0x12345679);
	XCTAssertEqual(state.status & Flag::ConditionCodes, Flag::Zero);
	XCTAssertEqual(_machine->get_cycle_count(), 6);
}

- (void)testBSET_D0D1_10 {
	[self performBSETD0D1:10];

	const auto state = _machine->get_processor_state();
	XCTAssertEqual(state.data[0], 0x12345678);
	XCTAssertEqual(state.status & Flag::ConditionCodes, 0);
	XCTAssertEqual(_machine->get_cycle_count(), 6);
}

- (void)testBSET_D0D1_49 {
	[self performBSETD0D1:49];

	const auto state = _machine->get_processor_state();
	XCTAssertEqual(state.data[0], 0x12365678);
	XCTAssertEqual(state.status & Flag::ConditionCodes, Flag::Zero);
	XCTAssertEqual(_machine->get_cycle_count(), 8);
}

- (void)performBSETD1Ind:(uint32_t)d1 {
	_machine->set_program({
		0x03d0		// BSET D1, (A0)
	});
	auto state = _machine->get_processor_state();
	state.address[0] = 0x3000;
	state.data[1] = d1;
	*_machine->ram_at(0x3000) = 0x7800;

	_machine->set_processor_state(state);
	_machine->run_for_instructions(1);

	state = _machine->get_processor_state();
	XCTAssertEqual(state.data[1], d1);
	XCTAssertEqual(state.address[0], 0x3000);
	XCTAssertEqual(_machine->get_cycle_count(), 12);
}

- (void)testBSET_D1Ind_50 {
	[self performBSETD1Ind:50];

	const auto state = _machine->get_processor_state();
	XCTAssertEqual(state.status & Flag::ConditionCodes, Flag::Zero);
	XCTAssertEqual(*_machine->ram_at(0x3000), 0x7c00);
}

- (void)testBSET_D1Ind_3 {
	[self performBSETD1Ind:3];

	const auto state = _machine->get_processor_state();
	XCTAssertEqual(state.status & Flag::ConditionCodes, 0);
	XCTAssertEqual(*_machine->ram_at(0x3000), 0x7800);
}

- (void)performBSETImm:(uint16_t)immediate {
	_machine->set_program({
		0x08c0, immediate		// BSET #, D0
	});
	auto state = _machine->get_processor_state();
	state.data[0] = 0x12345678;

	_machine->set_processor_state(state);
	_machine->run_for_instructions(1);
}

- (void)testBSET_Imm_28 {
	[self performBSETImm:28];

	const auto state = _machine->get_processor_state();
	XCTAssertEqual(state.data[0], 0x12345678);
	XCTAssertEqual(state.status & Flag::ConditionCodes, 0);
	XCTAssertEqual(_machine->get_cycle_count(), 12);
}

- (void)testBSET_Imm_2 {
	[self performBSETImm:2];

	const auto state = _machine->get_processor_state();
	XCTAssertEqual(state.data[0], 0x1234567c);
	XCTAssertEqual(state.status & Flag::ConditionCodes, Flag::Zero);
	XCTAssertEqual(_machine->get_cycle_count(), 10);
}

- (void)testBSET_ImmWWWx {
	_machine->set_program({
		0x08f8, 0x0006, 0x3000		// BSET #6, ($3000).W
	});
	*_machine->ram_at(0x3000) = 0x7800;

	_machine->run_for_instructions(1);

	const auto state = _machine->get_processor_state();
	XCTAssertEqual(state.status & Flag::ConditionCodes, 0);
	XCTAssertEqual(_machine->get_cycle_count(), 20);
	XCTAssertEqual(*_machine->ram_at(0x3000), 0x7800);
}

// MARK: BTST

- (void)performBTSTD0D1:(uint32_t)d1 {
	_machine->set_program({
		0x0300		// BTST D1, D0
	});
	auto state = _machine->get_processor_state();
	state.data[0] = 0x12345678;
	state.data[1] = d1;

	_machine->set_processor_state(state);
	_machine->run_for_instructions(1);

	state = _machine->get_processor_state();
	XCTAssertEqual(state.data[0], 0x12345678);
	XCTAssertEqual(state.data[1], d1);
	XCTAssertEqual(6, _machine->get_cycle_count());
}

- (void)testBTST_D0D1_0 {
	[self performBTSTD0D1:0];

	const auto state = _machine->get_processor_state();
	XCTAssertEqual(state.status & Flag::ConditionCodes, Flag::Zero);
}

- (void)testBTST_D0D1_10 {
	[self performBTSTD0D1:10];

	const auto state = _machine->get_processor_state();
	XCTAssertEqual(state.status & Flag::ConditionCodes, 0);
}

- (void)testBTST_D0D1_49 {
	[self performBTSTD0D1:49];

	const auto state = _machine->get_processor_state();
	XCTAssertEqual(state.status & Flag::ConditionCodes, Flag::Zero);
}

- (void)performBTSTD1Ind:(uint32_t)d1 {
	_machine->set_program({
		0x0310		// BTST D1, (A0)
	});
	auto state = _machine->get_processor_state();
	state.address[0] = 0x3000;
	state.data[1] = d1;
	*_machine->ram_at(0x3000) = 0x7800;

	_machine->set_processor_state(state);
	_machine->run_for_instructions(1);

	state = _machine->get_processor_state();
	XCTAssertEqual(state.data[1], d1);
	XCTAssertEqual(state.address[0], 0x3000);
	XCTAssertEqual(8, _machine->get_cycle_count());
	XCTAssertEqual(*_machine->ram_at(0x3000), 0x7800);
}

- (void)testBTST_D1Ind_50 {
	[self performBTSTD1Ind:50];

	const auto state = _machine->get_processor_state();
	XCTAssertEqual(state.status & Flag::ConditionCodes, Flag::Zero);
}

- (void)testBTST_D1Ind_3 {
	[self performBTSTD1Ind:3];

	const auto state = _machine->get_processor_state();
	XCTAssertEqual(state.status & Flag::ConditionCodes, 0);
}

- (void)performBTSTImm:(uint16_t)immediate {
	_machine->set_program({
		0x0800, immediate		// BTST #, D0
	});
	auto state = _machine->get_processor_state();
	state.data[0] = 0x12345678;

	_machine->set_processor_state(state);
	_machine->run_for_instructions(1);

	state = _machine->get_processor_state();
	XCTAssertEqual(state.data[0], 0x12345678);
	XCTAssertEqual(10, _machine->get_cycle_count());
}

- (void)testBTST_Imm_28 {
	[self performBTSTImm:28];

	const auto state = _machine->get_processor_state();
	XCTAssertEqual(state.status & Flag::ConditionCodes, 0);
}

- (void)testBTST_Imm_2 {
	[self performBTSTImm:2];

	const auto state = _machine->get_processor_state();
	XCTAssertEqual(state.status & Flag::ConditionCodes, Flag::Zero);
}

- (void)testBTST_ImmWWWx {
	_machine->set_program({
		0x0838, 0x0006, 0x3000		// BTST #6, ($3000).W
	});
	*_machine->ram_at(0x3000) = 0x7800;

	_machine->run_for_instructions(1);

	const auto state = _machine->get_processor_state();
	XCTAssertEqual(state.status & Flag::ConditionCodes, 0);
	XCTAssertEqual(_machine->get_cycle_count(), 16);
	XCTAssertEqual(*_machine->ram_at(0x3000), 0x7800);
}

// MARK: -
// MARK: ANDI

- (void)testANDIb {
	_machine->set_program({
		0x0201, 0x0012		// ANDI.B #$12, D1
	});

	auto state = _machine->get_processor_state();
	state.data[1] = 0x12345678;

	_machine->set_processor_state(state);
	_machine->run_for_instructions(1);

	state = _machine->get_processor_state();
	XCTAssertEqual(state.data[1], 0x12345610);
	XCTAssertEqual(state.status & Flag::ConditionCodes, 0);
	XCTAssertEqual(8, _machine->get_cycle_count());
}

- (void)testANDIl {
	_machine->set_program({
		0x02b8, 0xffff, 0x0000, 0x3000		// ANDI.L #$ffff0000, ($3000).W
	});

	*_machine->ram_at(0x3000) = 0x0000;
	*_machine->ram_at(0x3002) = 0xffff;

	_machine->run_for_instructions(1);

	const auto state = _machine->get_processor_state();
	XCTAssertEqual(*_machine->ram_at(0x3000), 0x0000);
	XCTAssertEqual(*_machine->ram_at(0x3002), 0x0000);
	XCTAssertEqual(state.status & Flag::ConditionCodes, Flag::Zero);
	XCTAssertEqual(32, _machine->get_cycle_count());
}

// MARK: ANDI CCR

- (void)testANDICCR {
	_machine->set_program({
		0x023c, 0x001b		// ANDI.b #$1b, CCR
	});
	auto state = _machine->get_processor_state();
	state.status |= 0xc;

	_machine->set_processor_state(state);
	_machine->run_for_instructions(1);

	state = _machine->get_processor_state();
	XCTAssertEqual(state.status & Flag::ConditionCodes, 0xc & 0x1b);
	XCTAssertEqual(20, _machine->get_cycle_count());
}

// MARK: ANDI SR

- (void)testANDISR_supervisor {
	_machine->set_program({
		0x027c, 0x0700		// ANDI.W #$700, SR
	});
	_machine->set_initial_stack_pointer(300);

	_machine->run_for_instructions(1);

	const auto state = _machine->get_processor_state();
	XCTAssertEqual(state.program_counter, 0x1004 + 4);
	XCTAssertEqual(20, _machine->get_cycle_count());
}

- (void)testANDISR_user {
	_machine->set_program({
		0x46fc, 0x0000,		// MOVE 0, SR
		0x027c, 0x0700		// ANDI.W #$700, SR
	});

	_machine->run_for_instructions(2);

	const auto state = _machine->get_processor_state();
	XCTAssertNotEqual(state.program_counter, 0x1008 + 4);
//	XCTAssertEqual(34, _machine->get_cycle_count());
}

// MARK: EOR

- (void)testEORw {
	_machine->set_program({
		0xb744		// EOR.w D3, D4
	});

	auto state = _machine->get_processor_state();
	state.status |= Flag::Extend | Flag::Carry | Flag::Overflow;
	state.data[3] = 0x54ff0056;
	state.data[4] = 0x9853abcd;

	_machine->set_processor_state(state);
	_machine->run_for_instructions(1);

	state = _machine->get_processor_state();
	XCTAssertEqual(state.data[3], 0x54ff0056);
	XCTAssertEqual(state.data[4], 0x9853ab9b);
	XCTAssertEqual(4, _machine->get_cycle_count());
}

- (void)testEORl {
	_machine->set_program({
		0xb792		// EOR.l D3, (A2)
	});

	auto state = _machine->get_processor_state();
	state.address[2] = 0x3000;
	state.data[3] = 0x54ff0056;
	*_machine->ram_at(0x3000) = 0x0f0f;
	*_machine->ram_at(0x3002) = 0x0f11;

	_machine->set_processor_state(state);
	_machine->run_for_instructions(1);

	state = _machine->get_processor_state();
	XCTAssertEqual(state.data[3], 0x54ff0056);
	XCTAssertEqual(state.address[2], 0x3000);
	XCTAssertEqual(*_machine->ram_at(0x3000), 0x5bf0);
	XCTAssertEqual(*_machine->ram_at(0x3002), 0x0f47);
	XCTAssertEqual(state.status & Flag::ConditionCodes, 0);
	XCTAssertEqual(20, _machine->get_cycle_count());
}

// MARK: EORI

- (void)testEORIb {
	_machine->set_program({
		0x0a01, 0x0012		// EORI.B #$12, D1
	});

	auto state = _machine->get_processor_state();
	state.data[1] = 0x12345678;

	_machine->set_processor_state(state);
	_machine->run_for_instructions(1);

	state = _machine->get_processor_state();
	XCTAssertEqual(state.data[1], 0x1234566a);
	XCTAssertEqual(state.status & Flag::ConditionCodes, 0);
	XCTAssertEqual(8, _machine->get_cycle_count());
}

- (void)testEORIl {
	_machine->set_program({
		0x0ab8, 0xffff, 0x0000, 0x3000		// EORI.L #$ffff0000, ($3000).W
	});

	*_machine->ram_at(0x3000) = 0x0000;
	*_machine->ram_at(0x3002) = 0xffff;

	_machine->run_for_instructions(1);

	const auto state = _machine->get_processor_state();
	XCTAssertEqual(*_machine->ram_at(0x3000), 0xffff);
	XCTAssertEqual(*_machine->ram_at(0x3002), 0xffff);
	XCTAssertEqual(state.status & Flag::ConditionCodes, Flag::Negative);
	XCTAssertEqual(32, _machine->get_cycle_count());
}

// MARK: EORI to CCR

- (void)testEORICCR {
	_machine->set_program({
		0x0a3c, 0x001b		// EORI.b #$1b, CCR
	});

	auto state = _machine->get_processor_state();
	state.status |= 0xc;

	_machine->set_processor_state(state);
	_machine->run_for_instructions(1);

	state = _machine->get_processor_state();
	XCTAssertEqual(state.status & Flag::ConditionCodes, 0xc ^ 0x1b);
	XCTAssertEqual(20, _machine->get_cycle_count());
}

// MARK: EORI to SR

- (void)testEORISR_supervisor {
	_machine->set_program({
		0x0a7c, 0x0700		// EORI.W #$700, SR
	});
	_machine->set_initial_stack_pointer(300);

	_machine->run_for_instructions(1);

	const auto state = _machine->get_processor_state();
	XCTAssertEqual(state.program_counter, 0x1004 + 4);
	XCTAssertEqual(20, _machine->get_cycle_count());
}

- (void)testEORISR_user {
	_machine->set_program({
		0x46fc, 0x0000,		// MOVE 0, SR
		0x0a7c, 0x0700		// EORI.W #$700, SR
	});

	_machine->run_for_instructions(2);

	const auto state = _machine->get_processor_state();
	XCTAssertNotEqual(state.program_counter, 0x1008 + 4);
//	XCTAssertEqual(34, _machine->get_cycle_count());
}

// MARK: NOT

- (void)testNOTb {
	_machine->set_program({
		0x4600		// NOT.B D0
	});
	auto state = _machine->get_processor_state();
	state.data[0] = 0x12345678;

	_machine->set_processor_state(state);
	_machine->run_for_instructions(1);

	state = _machine->get_processor_state();
	XCTAssertEqual(state.data[0], 0x12345687);
	XCTAssertEqual(state.status & Flag::ConditionCodes, Flag::Negative);
	XCTAssertEqual(4, _machine->get_cycle_count());
}

- (void)testNOTw {
	_machine->set_program({
		0x4640		// NOT.w D0
	});
	auto state = _machine->get_processor_state();
	state.data[0] = 0x12340000;
	state.status |= Flag::ConditionCodes;

	_machine->set_processor_state(state);
	_machine->run_for_instructions(1);

	state = _machine->get_processor_state();
	XCTAssertEqual(state.data[0], 0x1234ffff);
	XCTAssertEqual(state.status & Flag::ConditionCodes, Flag::Negative | Flag::Extend);
	XCTAssertEqual(4, _machine->get_cycle_count());
}

- (void)testNOTl_Dn {
	_machine->set_program({
		0x4680		// NOT.l D0
	});
	auto state = _machine->get_processor_state();
	state.data[0] = 0xffffff00;
	state.status |= Flag::ConditionCodes;

	_machine->set_processor_state(state);
	_machine->run_for_instructions(1);

	state = _machine->get_processor_state();
	XCTAssertEqual(state.data[0], 0x000000ff);
	XCTAssertEqual(state.status & Flag::ConditionCodes, Flag::Extend);
	XCTAssertEqual(6, _machine->get_cycle_count());
}

- (void)testNOTl_XXXl {
	_machine->set_program({
		0x46b9, 0x0000, 0x3000		// NOT.L ($3000).L
	});
	*_machine->ram_at(0x3000) = 0xf001;
	*_machine->ram_at(0x3002) = 0x2311;
	auto state = _machine->get_processor_state();
	state.status |= Flag::Extend;

	_machine->set_processor_state(state);
	_machine->run_for_instructions(1);

	state = _machine->get_processor_state();
	XCTAssertEqual(*_machine->ram_at(0x3000), 0x0ffe);
	XCTAssertEqual(*_machine->ram_at(0x3002), 0xdcee);
	XCTAssertEqual(state.status & Flag::ConditionCodes, Flag::Extend);
	XCTAssertEqual(28, _machine->get_cycle_count());
}

// MARK: OR

- (void)testORb {
	_machine->set_program({
		0x8604		// OR.b D4, D3
	});
	auto state = _machine->get_processor_state();
	state.data[3] = 0x54ff0056;
	state.data[4] = 0x9853abcd;
	state.status |= Flag::Extend | Flag::Carry | Flag::Overflow;

	_machine->set_processor_state(state);
	_machine->run_for_instructions(1);

	state = _machine->get_processor_state();
	XCTAssertEqual(state.data[3], 0x54ff00df);
	XCTAssertEqual(state.data[4], 0x9853abcd);
	XCTAssertEqual(state.status & Flag::ConditionCodes, Flag::Extend | Flag::Negative);
	XCTAssertEqual(4, _machine->get_cycle_count());
}

- (void)testORl_toDn {
	_machine->set_program({
		0x86ac, 0xfffa		// OR.l -6(A4), D3
	});
	auto state = _machine->get_processor_state();
	state.data[3] = 0x54fff856;
	state.address[4] = 0x3006;
	*_machine->ram_at(0x3000) = 0x1253;
	*_machine->ram_at(0x3002) = 0xfb34;

	_machine->set_processor_state(state);
	_machine->run_for_instructions(1);

	state = _machine->get_processor_state();
	XCTAssertEqual(state.data[3], 0x56fffb76);
	XCTAssertEqual(state.address[4], 0x3006);
	XCTAssertEqual(*_machine->ram_at(0x3000), 0x1253);
	XCTAssertEqual(*_machine->ram_at(0x3002), 0xfb34);
	XCTAssertEqual(state.status & Flag::ConditionCodes, 0);
	XCTAssertEqual(18, _machine->get_cycle_count());
}

- (void)testORl_fromDn {
	_machine->set_program({
		0x87ac, 0xfffa		// OR.l D3, -6(A4)
	});
	auto state = _machine->get_processor_state();
	state.data[3] = 0x54fff856;
	state.address[4] = 0x3006;
	*_machine->ram_at(0x3000) = 0x1253;
	*_machine->ram_at(0x3002) = 0xfb34;

	_machine->set_processor_state(state);
	_machine->run_for_instructions(1);

	state = _machine->get_processor_state();
	XCTAssertEqual(state.data[3], 0x54fff856);
	XCTAssertEqual(state.address[4], 0x3006);
	XCTAssertEqual(*_machine->ram_at(0x3000), 0x56ff);
	XCTAssertEqual(*_machine->ram_at(0x3002), 0xfb76);
	XCTAssertEqual(state.status & Flag::ConditionCodes, 0);
	XCTAssertEqual(24, _machine->get_cycle_count());
}

// MARK: ORI

- (void)testORIb {
	_machine->set_program({
		0x0001, 0x0012		// ORI.B #$12, D1
	});

	auto state = _machine->get_processor_state();
	state.data[1] = 0x12345678;

	_machine->set_processor_state(state);
	_machine->run_for_instructions(1);

	state = _machine->get_processor_state();
	XCTAssertEqual(state.data[1], 0x1234567a);
	XCTAssertEqual(state.status & Flag::ConditionCodes, 0);
	XCTAssertEqual(8, _machine->get_cycle_count());
}

- (void)testORIl {
	_machine->set_program({
		0x00b8, 0xffff, 0x0000, 0x3000		// ORI.L #$ffff0000, ($3000).W
	});

	*_machine->ram_at(0x3000) = 0x0000;
	*_machine->ram_at(0x3002) = 0xffff;

	_machine->run_for_instructions(1);

	const auto state = _machine->get_processor_state();
	XCTAssertEqual(*_machine->ram_at(0x3000), 0xffff);
	XCTAssertEqual(*_machine->ram_at(0x3002), 0xffff);
	XCTAssertEqual(state.status & Flag::ConditionCodes, Flag::Negative);
	XCTAssertEqual(32, _machine->get_cycle_count());
}

// MARK: ORI to CCR

- (void)testORICCR {
	_machine->set_program({
		0x003c, 0x001b		// ORI.b #$1b, CCR
	});

	auto state = _machine->get_processor_state();
	state.status |= 0xc;

	_machine->set_processor_state(state);
	_machine->run_for_instructions(1);

	state = _machine->get_processor_state();
	XCTAssertEqual(state.status & Flag::ConditionCodes, 0xc | 0x1b);
	XCTAssertEqual(20, _machine->get_cycle_count());
}

// MARK: ORI to SR

- (void)testORISR_supervisor {
	_machine->set_program({
		0x007c, 0x0700		// ORI.W #$700, SR
	});
	_machine->set_initial_stack_pointer(300);

	_machine->run_for_instructions(1);

	const auto state = _machine->get_processor_state();
	XCTAssertEqual(state.program_counter, 0x1004 + 4);
	XCTAssertEqual(20, _machine->get_cycle_count());
}

- (void)testORISR_user {
	_machine->set_program({
		0x46fc, 0x0000,		// MOVE 0, SR
		0x007c, 0x0700		// ORI.W #$700, SR
	});

	_machine->run_for_instructions(2);

	const auto state = _machine->get_processor_state();
	XCTAssertNotEqual(state.program_counter, 0x1008 + 4);
//	XCTAssertEqual(34, _machine->get_cycle_count());
}

// MARK: TAS

- (void)performTASDnd0:(uint32_t)d0 expectedCCR:(uint16_t)ccr {
	_machine->set_program({
		0x4ac0		// TAS D0
	});

	auto state = _machine->get_processor_state();
	state.data[0] = d0;

	_machine->set_processor_state(state);
	_machine->run_for_instructions(1);

	state = _machine->get_processor_state();
	XCTAssertEqual(state.status & Flag::ConditionCodes, ccr);
	XCTAssertEqual(4, _machine->get_cycle_count());
}

- (void)testTAS_Dn_unset {
	[self performTASDnd0:0x12345678 expectedCCR:0];
}

- (void)testTAS_Dn_set {
	[self performTASDnd0:0x123456f0 expectedCCR:Flag::Negative];
}

- (void)testTAS_XXXl {
	_machine->set_program({
		0x4af9, 0x0000, 0x3000		// TAS ($3000).l
	});
	*_machine->ram_at(0x3000) = 0x1100;

	auto state = _machine->get_processor_state();
	state.status |= Flag::ConditionCodes;

	_machine->set_processor_state(state);
	_machine->run_for_instructions(1);

	state = _machine->get_processor_state();
	XCTAssertEqual(state.status & Flag::ConditionCodes, Flag::Extend);
	XCTAssertEqual(*_machine->ram_at(0x3000), 0x9100);
	XCTAssertEqual(22, _machine->get_cycle_count());
}

@end
