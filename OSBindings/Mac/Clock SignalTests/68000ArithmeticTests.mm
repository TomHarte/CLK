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

@interface M68000ArithmeticTests : XCTestCase
@end

@implementation M68000ArithmeticTests {
	std::unique_ptr<RAM68000> _machine;
}

- (void)setUp {
    _machine.reset(new RAM68000());
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

// MARK: ADD

- (void)testADDb {
	_machine->set_program({
		0x0602, 0xff		// ADD.B #$ff, D2
	});
	auto state = _machine->get_processor_state();
	state.data[2] = 0x9ae;
	_machine->set_processor_state(state);
	_machine->run_for_instructions(1);

	state = _machine->get_processor_state();
	XCTAssertEqual(state.status & Flag::ConditionCodes, Flag::Carry | Flag::Negative | Flag::Extend);
	XCTAssertEqual(state.data[2], 0x9ad);
	XCTAssertEqual(8, _machine->get_cycle_count());
}

- (void)testADDb_Overflow {
	_machine->set_program({
		0xd43c, 0x82		// ADD.B #$82, D2
	});
	auto state = _machine->get_processor_state();
	state.data[2] = 0x82;
	_machine->set_processor_state(state);
	_machine->run_for_instructions(1);

	state = _machine->get_processor_state();
	XCTAssertEqual(state.status & Flag::ConditionCodes, Flag::Overflow | Flag::Carry | Flag::Extend);
	XCTAssertEqual(state.data[2], 0x04);
	XCTAssertEqual(8, _machine->get_cycle_count());
}

- (void)testADDb_XXXw {
	_machine->set_program({
		0xd538, 0x3000		// ADD.B D2, ($3000).W
	});
	auto state = _machine->get_processor_state();
	state.data[2] = 0x82;
	*_machine->ram_at(0x3000) = 0x8200;
	_machine->set_processor_state(state);
	_machine->run_for_instructions(1);

	state = _machine->get_processor_state();
	XCTAssertEqual(state.status & Flag::ConditionCodes, Flag::Overflow | Flag::Carry | Flag::Extend);
	XCTAssertEqual(state.data[2], 0x82);
	XCTAssertEqual(*_machine->ram_at(0x3000), 0x0400);
	XCTAssertEqual(16, _machine->get_cycle_count());
}

- (void)testADDw_DnDn {
	_machine->set_program({
		0xd442		// ADD.W D2, D2
	});
	auto state = _machine->get_processor_state();
	state.data[2] = 0x3e8;
	_machine->set_processor_state(state);
	_machine->run_for_instructions(1);

	state = _machine->get_processor_state();
	XCTAssertEqual(state.data[2], 0x7D0);
	XCTAssertEqual(20, _machine->get_cycle_count());
}

- (void)testADDl_DnPostInc {
	_machine->set_program({
		0xd59a		// ADD.L D2, (A2)+
	});
	auto state = _machine->get_processor_state();
	state.data[2] = 0xb2d05e00;
	state.address[2] = 0x2000;
	*_machine->ram_at(0x2000) = 0x7735;
	*_machine->ram_at(0x2002) = 0x9400;

	_machine->set_processor_state(state);
	_machine->run_for_instructions(1);

	state = _machine->get_processor_state();
	XCTAssertEqual(state.data[2], 0xb2d05e00);
	XCTAssertEqual(*_machine->ram_at(0x2000), 0x2a05);
	XCTAssertEqual(*_machine->ram_at(0x2002), 0xf200);
	XCTAssertEqual(state.status & Flag::ConditionCodes, Flag::Carry | Flag::Extend);
	XCTAssertEqual(20, _machine->get_cycle_count());
}

- (void)testADDw_PreDec {
	_machine->set_program({
		0xd462		// ADD.W -(A2), D2
	});
	auto state = _machine->get_processor_state();
	state.data[2] = 0xFFFF0000;
	state.address[2] = 0x2002;
	*_machine->ram_at(0x2000) = 0;
	*_machine->ram_at(0x2002) = 0xffff;

	_machine->set_processor_state(state);
	_machine->run_for_instructions(1);

	state = _machine->get_processor_state();
	XCTAssertEqual(state.data[2], 0xFFFF0000);
	XCTAssertEqual(state.address[2], 0x2000);
	XCTAssertEqual(state.status & Flag::ConditionCodes, Flag::Zero);
	XCTAssertEqual(10, _machine->get_cycle_count());
}

/*
	ADD.B A2, D2 test omitted; no such opcode exists on the 68000.
	See P4-5 of the 68000PRM: An is defined for word and long only.
*/

- (void)testADDl_DnDn {
	_machine->set_program({
		0xd481		// ADD.l D1, D2
	});
	auto state = _machine->get_processor_state();
	state.data[1] = 0xfe35aab0;
	state.data[2] = 0x012557ac;

	_machine->set_processor_state(state);
	_machine->run_for_instructions(1);

	state = _machine->get_processor_state();
	XCTAssertEqual(state.data[1], 0xfe35aab0);
	XCTAssertEqual(state.data[2], 0xff5b025c);
	XCTAssertEqual(state.status & Flag::ConditionCodes, Flag::Negative);
	XCTAssertEqual(8, _machine->get_cycle_count());
}

// MARK: ADDA

- (void)testADDAL {
	_machine->set_program({
		0xd5fc, 0x1234, 0x5678		// ADDA.L #$12345678, A2
	});
	auto state = _machine->get_processor_state();
	state.address[2] = 0xae43ab1d;
	state.status = Flag::ConditionCodes;

	_machine->set_processor_state(state);
	_machine->run_for_instructions(1);

	state = _machine->get_processor_state();
	XCTAssertEqual(state.address[2], 0xc0780195);
	XCTAssertEqual(state.status & Flag::ConditionCodes, Flag::ConditionCodes);
}

- (void)testADDAWPositive {
	_machine->set_program({
		0xd4fc, 0x5678		// ADDA.W #$5678, A2
	});
	auto state = _machine->get_processor_state();
	state.address[2] = 0xae43ab1d;
	state.status = Flag::ConditionCodes;

	_machine->set_processor_state(state);
	_machine->run_for_instructions(1);

	state = _machine->get_processor_state();
	XCTAssertEqual(state.address[2], 0xae440195);
	XCTAssertEqual(state.status & Flag::ConditionCodes, Flag::ConditionCodes);
}

- (void)testADDAWNegative {
	_machine->set_program({
		0xd4fc, 0xf678		// ADDA.W #$f678, A2
	});
	auto state = _machine->get_processor_state();
	state.address[2] = 0xae43ab1d;
	state.status = Flag::ConditionCodes;

	_machine->set_processor_state(state);
	_machine->run_for_instructions(1);

	state = _machine->get_processor_state();
	XCTAssertEqual(state.address[2], 0xae43a195);
	XCTAssertEqual(state.status & Flag::ConditionCodes, Flag::ConditionCodes);
}

- (void)testADDAWNegative_2 {
	_machine->set_program({
		0xd4fc, 0xf000		// ADDA.W #$f000, A2
	});
	auto state = _machine->get_processor_state();
	state.status = Flag::ConditionCodes;

	_machine->set_processor_state(state);
	_machine->run_for_instructions(1);

	state = _machine->get_processor_state();
	XCTAssertEqual(state.address[2], 0xfffff000);
	XCTAssertEqual(state.status & Flag::ConditionCodes, Flag::ConditionCodes);
}

- (void)testADDALPreDec {
	_machine->set_program({
		0xd5e2				// ADDA.L -(A2), A2
	});
	auto state = _machine->get_processor_state();
	state.status = Flag::ConditionCodes;
	state.address[2] = 0x2004;
	*_machine->ram_at(0x2000) = 0x7002;
	*_machine->ram_at(0x2002) = 0;

	_machine->set_processor_state(state);
	_machine->run_for_instructions(1);

	state = _machine->get_processor_state();
	XCTAssertEqual(state.address[2], 0x70022000);
	XCTAssertEqual(*_machine->ram_at(0x2000), 0x7002);
	XCTAssertEqual(*_machine->ram_at(0x2002), 0x0000);
	XCTAssertEqual(state.status & Flag::ConditionCodes, Flag::ConditionCodes);
}

// MARL: ADDX

- (void)testADDXl_Dn {
	_machine->set_program({
		0xd581		// ADDX.l D1, D2
	});
	auto state = _machine->get_processor_state();
	state.data[1] = 0x12345678;
	state.data[2] = 0x12345678;
	state.status |= Flag::ConditionCodes;

	_machine->set_processor_state(state);
	_machine->run_for_instructions(1);

	state = _machine->get_processor_state();
	XCTAssertEqual(state.data[1], 0x12345678);
	XCTAssertEqual(state.data[2], 0x2468acf1);
	XCTAssertEqual(state.status & Flag::ConditionCodes, 0);
	XCTAssertEqual(8, _machine->get_cycle_count());
}

- (void)testADDXb {
	_machine->set_program({
		0xd501		// ADDX.b D1, D2
	});
	auto state = _machine->get_processor_state();
	state.data[1] = 0x80;
	state.data[2] = 0x8080;

	_machine->set_processor_state(state);
	_machine->run_for_instructions(1);

	state = _machine->get_processor_state();
	XCTAssertEqual(state.data[1], 0x80);
	XCTAssertEqual(state.data[2], 0x8000);
	XCTAssertEqual(state.status & Flag::ConditionCodes, Flag::Carry | Flag::Overflow | Flag::Extend);
	XCTAssertEqual(4, _machine->get_cycle_count());
}

- (void)testADDXw {
	_machine->set_program({
		0xd541		// ADDX.w D1, D2
	});
	auto state = _machine->get_processor_state();
	state.data[1] = 0x1ffff;
	state.data[2] = 0x18080;
	state.status |= Flag::ConditionCodes;

	_machine->set_processor_state(state);
	_machine->run_for_instructions(1);

	state = _machine->get_processor_state();
	XCTAssertEqual(state.data[1], 0x1ffff);
	XCTAssertEqual(state.data[2], 0x18080);
	XCTAssertEqual(state.status & Flag::ConditionCodes, Flag::Carry | Flag::Negative | Flag::Extend);
	XCTAssertEqual(4, _machine->get_cycle_count());
}

- (void)testADDXl_PreDec {
	_machine->set_program({
		0xd389		// ADDX.l -(A1), -(A1)
	});
	auto state = _machine->get_processor_state();
	state.address[1] = 0x3000;
	state.status |= Flag::ConditionCodes;
	*_machine->ram_at(0x2ff8) = 0x1000;
	*_machine->ram_at(0x2ffa) = 0x0000;
	*_machine->ram_at(0x2ffc) = 0x7000;
	*_machine->ram_at(0x2ffe) = 0x1ff1;

	_machine->set_processor_state(state);
	_machine->run_for_instructions(1);

	state = _machine->get_processor_state();
	XCTAssertEqual(state.address[1], 0x2ff8);
	XCTAssertEqual(state.status & Flag::ConditionCodes, Flag::Negative | Flag::Overflow);
	XCTAssertEqual(*_machine->ram_at(0x2ff8), 0x8000);
	XCTAssertEqual(*_machine->ram_at(0x2ffa), 0x1ff2);
	XCTAssertEqual(*_machine->ram_at(0x2ffc), 0x7000);
	XCTAssertEqual(*_machine->ram_at(0x2ffe), 0x1ff1);
	XCTAssertEqual(30, _machine->get_cycle_count());
}

// MARK: ADDI

- (void)testADDIl {
	_machine->set_program({
		0x0681, 0x1111, 0x1111		// ADDI.l #$11111111, D1
	});
	auto state = _machine->get_processor_state();
	state.data[1] = 0x300021b3;

	_machine->set_processor_state(state);
	_machine->run_for_instructions(1);

	state = _machine->get_processor_state();
	XCTAssertEqual(state.data[1], 0x411132C4);
	XCTAssertEqual(state.status & Flag::ConditionCodes, 0);
	XCTAssertEqual(16, _machine->get_cycle_count());
}

- (void)testADDIw {
	_machine->set_program({
		0x0678, 0x7aaa, 0x3000		// ADDI.W #$7aaa, ($3000).W
	});
	*_machine->ram_at(0x3000) = 0xaaaa;

	_machine->run_for_instructions(1);

	const auto state = _machine->get_processor_state();
	XCTAssertEqual(*_machine->ram_at(0x3000), 0x2554);
	XCTAssertEqual(state.status & Flag::ConditionCodes, Flag::Extend | Flag::Carry);
	XCTAssertEqual(20, _machine->get_cycle_count());
}

// MARK: ADDQ

- (void)testADDQl {
	_machine->set_program({
		0x5e81		// ADDQ.l #$7, D1
	});
	auto state = _machine->get_processor_state();
	state.data[1] = 0xffffffff;

	_machine->set_processor_state(state);
	_machine->run_for_instructions(1);

	state = _machine->get_processor_state();
	XCTAssertEqual(state.data[1], 0x6);
	XCTAssertEqual(state.status & Flag::ConditionCodes, Flag::Extend | Flag::Carry);
	XCTAssertEqual(8, _machine->get_cycle_count());
}

- (void)testADDQw_Dn {
	_machine->set_program({
		0x5641		// ADDQ.W #$3, D1
	});
	auto state = _machine->get_processor_state();
	state.data[1] = 0xfffffffe;

	_machine->set_processor_state(state);
	_machine->run_for_instructions(1);

	state = _machine->get_processor_state();
	XCTAssertEqual(state.data[1], 0xffff0001);
	XCTAssertEqual(state.status & Flag::ConditionCodes, Flag::Extend | Flag::Carry);
	XCTAssertEqual(4, _machine->get_cycle_count());
}

- (void)testADDQw_An {
	_machine->set_program({
		0x5649		// ADDQ.W #$3, A1
	});
	auto state = _machine->get_processor_state();
	state.address[1] = 0xfffffffe;

	_machine->set_processor_state(state);
	_machine->run_for_instructions(1);

	state = _machine->get_processor_state();
	XCTAssertEqual(state.address[1], 1);
	XCTAssertEqual(state.status & Flag::ConditionCodes, 0);
	XCTAssertEqual(8, _machine->get_cycle_count());
}

- (void)testADDQw_XXXw {
	_machine->set_program({
		0x5078, 0x3000		// ADDQ.W #$8, ($3000).W
	});
	*_machine->ram_at(0x3000) = 0x7ffd;

	_machine->run_for_instructions(1);

	const auto state = _machine->get_processor_state();
	XCTAssertEqual(*_machine->ram_at(0x3000), 0x8005);
	XCTAssertEqual(state.status & Flag::ConditionCodes, Flag::Negative | Flag::Overflow);
	XCTAssertEqual(16, _machine->get_cycle_count());
}

// MARK: CMP

- (void)performCMPb:(uint16_t)opcode expectedFlags:(uint16_t)flags {
	_machine->set_program({
		opcode
	});
	auto state = _machine->get_processor_state();
	state.data[1] = 0x1234567f;
	state.data[2] = 0x12345680;

	_machine->set_processor_state(state);
	_machine->run_for_instructions(1);

	state = _machine->get_processor_state();
	XCTAssertEqual(state.data[1], 0x1234567f);
	XCTAssertEqual(state.data[2], 0x12345680);
	XCTAssertEqual(state.status & Flag::ConditionCodes, flags);
	XCTAssertEqual(4, _machine->get_cycle_count());
}

- (void)testCMPb_D1D2 {
	[self performCMPb:0xb401 expectedFlags:Flag::Overflow];	// CMP.b D1, D2
}

- (void)testCMPb_D2D1 {
	[self performCMPb:0xb202 expectedFlags:Flag::Overflow | Flag::Negative | Flag::Carry];	// CMP.b D2, D1
}

- (void)performCMPwd1:(uint32_t)d1 d2:(uint32_t)d2 expectedFlags:(uint16_t)flags {
	_machine->set_program({
		0xb242		// CMP.W D2, D1
	});
	auto state = _machine->get_processor_state();
	state.data[1] = d1;
	state.data[2] = d2;

	_machine->set_processor_state(state);
	_machine->run_for_instructions(1);

	state = _machine->get_processor_state();
	XCTAssertEqual(state.data[1], d1);
	XCTAssertEqual(state.data[2], d2);
	XCTAssertEqual(state.status & Flag::ConditionCodes, flags);
	XCTAssertEqual(4, _machine->get_cycle_count());
}

- (void)testCMPw_8004v7002 {
	[self performCMPwd1:0x12347002 d2:0x12348004 expectedFlags:Flag::Overflow | Flag::Negative | Flag::Carry];
}

- (void)testCMPw_6666v5555 {
	[self performCMPwd1:0x55555555 d2:0x66666666 expectedFlags:Flag::Negative | Flag::Carry];
}

- (void)testCMPl {
	_machine->set_program({
		0xb282		// CMP.l D2, D1
	});
	auto state = _machine->get_processor_state();
	state.data[1] = 0x12347002;
	state.data[2] = 0x12348004;

	_machine->set_processor_state(state);
	_machine->run_for_instructions(1);

	state = _machine->get_processor_state();
	XCTAssertEqual(state.data[1], 0x12347002);
	XCTAssertEqual(state.data[2], 0x12348004);
	XCTAssertEqual(state.status & Flag::ConditionCodes, Flag::Negative | Flag::Carry);
	XCTAssertEqual(6, _machine->get_cycle_count());
}

// MARK: CMPA

- (void)performCMPAld1:(uint32_t)d1 a2:(uint32_t)a2 {
	_machine->set_program({
		0xb5c1		// CMPA.l D1, A2
	});
	auto state = _machine->get_processor_state();
	state.data[1] = d1;
	state.address[2] = a2;

	_machine->set_processor_state(state);
	_machine->run_for_instructions(1);

	state = _machine->get_processor_state();
	XCTAssertEqual(state.data[1], d1);
	XCTAssertEqual(state.address[2], a2);
	XCTAssertEqual(6, _machine->get_cycle_count());
}

- (void)testCMPAl_noFlags {
	[self performCMPAld1:0x1234567f a2:0x12345680];

	const auto state = _machine->get_processor_state();
	XCTAssertEqual(state.status & Flag::ConditionCodes, 0);
}

- (void)testCMPAl_carry {
	[self performCMPAld1:0xfd234577 a2:0x12345680];

	const auto state = _machine->get_processor_state();
	XCTAssertEqual(state.status & Flag::ConditionCodes, Flag::Carry);
}

- (void)testCMPAl_carryOverflowNegative {
	[self performCMPAld1:0x85678943 a2:0x22563245];

	const auto state = _machine->get_processor_state();
	XCTAssertEqual(state.status & Flag::ConditionCodes, Flag::Carry | Flag::Overflow | Flag::Negative);
}

- (void)performCMPAwd1:(uint32_t)d1 a2:(uint32_t)a2 {
	_machine->set_program({
		0xb4c1		// CMPA.w D1, A2
	});
	auto state = _machine->get_processor_state();
	state.data[1] = d1;
	state.address[2] = a2;

	_machine->set_processor_state(state);
	_machine->run_for_instructions(1);

	state = _machine->get_processor_state();
	XCTAssertEqual(state.data[1], d1);
	XCTAssertEqual(state.address[2], a2);
	XCTAssertEqual(6, _machine->get_cycle_count());
}

- (void)testCMPAw_carry {
	[self performCMPAwd1:0x85678943 a2:0x22563245];

	const auto state = _machine->get_processor_state();
	XCTAssertEqual(state.status & Flag::ConditionCodes, Flag::Carry);
}

- (void)testCMPAw_zero {
	[self performCMPAwd1:0x0000ffff a2:0xffffffff];

	const auto state = _machine->get_processor_state();
	XCTAssertEqual(state.status & Flag::ConditionCodes, Flag::Zero);
}

// MARK: CMPM

- (void)testCMPMl {
	_machine->set_program({
		0xb389		// CMPM.L (A1)+, (A1)+
	});
	auto state = _machine->get_processor_state();
	state.address[1] = 0x3000;
	state.status |= Flag::ConditionCodes;
	*_machine->ram_at(0x3000) = 0x7000;
	*_machine->ram_at(0x3002) = 0x1ff1;
	*_machine->ram_at(0x3004) = 0x1000;
	*_machine->ram_at(0x3006) = 0x0000;

	_machine->set_processor_state(state);
	_machine->run_for_instructions(1);

	state = _machine->get_processor_state();
	XCTAssertEqual(state.address[1], 0x3008);
	XCTAssertEqual(state.status & Flag::ConditionCodes, Flag::Negative | Flag::Extend | Flag::Carry);
	XCTAssertEqual(20, _machine->get_cycle_count());
}

- (void)testCMPMw {
	_machine->set_program({
		0xb549		// CMPM.w (A1)+, (A2)+
	});
	auto state = _machine->get_processor_state();
	state.address[1] = 0x3000;
	state.address[2] = 0x3002;
	state.status |= Flag::ConditionCodes;
	*_machine->ram_at(0x3000) = 0;
	*_machine->ram_at(0x3002) = 0;

	_machine->set_processor_state(state);
	_machine->run_for_instructions(1);

	state = _machine->get_processor_state();
	XCTAssertEqual(state.address[1], 0x3002);
	XCTAssertEqual(state.address[2], 0x3004);
	XCTAssertEqual(state.status & Flag::ConditionCodes, Flag::Zero | Flag::Extend);
	XCTAssertEqual(12, _machine->get_cycle_count());
}

- (void)testCMPMb {
	_machine->set_program({
		0xb509		// CMPM.b (A1)+, (A2)+
	});
	auto state = _machine->get_processor_state();
	state.address[1] = 0x3000;
	state.address[2] = 0x3001;
	state.status |= Flag::ConditionCodes;
	*_machine->ram_at(0x3000) = 0x807f;

	_machine->set_processor_state(state);
	_machine->run_for_instructions(1);

	state = _machine->get_processor_state();
	XCTAssertEqual(state.address[1], 0x3001);
	XCTAssertEqual(state.address[2], 0x3002);
	XCTAssertEqual(state.status & Flag::ConditionCodes, Flag::Negative | Flag::Extend | Flag::Carry | Flag::Overflow);
	XCTAssertEqual(12, _machine->get_cycle_count());
}

// MARK: DIVS

- (void)performDIVS:(uint16_t)divisor d1:(uint32_t)d1 {
	_machine->set_program({
		0x83fc, divisor		// DIVS #$eef0, D1
	});
	auto state = _machine->get_processor_state();
	state.data[1] = d1;
	state.status = Flag::ConditionCodes;

	_machine->set_processor_state(state);
	_machine->run_for_instructions(1);
}

- (void)performDIVSOverflowTestDivisor:(uint16_t)divisor {
	[self performDIVS:divisor d1:0x4768f231];

	const auto state = _machine->get_processor_state();
	XCTAssertEqual(state.data[1], 0x4768f231);
	XCTAssertEqual(state.status & Flag::ConditionCodes, Flag::Extend | Flag::Negative | Flag::Overflow);
//	XCTAssertEqual(20, _machine->get_cycle_count());
}

- (void)testDIVSOverflow {
	[self performDIVSOverflowTestDivisor:1];
}

- (void)testDIVSOverflow2 {
	[self performDIVSOverflowTestDivisor:0x1234];
}

- (void)testDIVSUnderflow {
	[self performDIVSOverflowTestDivisor:0xeeff];
}

- (void)testDIVS_2 {
	[self performDIVS:0xeef0 d1:0x0768f231];

	const auto state = _machine->get_processor_state();
	XCTAssertEqual(state.data[1], 0x026190D3);
	XCTAssertEqual(state.status & Flag::ConditionCodes, Flag::Extend | Flag::Negative);
//	XCTAssertEqual(138, _machine->get_cycle_count());
}

- (void)testDIVS_3 {
	[self performDIVS:0xffff d1:0xffffffff];

	const auto state = _machine->get_processor_state();
	XCTAssertEqual(state.data[1], 1);
	XCTAssertEqual(state.status & Flag::ConditionCodes, Flag::Extend);
//	XCTAssertEqual(158, _machine->get_cycle_count());
}

- (void)testDIVS_4 {
	[self performDIVS:0x3333 d1:0xffff0000];

	const auto state = _machine->get_processor_state();
	XCTAssertEqual(state.data[1], 0xfffffffb);
	XCTAssertEqual(state.status & Flag::ConditionCodes, Flag::Extend | Flag::Negative);
//	XCTAssertEqual(158, _machine->get_cycle_count());
}

- (void)testDIVS_5 {
	[self performDIVS:0x23 d1:0x8765];

	const auto state = _machine->get_processor_state();
	XCTAssertEqual(state.data[1], 0xb03de);
	XCTAssertEqual(state.status & Flag::ConditionCodes, Flag::Extend);
//	XCTAssertEqual(138, _machine->get_cycle_count());
}

- (void)testDIVS_6 {
	[self performDIVS:0x8765 d1:0x65];

	const auto state = _machine->get_processor_state();
	XCTAssertEqual(state.data[1], 0x650000);
	XCTAssertEqual(state.status & Flag::ConditionCodes, Flag::Extend | Flag::Zero);
//	XCTAssertEqual(156, _machine->get_cycle_count());
}

- (void)testDIVSExpensiveOverflow {
	// DIVS.W #$ffff, D1 alt
	[self performDIVS:0xffff d1:0x80000000];

	const auto state = _machine->get_processor_state();
	XCTAssertEqual(state.data[1], 0x80000000);
	XCTAssertEqual(state.status & Flag::ConditionCodes, Flag::Extend | Flag::Negative | Flag::Overflow);
//	XCTAssertEqual(158, _machine->get_cycle_count());
}

- (void)testDIVS_8 {
	// DIVS.W #$fffd, D1
	[self performDIVS:0xfffd d1:0xfffffffd];

	const auto state = _machine->get_processor_state();
	XCTAssertEqual(state.data[1], 0x1);
	XCTAssertEqual(state.status & Flag::ConditionCodes, Flag::Extend);
//	XCTAssertEqual(158, _machine->get_cycle_count());
}

- (void)testDIVS_9 {
	// DIVS.W #$7aee, D1
	[self performDIVS:0x7aee d1:0xdaaa00fe];

	const auto state = _machine->get_processor_state();
	XCTAssertEqual(state.data[1], 0xc97eb240);
	XCTAssertEqual(state.status & Flag::ConditionCodes, Flag::Extend | Flag::Negative);
//	XCTAssertEqual(148, _machine->get_cycle_count());
}

- (void)testDIVS_10 {
	// DIVS.W #$7fff, D1
	[self performDIVS:0x7fff d1:0x82f9fff];

	const auto state = _machine->get_processor_state();
	XCTAssertEqual(state.data[1], 0x305e105f);
	XCTAssertEqual(state.status & Flag::ConditionCodes, Flag::Extend);
//	XCTAssertEqual(142, _machine->get_cycle_count());
}

- (void)testDIVS_11 {
	// DIVS.W #$f32, D1
	[self performDIVS:0xf32 d1:0x00e1d44];

	const auto state = _machine->get_processor_state();
	XCTAssertEqual(state.data[1], 0x0bfa00ed);
	XCTAssertEqual(state.status & Flag::ConditionCodes, Flag::Extend);
//	XCTAssertEqual(144, _machine->get_cycle_count());
}

- (void)testDIVS_12 {
	// DIVS.W #$af32, D1
	[self performDIVS:0xaf32 d1:0xe1d44];

	const auto state = _machine->get_processor_state();
	XCTAssertEqual(state.data[1], 0x39dcffd4);
	XCTAssertEqual(state.status & Flag::ConditionCodes, Flag::Extend | Flag::Negative);
//	XCTAssertEqual(150, _machine->get_cycle_count());
}

- (void)testDIVSException {
	// DIVS.W #0, D1
	_machine->set_initial_stack_pointer(0);
	[self performDIVS:0x0 d1:0x1fffffff];

	const auto state = _machine->get_processor_state();
	XCTAssertEqual(state.data[1], 0x1fffffff);
	XCTAssertEqual(state.supervisor_stack_pointer, 0xfffffffa);
	XCTAssertEqual(state.status & Flag::ConditionCodes, Flag::Extend);
//	XCTAssertEqual(42, _machine->get_cycle_count());
}

// MARK: DIVU

- (void)performDIVU:(uint16_t)divisor d1:(uint32_t)d1 {
	_machine->set_program({
		0x82fc, divisor		// DIVU #$eef0, D1
	});
	auto state = _machine->get_processor_state();
	state.data[1] = d1;
	state.status |= Flag::ConditionCodes;

	_machine->set_processor_state(state);
	_machine->run_for_instructions(1);
}

- (void)testDIVU_0001 {
	[self performDIVU:1 d1:0x4768f231];

	const auto state = _machine->get_processor_state();
	XCTAssertEqual(state.data[1], 0x4768f231);
	XCTAssertEqual(state.status & Flag::ConditionCodes, Flag::Extend | Flag::Negative | Flag::Overflow);
	XCTAssertEqual(14, _machine->get_cycle_count());
}

- (void)testDIVU_1234 {
	[self performDIVU:0x1234 d1:0x4768f231];

	const auto state = _machine->get_processor_state();
	XCTAssertEqual(state.data[1], 0x4768f231);
	XCTAssertEqual(state.status & Flag::ConditionCodes, Flag::Extend | Flag::Negative | Flag::Overflow);
	XCTAssertEqual(14, _machine->get_cycle_count());
}

- (void)testDIVU_eeff {
	[self performDIVU:0xeeff d1:0x4768f231];

	const auto state = _machine->get_processor_state();
	XCTAssertEqual(state.data[1], 0x8bae4c7d);
	XCTAssertEqual(state.status & Flag::ConditionCodes, Flag::Extend);
	XCTAssertEqual(108, _machine->get_cycle_count());
}

- (void)testDIVU_3333 {
	[self performDIVU:0x3333 d1:0x1fffffff];

	const auto state = _machine->get_processor_state();
	XCTAssertEqual(state.data[1], 0x1fffa000);
	XCTAssertEqual(state.status & Flag::ConditionCodes, Flag::Extend | Flag::Negative);
	XCTAssertEqual(136, _machine->get_cycle_count());
}

// Omitted: divide by zero test.

// MARK: MULS

- (void)performMULSd1:(uint32_t)d1 d2:(uint32_t)d2 ccr:(uint8_t)ccr {
	_machine->set_program({
		0xc5c1		// MULS D1, D2
	});
	auto state = _machine->get_processor_state();
	state.data[1] = d1;
	state.data[2] = d2;
	state.status = ccr;

	_machine->set_processor_state(state);
	_machine->run_for_instructions(1);
}

- (void)performMULSConstant:(uint16_t)constant d2:(uint32_t)d2 {
	_machine->set_program({
		0xc5fc, constant		// MULS #constant, D2
	});
	auto state = _machine->get_processor_state();
	state.data[2] = d2;
	state.status = Flag::Carry | Flag::Extend | Flag::Overflow;

	_machine->set_processor_state(state);
	_machine->run_for_instructions(1);
}

- (void)testMULS {
	[self performMULSd1:0x12345678 d2:0x12345678 ccr:0];

	const auto state = _machine->get_processor_state();
	XCTAssertEqual(state.data[1], 0x12345678);
	XCTAssertEqual(state.data[2], 0x1d34d840);
	XCTAssertEqual(state.status & Flag::ConditionCodes, 0);
	XCTAssertEqual(54, _machine->get_cycle_count());
}

- (void)testMULS_2 {
	[self performMULSd1:0x82348678 d2:0x823486ff ccr:0];

	const auto state = _machine->get_processor_state();
	XCTAssertEqual(state.data[1], 0x82348678);
	XCTAssertEqual(state.data[2], 0x3971c188);
	XCTAssertEqual(state.status & Flag::ConditionCodes, 0);
	XCTAssertEqual(48, _machine->get_cycle_count());
}

- (void)testMULSZero {
	[self performMULSd1:1 d2:0 ccr:Flag::Carry | Flag::Overflow | Flag::Extend];

	const auto state = _machine->get_processor_state();
	XCTAssertEqual(state.data[1], 1);
	XCTAssertEqual(state.data[2], 0);
	XCTAssertEqual(state.status & Flag::ConditionCodes, Flag::Extend | Flag::Zero);
	XCTAssertEqual(42, _machine->get_cycle_count());
}

- (void)testMULSFFFF {
	[self performMULSConstant:0xffff d2:0xffff];

	const auto state = _machine->get_processor_state();
	XCTAssertEqual(state.data[2], 1);
	XCTAssertEqual(state.status & Flag::ConditionCodes, Flag::Extend);
	XCTAssertEqual(44, _machine->get_cycle_count());
}

- (void)testMULSNegative {
	[self performMULSConstant:0x1fff d2:0x8fff];

	const auto state = _machine->get_processor_state();
	XCTAssertEqual(state.data[2], 0xf2005001);
	XCTAssertEqual(state.status & Flag::ConditionCodes, Flag::Extend | Flag::Negative);
	XCTAssertEqual(46, _machine->get_cycle_count());
}

// MARK: MULU

- (void)performMULUd1:(uint32_t)d1 d2:(uint32_t)d2 ccr:(uint8_t)ccr {
	_machine->set_program({
		0xc4c1		// MULU D1, D2
	});
	auto state = _machine->get_processor_state();
	state.data[1] = d1;
	state.data[2] = d2;
	state.status |= ccr;

	_machine->set_processor_state(state);
	_machine->run_for_instructions(1);
}

- (void)testMULU_Dn {
	[self performMULUd1:0x12345678 d2:0x12345678 ccr:0];

	const auto state = _machine->get_processor_state();
	XCTAssertEqual(state.data[2], 0x1d34d840);
	XCTAssertEqual(state.status & Flag::ConditionCodes, 0);
	XCTAssertEqual(54, _machine->get_cycle_count());
}

- (void)testMULU_Dn_Zero {
	[self performMULUd1:1 d2:0 ccr:Flag::Extend | Flag::Overflow | Flag::Carry];

	const auto state = _machine->get_processor_state();
	XCTAssertEqual(state.data[1], 1);
	XCTAssertEqual(state.data[2], 0);
	XCTAssertEqual(state.status & Flag::ConditionCodes, Flag::Extend | Flag::Zero);
	XCTAssertEqual(40, _machine->get_cycle_count());
}

- (void)testMULU_Imm {
	_machine->set_program({
		0xc4fc, 0xffff		// MULU.W    #$ffff, D2
	});
	auto state = _machine->get_processor_state();
	state.data[2] = 0xffff;
	state.status |= Flag::Extend | Flag::Overflow | Flag::Carry;

	_machine->set_processor_state(state);
	_machine->run_for_instructions(1);

	state = _machine->get_processor_state();
	XCTAssertEqual(state.data[2], 0xfffe0001);
	XCTAssertEqual(state.status & Flag::ConditionCodes, Flag::Extend | Flag::Negative);
	XCTAssertEqual(74, _machine->get_cycle_count());
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

// MARK: NEG

- (void)performNEGb:(uint32_t)value {
	_machine->set_program({
		0x4400		// NEG.b D0
	});
	auto state = _machine->get_processor_state();
	state.data[0] = value;

	_machine->set_processor_state(state);
	_machine->run_for_instructions(1);

	XCTAssertEqual(4, _machine->get_cycle_count());
}

- (void)testNEGb_78 {
	[self performNEGb:0x12345678];

	const auto state = _machine->get_processor_state();
	XCTAssertEqual(state.data[0], 0x12345688);
	XCTAssertEqual(state.status & Flag::ConditionCodes, Flag::Carry | Flag::Extend | Flag::Negative);
}

- (void)testNEGb_00 {
	[self performNEGb:0x12345600];

	const auto state = _machine->get_processor_state();
	XCTAssertEqual(state.data[0], 0x12345600);
	XCTAssertEqual(state.status & Flag::ConditionCodes, Flag::Zero);
}

- (void)testNEGb_80 {
	[self performNEGb:0x12345680];

	const auto state = _machine->get_processor_state();
	XCTAssertEqual(state.data[0], 0x12345680);
	XCTAssertEqual(state.status & Flag::ConditionCodes, Flag::Negative | Flag::Overflow | Flag::Extend | Flag::Carry);
}

- (void)testNEGw {
	_machine->set_program({
		0x4440		// NEG.w D0
	});
	auto state = _machine->get_processor_state();
	state.data[0] = 0x12348000;

	_machine->set_processor_state(state);
	_machine->run_for_instructions(1);

	state = _machine->get_processor_state();
	XCTAssertEqual(4, _machine->get_cycle_count());
	XCTAssertEqual(state.data[0], 0x12348000);
	XCTAssertEqual(state.status & Flag::ConditionCodes, Flag::Negative | Flag::Overflow | Flag::Extend | Flag::Carry);
}

- (void)performNEGl:(uint32_t)value {
	_machine->set_program({
		0x4480		// NEG.l D0
	});
	auto state = _machine->get_processor_state();
	state.data[0] = value;

	_machine->set_processor_state(state);
	_machine->run_for_instructions(1);

	XCTAssertEqual(6, _machine->get_cycle_count());
}

- (void)testNEGl_large {
	[self performNEGl:0x12345678];

	const auto state = _machine->get_processor_state();
	XCTAssertEqual(state.data[0], 0xedcba988);
	XCTAssertEqual(state.status & Flag::ConditionCodes, Flag::Carry | Flag::Extend | Flag::Negative);
}

- (void)testNEGl_small {
	[self performNEGl:0xffffffff];

	const auto state = _machine->get_processor_state();
	XCTAssertEqual(state.data[0], 0x1);
	XCTAssertEqual(state.status & Flag::ConditionCodes, Flag::Carry | Flag::Extend);
}

- (void)testNEGl_XXXl {
	_machine->set_program({
		0x44b9, 0x0000, 0x3000		// NEG.L ($3000).L
	});
	*_machine->ram_at(0x3000) = 0xf001;
	*_machine->ram_at(0x3002) = 0x2311;

	_machine->run_for_instructions(1);

	const auto state = _machine->get_processor_state();
	XCTAssertEqual(28, _machine->get_cycle_count());
	XCTAssertEqual(*_machine->ram_at(0x3000), 0x0ffe);
	XCTAssertEqual(*_machine->ram_at(0x3002), 0xdcef);
	XCTAssertEqual(state.status & Flag::ConditionCodes, Flag::Extend | Flag::Carry);
}

// MARK: NEGX

- (void)performNEGXb:(uint32_t)value {
	_machine->set_program({
		0x4000		// NEGX.b D0
	});
	auto state = _machine->get_processor_state();
	state.data[0] = value;
	state.status |= Flag::Extend;

	_machine->set_processor_state(state);
	_machine->run_for_instructions(1);

	XCTAssertEqual(4, _machine->get_cycle_count());
}

- (void)testNEGXb_78 {
	[self performNEGXb:0x12345678];

	const auto state = _machine->get_processor_state();
	XCTAssertEqual(state.data[0], 0x12345687);
	XCTAssertEqual(state.status & Flag::ConditionCodes, Flag::Carry | Flag::Extend | Flag::Negative);
}

- (void)testNEGXb_00 {
	[self performNEGXb:0x12345600];

	const auto state = _machine->get_processor_state();
	XCTAssertEqual(state.data[0], 0x123456ff);
	XCTAssertEqual(state.status & Flag::ConditionCodes, Flag::Carry | Flag::Extend | Flag::Negative);
}

- (void)testNEGXb_80 {
	[self performNEGXb:0x12345680];

	const auto state = _machine->get_processor_state();
	XCTAssertEqual(state.data[0], 0x1234567f);
	XCTAssertEqual(state.status & Flag::ConditionCodes, Flag::Extend | Flag::Carry);
}

- (void)testNEGXw {
	_machine->set_program({
		0x4040		// NEGX.w D0
	});
	auto state = _machine->get_processor_state();
	state.data[0] = 0x12348000;
	state.status |= Flag::Extend;

	_machine->set_processor_state(state);
	_machine->run_for_instructions(1);

	state = _machine->get_processor_state();
	XCTAssertEqual(4, _machine->get_cycle_count());
	XCTAssertEqual(state.data[0], 0x12347fff);
	XCTAssertEqual(state.status & Flag::ConditionCodes, Flag::Extend | Flag::Carry);
}

- (void)performNEGXl:(uint32_t)value {
	_machine->set_program({
		0x4080		// NEGX.l D0
	});
	auto state = _machine->get_processor_state();
	state.data[0] = value;
	state.status |= Flag::Extend;

	_machine->set_processor_state(state);
	_machine->run_for_instructions(1);

	XCTAssertEqual(6, _machine->get_cycle_count());
}

- (void)testNEGXl_large {
	[self performNEGXl:0x12345678];

	const auto state = _machine->get_processor_state();
	XCTAssertEqual(state.data[0], 0xedcba987);
	XCTAssertEqual(state.status & Flag::ConditionCodes, Flag::Carry | Flag::Extend | Flag::Negative);
}

- (void)testNEGXl_small {
	[self performNEGXl:0xffffffff];

	const auto state = _machine->get_processor_state();
	XCTAssertEqual(state.data[0], 0x0);
	XCTAssertEqual(state.status & Flag::ConditionCodes, Flag::Carry | Flag::Extend);
}

- (void)testNEGXl_XXXl {
	_machine->set_program({
		0x40b9, 0x0000, 0x3000		// NEGX.L ($3000).L
	});
	*_machine->ram_at(0x3000) = 0xf001;
	*_machine->ram_at(0x3002) = 0x2311;
	auto state = _machine->get_processor_state();
	state.status |= Flag::Extend;

	_machine->set_processor_state(state);
	_machine->run_for_instructions(1);

	state = _machine->get_processor_state();
	XCTAssertEqual(28, _machine->get_cycle_count());
	XCTAssertEqual(*_machine->ram_at(0x3000), 0x0ffe);
	XCTAssertEqual(*_machine->ram_at(0x3002), 0xdcee);
	XCTAssertEqual(state.status & Flag::ConditionCodes, Flag::Extend | Flag::Carry);
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

// MARK: SUB

- (void)performSUBbIMM:(uint16_t)value d2:(uint32_t)d2 {
	_machine->set_program({
		0x0402, value		// SUB.b #value, D2
	});
	auto state = _machine->get_processor_state();
	state.data[2] = d2;

	_machine->set_processor_state(state);
	_machine->run_for_instructions(1);

	XCTAssertEqual(8, _machine->get_cycle_count());
}

- (void)testSUBb_IMM_ff {
	[self performSUBbIMM:0xff d2:0x9ae];

	const auto state = _machine->get_processor_state();
	XCTAssertEqual(state.data[2], 0x9af);
	XCTAssertEqual(state.status & Flag::ConditionCodes, Flag::Carry | Flag::Negative | Flag::Extend);
}

- (void)testSUBb_IMM_82 {
	[self performSUBbIMM:0x82 d2:0x0a];

	const auto state = _machine->get_processor_state();
	XCTAssertEqual(state.data[2], 0x88);
	XCTAssertEqual(state.status & Flag::ConditionCodes, Flag::Carry | Flag::Negative | Flag::Overflow | Flag::Extend);
}

- (void)testSUBb_IMM_f0 {
	[self performSUBbIMM:0xf0 d2:0x64];

	const auto state = _machine->get_processor_state();
	XCTAssertEqual(state.data[2], 0x74);
	XCTAssertEqual(state.status & Flag::ConditionCodes, Flag::Carry | Flag::Extend);
}

- (void)testSUBb_IMM_28 {
	[self performSUBbIMM:0x28 d2:0xff96];

	const auto state = _machine->get_processor_state();
	XCTAssertEqual(state.data[2], 0xff6e);
	XCTAssertEqual(state.status & Flag::ConditionCodes, Flag::Overflow);
}

- (void)testSUBb_PreDec {
	_machine->set_program({
		0x9427		// SUB.b -(A7), D2
	});
	_machine->set_initial_stack_pointer(0x2002);
	auto state = _machine->get_processor_state();
	state.data[2] = 0x9c40;
	*_machine->ram_at(0x2000) = 0x2710;

	_machine->set_processor_state(state);
	_machine->run_for_instructions(1);

	state = _machine->get_processor_state();
	XCTAssertEqual(10, _machine->get_cycle_count());
	XCTAssertEqual(state.data[2], 0x9c19);
	XCTAssertEqual(state.stack_pointer(), 0x2000);
}

// Omitted: SUB.w -(A6), D2, which is designed to trigger an address error.

- (void)testSUBw_XXXw {
	_machine->set_program({
		0x9578, 0x3000		// SUB.w D2, ($3000).w
	});
	auto state = _machine->get_processor_state();
	state.data[2] = 0x2711;
	*_machine->ram_at(0x3000) = 0x759f;

	_machine->set_processor_state(state);
	_machine->run_for_instructions(1);

	state = _machine->get_processor_state();
	XCTAssertEqual(16, _machine->get_cycle_count());
	XCTAssertEqual(state.data[2], 0x2711);
	XCTAssertEqual(state.status & Flag::ConditionCodes, 0);
	XCTAssertEqual(*_machine->ram_at(0x3000), 0x4e8e);
}

- (void)testSUBl_dAn {
	_machine->set_program({
		0x95ab, 0x0004		// SUB.l D2, 4(A3)
	});
	auto state = _machine->get_processor_state();
	state.data[2] = 0x45fd5ab4;
	state.address[3] = 0x3000;
	*_machine->ram_at(0x3004) = 0x327a;
	*_machine->ram_at(0x3006) = 0x4ef3;

	_machine->set_processor_state(state);
	_machine->run_for_instructions(1);

	state = _machine->get_processor_state();
	XCTAssertEqual(24, _machine->get_cycle_count());
	XCTAssertEqual(state.data[2], 0x45fd5ab4);
	XCTAssertEqual(state.address[3], 0x3000);
	XCTAssertEqual(state.status & Flag::ConditionCodes, Flag::Carry | Flag::Extend | Flag::Negative);
	XCTAssertEqual(*_machine->ram_at(0x3004), 0xec7c);
	XCTAssertEqual(*_machine->ram_at(0x3006), 0xf43f);
}

// MARK: SUBA

- (void)testSUBAl_Imm {
	_machine->set_program({
		0x95fc, 0x1234, 0x5678		// SUBA.l #$12345678, A2
	});
	auto state = _machine->get_processor_state();
	state.address[2] = 0xae43ab1d;
	state.status |= Flag::ConditionCodes;

	_machine->set_processor_state(state);
	_machine->run_for_instructions(1);

	state = _machine->get_processor_state();
	XCTAssertEqual(16, _machine->get_cycle_count());
	XCTAssertEqual(state.address[2], 0x9c0f54a5);
	XCTAssertEqual(state.status & Flag::ConditionCodes, Flag::ConditionCodes);
}

- (void)testSUBAw_ImmPositive {
	_machine->set_program({
		0x94fc, 0x5678		// SUBA.w #$5678, A2
	});
	auto state = _machine->get_processor_state();
	state.address[2] = 0xae43ab1d;

	_machine->set_processor_state(state);
	_machine->run_for_instructions(1);

	state = _machine->get_processor_state();
	XCTAssertEqual(12, _machine->get_cycle_count());
	XCTAssertEqual(state.address[2], 0xae4354a5);
	XCTAssertEqual(state.status & Flag::ConditionCodes, 0);
}

- (void)testSUBAw_ImmNegative {
	_machine->set_program({
		0x94fc, 0xf678		// SUBA.w #$5678, A2
	});
	auto state = _machine->get_processor_state();
	state.address[2] = 0xae43ab1d;

	_machine->set_processor_state(state);
	_machine->run_for_instructions(1);

	state = _machine->get_processor_state();
	XCTAssertEqual(12, _machine->get_cycle_count());
	XCTAssertEqual(state.address[2], 0xae43b4a5);
	XCTAssertEqual(state.status & Flag::ConditionCodes, 0);
}

- (void)testSUBAw_PreDec {
	_machine->set_program({
		0x95e2		// SUBA.l -(A2), A2
	});
	auto state = _machine->get_processor_state();
	state.address[2] = 0x2004;
	*_machine->ram_at(0x2000) = 0x7002;
	*_machine->ram_at(0x2002) = 0x0000;

	_machine->set_processor_state(state);
	_machine->run_for_instructions(1);

	state = _machine->get_processor_state();
	XCTAssertEqual(16, _machine->get_cycle_count());
	XCTAssertEqual(state.address[2], 0x8ffe2000);
	XCTAssertEqual(state.status & Flag::ConditionCodes, 0);
}

// MARK: SUBI

- (void)testSUBIl {
	_machine->set_program({
		0x0481, 0xf111, 0x1111		// SUBI.L #$f1111111, D1
	});
	auto state = _machine->get_processor_state();
	state.data[1] = 0x300021b3;

	_machine->set_processor_state(state);
	_machine->run_for_instructions(1);

	state = _machine->get_processor_state();
	XCTAssertEqual(16, _machine->get_cycle_count());
	XCTAssertEqual(state.data[1], 0x3eef10a2);
	XCTAssertEqual(state.status & Flag::ConditionCodes, Flag::Carry | Flag::Extend);
}

// Test of SUBI.W #$7aaa, ($3000).W omitted; that doesn't appear to be a valid instruction?

// MARK: SUBQ

- (void)testSUBQl {
	_machine->set_program({
		0x5f81		// SUBQ.L #$7, D1
	});
	auto state = _machine->get_processor_state();
	state.data[1] = 0xffffffff;

	_machine->set_processor_state(state);
	_machine->run_for_instructions(1);

	state = _machine->get_processor_state();
	XCTAssertEqual(8, _machine->get_cycle_count());
	XCTAssertEqual(state.data[1], 0xfffffff8);
	XCTAssertEqual(state.status & Flag::ConditionCodes, Flag::Negative);
}

- (void)testSUBQw {
	_machine->set_program({
		0x5f49		// SUBQ.W #$7, A1
	});
	auto state = _machine->get_processor_state();
	state.address[1] = 0xffff0001;

	_machine->set_processor_state(state);
	_machine->run_for_instructions(1);

	state = _machine->get_processor_state();
	XCTAssertEqual(8, _machine->get_cycle_count());
	XCTAssertEqual(state.address[1], 0xfffefffa);
	XCTAssertEqual(state.status & Flag::ConditionCodes, 0);
}

- (void)testSUBQb {
	_machine->set_program({
		0x5f38, 0x3000		// SUBQ.b #$7, ($3000).w
	});
	*_machine->ram_at(0x3000) = 0x0600;

	_machine->run_for_instructions(1);

	const auto state = _machine->get_processor_state();
	XCTAssertEqual(16, _machine->get_cycle_count());
	XCTAssertEqual(*_machine->ram_at(0x3000), 0xff00);
	XCTAssertEqual(state.status & Flag::ConditionCodes, Flag::Extend | Flag::Negative | Flag::Carry);
}

// MARK: SUBX

- (void)testSUBXl_Dn {
	_machine->set_program({
		0x9581		// SUBX.l D1, D2
	});
	auto state = _machine->get_processor_state();
	state.data[1] = 0x12345678;
	state.data[2] = 0x12345678;
	state.status |= Flag::ConditionCodes;

	_machine->set_processor_state(state);
	_machine->run_for_instructions(1);

	state = _machine->get_processor_state();
	XCTAssertEqual(8, _machine->get_cycle_count());
	XCTAssertEqual(state.data[1], 0x12345678);
	XCTAssertEqual(state.data[2], 0xffffffff);
	XCTAssertEqual(state.status & Flag::ConditionCodes, Flag::Extend | Flag::Carry | Flag::Negative);
}

- (void)testSUBXb_Dn {
	_machine->set_program({
		0x9501		// SUBX.b D1, D2
	});
	auto state = _machine->get_processor_state();
	state.data[1] = 0x80;
	state.data[2] = 0x01;

	_machine->set_processor_state(state);
	_machine->run_for_instructions(1);

	state = _machine->get_processor_state();
	XCTAssertEqual(4, _machine->get_cycle_count());
	XCTAssertEqual(state.data[1], 0x80);
	XCTAssertEqual(state.data[2], 0x81);
	XCTAssertEqual(state.status & Flag::ConditionCodes, Flag::Extend | Flag::Carry | Flag::Negative | Flag::Overflow);
}

- (void)testSUBXl_PreDec {
	_machine->set_program({
		0x9389		// SUBX.l -(A1), -(A1)
	});
	auto state = _machine->get_processor_state();
	state.address[1] = 0x3000;
	*_machine->ram_at(0x2ff8) = 0x1000;
	*_machine->ram_at(0x2ffa) = 0x0000;
	*_machine->ram_at(0x2ffc) = 0x7000;
	*_machine->ram_at(0x2ffe) = 0x1ff1;
	state.status |= Flag::ConditionCodes;

	_machine->set_processor_state(state);
	_machine->run_for_instructions(1);

	state = _machine->get_processor_state();
	XCTAssertEqual(30, _machine->get_cycle_count());
	XCTAssertEqual(state.address[1], 0x2ff8);
	XCTAssertEqual(state.status & Flag::ConditionCodes, Flag::Extend | Flag::Carry | Flag::Negative );
	XCTAssertEqual(*_machine->ram_at(0x2ff8), 0x9fff);
	XCTAssertEqual(*_machine->ram_at(0x2ffa), 0xe00e);
	XCTAssertEqual(*_machine->ram_at(0x2ffc), 0x7000);
	XCTAssertEqual(*_machine->ram_at(0x2ffe), 0x1ff1);
}

@end
