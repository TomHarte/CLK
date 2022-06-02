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

- (RAM68000 *)machine {
	return _machine.get();
}

- (void)setUp {
	_machine = std::make_unique<RAM68000>();
}

- (void)tearDown {
	_machine.reset();
}

@end

// MARK: - ADD

@interface M68000ADDTests : M68000ArithmeticTests
@end
@implementation M68000ADDTests

- (void)testADDb {
	self.machine->set_program({
		0x0602, 0xff		// ADD.B #$ff, D2
	});
	self.machine->set_registers([=](auto &registers) {
		registers.data[2] = 0x9ae;
	});
	self.machine->run_for_instructions(1);

	const auto state = self.machine->get_processor_state();
	XCTAssertEqual(state.registers.status & ConditionCode::AllConditions, ConditionCode::Carry | ConditionCode::Negative | ConditionCode::Extend);
	XCTAssertEqual(state.registers.data[2], 0x9ad);
	XCTAssertEqual(8, self.machine->get_cycle_count());
}

- (void)testADDb_Overflow {
	self.machine->set_program({
		0xd43c, 0x82		// ADD.B #$82, D2
	});
	self.machine->set_registers([=](auto &registers) {
		registers.data[2] = 0x82;
	});
	self.machine->run_for_instructions(1);

	const auto state = self.machine->get_processor_state();
	XCTAssertEqual(state.registers.status & ConditionCode::AllConditions, ConditionCode::Overflow | ConditionCode::Carry | ConditionCode::Extend);
	XCTAssertEqual(state.registers.data[2], 0x04);
	XCTAssertEqual(8, self.machine->get_cycle_count());
}

- (void)testADDb_XXXw {
	self.machine->set_program({
		0xd538, 0x3000		// ADD.B D2, ($3000).W
	});
	self.machine->set_registers([=](auto &registers) {
		registers.data[2] = 0x82;
	});
	*self.machine->ram_at(0x3000) = 0x8200;
	self.machine->run_for_instructions(1);

	const auto state = self.machine->get_processor_state();
	XCTAssertEqual(state.registers.status & ConditionCode::AllConditions, ConditionCode::Overflow | ConditionCode::Carry | ConditionCode::Extend);
	XCTAssertEqual(state.registers.data[2], 0x82);
	XCTAssertEqual(*self.machine->ram_at(0x3000), 0x0400);
	XCTAssertEqual(16, self.machine->get_cycle_count());
}

- (void)testADDw_DnDn {
	self.machine->set_program({
		0xd442		// ADD.W D2, D2
	});
	self.machine->set_registers([=](auto &registers) {
		registers.data[2] = 0x3e8;
	});
	self.machine->run_for_instructions(1);

	const auto state = self.machine->get_processor_state();
	XCTAssertEqual(state.registers.data[2], 0x7D0);
	XCTAssertEqual(4, self.machine->get_cycle_count());
}

- (void)testADDl_DnPostInc {
	self.machine->set_program({
		0xd59a		// ADD.L D2, (A2)+
	});
	self.machine->set_registers([=](auto &registers) {
		registers.data[2] = 0xb2d05e00;
		registers.address[2] = 0x2000;
	});
	*self.machine->ram_at(0x2000) = 0x7735;
	*self.machine->ram_at(0x2002) = 0x9400;
	self.machine->run_for_instructions(1);

	const auto state = self.machine->get_processor_state();
	XCTAssertEqual(state.registers.data[2], 0xb2d05e00);
	XCTAssertEqual(*self.machine->ram_at(0x2000), 0x2a05);
	XCTAssertEqual(*self.machine->ram_at(0x2002), 0xf200);
	XCTAssertEqual(state.registers.status & ConditionCode::AllConditions, ConditionCode::Carry | ConditionCode::Extend);
	XCTAssertEqual(20, self.machine->get_cycle_count());
}

- (void)testADDw_PreDec {
	self.machine->set_program({
		0xd462		// ADD.W -(A2), D2
	});
	self.machine->set_registers([=](auto &registers) {
		registers.data[2] = 0xFFFF0000;
		registers.address[2] = 0x2002;
	});
	*self.machine->ram_at(0x2000) = 0;
	*self.machine->ram_at(0x2002) = 0xffff;
	self.machine->run_for_instructions(1);

	const auto state = self.machine->get_processor_state();
	XCTAssertEqual(state.registers.data[2], 0xFFFF0000);
	XCTAssertEqual(state.registers.address[2], 0x2000);
	XCTAssertEqual(state.registers.status & ConditionCode::AllConditions, ConditionCode::Zero);
	XCTAssertEqual(10, self.machine->get_cycle_count());
}

/*
	ADD.B A2, D2 test omitted; no such opcode exists on the 68000.
	See P4-5 of the 68000PRM: An is defined for word and long only.
*/

- (void)testADDl_DnDn {
	self.machine->set_program({
		0xd481		// ADD.l D1, D2
	});
	self.machine->set_registers([=](auto &registers) {
		registers.data[1] = 0xfe35aab0;
		registers.data[2] = 0x012557ac;
	});
	self.machine->run_for_instructions(1);

	const auto state = self.machine->get_processor_state();
	XCTAssertEqual(state.registers.data[1], 0xfe35aab0);
	XCTAssertEqual(state.registers.data[2], 0xff5b025c);
	XCTAssertEqual(state.registers.status & ConditionCode::AllConditions, ConditionCode::Negative);
	XCTAssertEqual(8, self.machine->get_cycle_count());
}

@end

// MARK: - ADDA

@interface M68000ADDATests : M68000ArithmeticTests
@end
@implementation M68000ADDATests

- (void)testADDAL {
	self.machine->set_program({
		0xd5fc, 0x1234, 0x5678		// ADDA.L #$12345678, A2
	});
	self.machine->set_registers([=](auto &registers) {
		registers.address[2] = 0xae43ab1d;
		registers.status = ConditionCode::AllConditions;
	});
	self.machine->run_for_instructions(1);

	const auto state = self.machine->get_processor_state();
	XCTAssertEqual(state.registers.address[2], 0xc0780195);
	XCTAssertEqual(state.registers.status & ConditionCode::AllConditions, ConditionCode::AllConditions);
}

- (void)testADDAWPositive {
	self.machine->set_program({
		0xd4fc, 0x5678		// ADDA.W #$5678, A2
	});
	self.machine->set_registers([=](auto &registers) {
		registers.address[2] = 0xae43ab1d;
		registers.status = ConditionCode::AllConditions;
	});
	self.machine->run_for_instructions(1);

	const auto state = self.machine->get_processor_state();
	XCTAssertEqual(state.registers.address[2], 0xae440195);
	XCTAssertEqual(state.registers.status & ConditionCode::AllConditions, ConditionCode::AllConditions);
}

- (void)testADDAWNegative {
	self.machine->set_program({
		0xd4fc, 0xf678		// ADDA.W #$f678, A2
	});
	self.machine->set_registers([=](auto &registers) {
		registers.address[2] = 0xae43ab1d;
		registers.status = ConditionCode::AllConditions;
	});
	self.machine->run_for_instructions(1);

	const auto state = self.machine->get_processor_state();
	XCTAssertEqual(state.registers.address[2], 0xae43a195);
	XCTAssertEqual(state.registers.status & ConditionCode::AllConditions, ConditionCode::AllConditions);
}

- (void)testADDAWNegative_2 {
	self.machine->set_program({
		0xd4fc, 0xf000		// ADDA.W #$f000, A2
	});
	self.machine->set_registers([=](auto &registers) {
		registers.status = ConditionCode::AllConditions;
		registers.address[2] = 0;
	});
	self.machine->run_for_instructions(1);

	const auto state = self.machine->get_processor_state();
	XCTAssertEqual(state.registers.address[2], 0xfffff000);
	XCTAssertEqual(state.registers.status & ConditionCode::AllConditions, ConditionCode::AllConditions);
}

- (void)testADDALPreDec {
	self.machine->set_program({
		0xd5e2				// ADDA.L -(A2), A2
	});
	self.machine->set_registers([=](auto &registers) {
		registers.status = ConditionCode::AllConditions;
		registers.address[2] = 0x2004;
	});
	*self.machine->ram_at(0x2000) = 0x7002;
	*self.machine->ram_at(0x2002) = 0;
	self.machine->run_for_instructions(1);

	const auto state = self.machine->get_processor_state();
	XCTAssertEqual(state.registers.address[2], 0x70022000);
	XCTAssertEqual(*self.machine->ram_at(0x2000), 0x7002);
	XCTAssertEqual(*self.machine->ram_at(0x2002), 0x0000);
	XCTAssertEqual(state.registers.status & ConditionCode::AllConditions, ConditionCode::AllConditions);
}

@end

// MARK: - ADDX

@interface M68000ADDXTests : M68000ArithmeticTests
@end
@implementation M68000ADDXTests

- (void)testADDXl_Dn {
	self.machine->set_program({
		0xd581		// ADDX.l D1, D2
	});
	self.machine->set_registers([=](auto &registers) {
		registers.data[1] = 0x12345678;
		registers.data[2] = 0x12345678;
		registers.status |= ConditionCode::AllConditions;
	});
	self.machine->run_for_instructions(1);

	const auto state = self.machine->get_processor_state();
	XCTAssertEqual(state.registers.data[1], 0x12345678);
	XCTAssertEqual(state.registers.data[2], 0x2468acf1);
	XCTAssertEqual(state.registers.status & ConditionCode::AllConditions, 0);
	XCTAssertEqual(8, self.machine->get_cycle_count());
}

- (void)testADDXb {
	self.machine->set_program({
		0xd501		// ADDX.b D1, D2
	});
	self.machine->set_registers([=](auto &registers) {
		registers.data[1] = 0x80;
		registers.data[2] = 0x8080;
	});
	self.machine->run_for_instructions(1);

	const auto state = self.machine->get_processor_state();
	XCTAssertEqual(state.registers.data[1], 0x80);
	XCTAssertEqual(state.registers.data[2], 0x8000);
	XCTAssertEqual(state.registers.status & ConditionCode::AllConditions, ConditionCode::Carry | ConditionCode::Overflow | ConditionCode::Extend);
	XCTAssertEqual(4, self.machine->get_cycle_count());
}

- (void)testADDXw {
	self.machine->set_program({
		0xd541		// ADDX.w D1, D2
	});
	self.machine->set_registers([=](auto &registers) {
		registers.data[1] = 0x1ffff;
		registers.data[2] = 0x18080;
		registers.status |= ConditionCode::AllConditions;
	});
	self.machine->run_for_instructions(1);

	const auto state = self.machine->get_processor_state();
	XCTAssertEqual(state.registers.data[1], 0x1ffff);
	XCTAssertEqual(state.registers.data[2], 0x18080);
	XCTAssertEqual(state.registers.status & ConditionCode::AllConditions, ConditionCode::Carry | ConditionCode::Negative | ConditionCode::Extend);
	XCTAssertEqual(4, self.machine->get_cycle_count());
}

- (void)testADDXl_PreDec {
	self.machine->set_program({
		0xd389		// ADDX.l -(A1), -(A1)
	});
	self.machine->set_registers([=](auto &registers) {
		registers.address[1] = 0x3000;
		registers.status |= ConditionCode::AllConditions;
	});
	*self.machine->ram_at(0x2ff8) = 0x1000;
	*self.machine->ram_at(0x2ffa) = 0x0000;
	*self.machine->ram_at(0x2ffc) = 0x7000;
	*self.machine->ram_at(0x2ffe) = 0x1ff1;
	self.machine->run_for_instructions(1);

	const auto state = self.machine->get_processor_state();
	XCTAssertEqual(state.registers.address[1], 0x2ff8);
	XCTAssertEqual(state.registers.status & ConditionCode::AllConditions, ConditionCode::Negative | ConditionCode::Overflow);
	XCTAssertEqual(*self.machine->ram_at(0x2ff8), 0x8000);
	XCTAssertEqual(*self.machine->ram_at(0x2ffa), 0x1ff2);
	XCTAssertEqual(*self.machine->ram_at(0x2ffc), 0x7000);
	XCTAssertEqual(*self.machine->ram_at(0x2ffe), 0x1ff1);
	XCTAssertEqual(30, self.machine->get_cycle_count());
}

@end

// MARK: - ADDI

@interface M68000ADDITests : M68000ArithmeticTests
@end
@implementation M68000ADDITests

- (void)testADDIl {
	self.machine->set_program({
		0x0681, 0x1111, 0x1111		// ADDI.l #$11111111, D1
	});
	self.machine->set_registers([=](auto &registers) {
		registers.data[1] = 0x300021b3;
	});
	self.machine->run_for_instructions(1);

	const auto state = self.machine->get_processor_state();
	XCTAssertEqual(state.registers.data[1], 0x411132C4);
	XCTAssertEqual(state.registers.status & ConditionCode::AllConditions, 0);
	XCTAssertEqual(16, self.machine->get_cycle_count());
}

- (void)testADDIw {
	self.machine->set_program({
		0x0678, 0x7aaa, 0x3000		// ADDI.W #$7aaa, ($3000).W
	});
	*self.machine->ram_at(0x3000) = 0xaaaa;

	self.machine->run_for_instructions(1);

	const auto state = self.machine->get_processor_state();
	XCTAssertEqual(*self.machine->ram_at(0x3000), 0x2554);
	XCTAssertEqual(state.registers.status & ConditionCode::AllConditions, ConditionCode::Extend | ConditionCode::Carry);
	XCTAssertEqual(20, self.machine->get_cycle_count());
}

@end

// MARK: - ADDQ

@interface M68000ADDQTests : M68000ArithmeticTests
@end
@implementation M68000ADDQTests

- (void)testADDQl {
	self.machine->set_program({
		0x5e81		// ADDQ.l #$7, D1
	});
	self.machine->set_registers([=](auto &registers) {
		registers.data[1] = 0xffffffff;
	});
	self.machine->run_for_instructions(1);

	const auto state = self.machine->get_processor_state();
	XCTAssertEqual(state.registers.data[1], 0x6);
	XCTAssertEqual(state.registers.status & ConditionCode::AllConditions, ConditionCode::Extend | ConditionCode::Carry);
	XCTAssertEqual(8, self.machine->get_cycle_count());
}

- (void)testADDQw_Dn {
	self.machine->set_program({
		0x5641		// ADDQ.W #$3, D1
	});
	self.machine->set_registers([=](auto &registers) {
		registers.data[1] = 0xfffffffe;
	});
	self.machine->run_for_instructions(1);

	const auto state = self.machine->get_processor_state();
	XCTAssertEqual(state.registers.data[1], 0xffff0001);
	XCTAssertEqual(state.registers.status & ConditionCode::AllConditions, ConditionCode::Extend | ConditionCode::Carry);
	XCTAssertEqual(4, self.machine->get_cycle_count());
}

- (void)testADDQw_An {
	self.machine->set_program({
		0x5649		// ADDQ.W #$3, A1
	});
	self.machine->set_registers([=](auto &registers) {
		registers.address[1] = 0xfffffffe;
	});
	self.machine->run_for_instructions(1);

	const auto state = self.machine->get_processor_state();
	XCTAssertEqual(state.registers.address[1], 1);
	XCTAssertEqual(state.registers.status & ConditionCode::AllConditions, 0);
	XCTAssertEqual(8, self.machine->get_cycle_count());
}

- (void)testADDQw_XXXw {
	self.machine->set_program({
		0x5078, 0x3000		// ADDQ.W #$8, ($3000).W
	});
	*self.machine->ram_at(0x3000) = 0x7ffd;

	self.machine->run_for_instructions(1);

	const auto state = self.machine->get_processor_state();
	XCTAssertEqual(*self.machine->ram_at(0x3000), 0x8005);
	XCTAssertEqual(state.registers.status & ConditionCode::AllConditions, ConditionCode::Negative | ConditionCode::Overflow);
	XCTAssertEqual(16, self.machine->get_cycle_count());
}

@end

// MARK: - CMP

@interface M68000CMPTests : M68000ArithmeticTests
@end
@implementation M68000CMPTests

- (void)performCMPb:(uint16_t)opcode expectedFlags:(uint16_t)flags {
	self.machine->set_program({
		opcode
	});
	self.machine->set_registers([=](auto &registers) {
		registers.data[1] = 0x1234567f;
		registers.data[2] = 0x12345680;
	});
	self.machine->run_for_instructions(1);

	const auto state = self.machine->get_processor_state();
	XCTAssertEqual(state.registers.data[1], 0x1234567f);
	XCTAssertEqual(state.registers.data[2], 0x12345680);
	XCTAssertEqual(state.registers.status & ConditionCode::AllConditions, flags);
	XCTAssertEqual(4, self.machine->get_cycle_count());
}

- (void)testCMPb_D1D2 {
	[self performCMPb:0xb401 expectedFlags:ConditionCode::Overflow];	// CMP.b D1, D2
}

- (void)testCMPb_D2D1 {
	[self performCMPb:0xb202 expectedFlags:ConditionCode::Overflow | ConditionCode::Negative | ConditionCode::Carry];	// CMP.b D2, D1
}

- (void)performCMPwd1:(uint32_t)d1 d2:(uint32_t)d2 expectedFlags:(uint16_t)flags {
	self.machine->set_program({
		0xb242		// CMP.W D2, D1
	});
	self.machine->set_registers([=](auto &registers) {
		registers.data[1] = d1;
		registers.data[2] = d2;
	});
	self.machine->run_for_instructions(1);

	const auto state = self.machine->get_processor_state();
	XCTAssertEqual(state.registers.data[1], d1);
	XCTAssertEqual(state.registers.data[2], d2);
	XCTAssertEqual(state.registers.status & ConditionCode::AllConditions, flags);
	XCTAssertEqual(4, self.machine->get_cycle_count());
}

- (void)testCMPw_8004v7002 {
	[self performCMPwd1:0x12347002 d2:0x12348004 expectedFlags:ConditionCode::Overflow | ConditionCode::Negative | ConditionCode::Carry];
}

- (void)testCMPw_6666v5555 {
	[self performCMPwd1:0x55555555 d2:0x66666666 expectedFlags:ConditionCode::Negative | ConditionCode::Carry];
}

- (void)testCMPl {
	self.machine->set_program({
		0xb282		// CMP.l D2, D1
	});
	self.machine->set_registers([=](auto &registers) {
		registers.data[1] = 0x12347002;
		registers.data[2] = 0x12348004;
	});
	self.machine->run_for_instructions(1);

	const auto state = self.machine->get_processor_state();
	XCTAssertEqual(state.registers.data[1], 0x12347002);
	XCTAssertEqual(state.registers.data[2], 0x12348004);
	XCTAssertEqual(state.registers.status & ConditionCode::AllConditions, ConditionCode::Negative | ConditionCode::Carry);
	XCTAssertEqual(6, self.machine->get_cycle_count());
}

@end

// MARK: - CMPA

@interface M68000CMPATests : M68000ArithmeticTests
@end
@implementation M68000CMPATests

- (void)performCMPAld1:(uint32_t)d1 a2:(uint32_t)a2 {
	self.machine->set_program({
		0xb5c1		// CMPA.l D1, A2
	});
	self.machine->set_registers([=](auto &registers) {
		registers.data[1] = d1;
		registers.address[2] = a2;
	});
	self.machine->run_for_instructions(1);

	const auto state = self.machine->get_processor_state();
	XCTAssertEqual(state.registers.data[1], d1);
	XCTAssertEqual(state.registers.address[2], a2);
	XCTAssertEqual(6, self.machine->get_cycle_count());
}

- (void)testCMPAl_noFlags {
	[self performCMPAld1:0x1234567f a2:0x12345680];

	const auto state = self.machine->get_processor_state();
	XCTAssertEqual(state.registers.status & ConditionCode::AllConditions, 0);
}

- (void)testCMPAl_carry {
	[self performCMPAld1:0xfd234577 a2:0x12345680];

	const auto state = self.machine->get_processor_state();
	XCTAssertEqual(state.registers.status & ConditionCode::AllConditions, ConditionCode::Carry);
}

- (void)testCMPAl_carryOverflowNegative {
	[self performCMPAld1:0x85678943 a2:0x22563245];

	const auto state = self.machine->get_processor_state();
	XCTAssertEqual(state.registers.status & ConditionCode::AllConditions, ConditionCode::Carry | ConditionCode::Overflow | ConditionCode::Negative);
}

- (void)performCMPAwd1:(uint32_t)d1 a2:(uint32_t)a2 {
	self.machine->set_program({
		0xb4c1		// CMPA.w D1, A2
	});
	self.machine->set_registers([=](auto &registers) {
		registers.data[1] = d1;
		registers.address[2] = a2;
	});
	self.machine->run_for_instructions(1);

	const auto state = self.machine->get_processor_state();
	XCTAssertEqual(state.registers.data[1], d1);
	XCTAssertEqual(state.registers.address[2], a2);
	XCTAssertEqual(6, self.machine->get_cycle_count());
}

- (void)testCMPAw_carry {
	[self performCMPAwd1:0x85678943 a2:0x22563245];

	const auto state = self.machine->get_processor_state();
	XCTAssertEqual(state.registers.status & ConditionCode::AllConditions, ConditionCode::Carry);
}

- (void)testCMPAw_zero {
	[self performCMPAwd1:0x0000ffff a2:0xffffffff];

	const auto state = self.machine->get_processor_state();
	XCTAssertEqual(state.registers.status & ConditionCode::AllConditions, ConditionCode::Zero);
}

@end

// MARK: CMPI

@interface M68000CMPITests : M68000ArithmeticTests
@end
@implementation M68000CMPITests

- (void)testCMPIw {
	self.machine->set_program({
		0x0c41, 0xffff		// CMPI.W #$ffff, D1
	});
	self.machine->set_registers([=](auto &registers) {
		registers.data[1] = 0xfff2ffff;
	});
	self.machine->run_for_instructions(1);

	const auto state = self.machine->get_processor_state();
	XCTAssertEqual(state.registers.data[1], 0xfff2ffff);
	XCTAssertEqual(state.registers.status & ConditionCode::AllConditions, ConditionCode::Zero);
	XCTAssertEqual(8, self.machine->get_cycle_count());
}

- (void)testCMPIl {
	self.machine->set_program({
		0x0c81, 0x8000, 0x0000		// CMPI.L #$80000000, D1
	});
	self.machine->set_registers([=](auto &registers) {
		registers.data[1] = 0x7fffffff;
	});
	self.machine->run_for_instructions(1);

	const auto state = self.machine->get_processor_state();
	XCTAssertEqual(state.registers.data[1], 0x7fffffff);
	XCTAssertEqual(state.registers.status & ConditionCode::AllConditions, ConditionCode::Negative | ConditionCode::Overflow | ConditionCode::Carry);
	XCTAssertEqual(14, self.machine->get_cycle_count());
}

- (void)testCMPIb {
	self.machine->set_program({
		0x0c01, 0x0090		// CMPI.B #$90, D1
	});
	self.machine->set_registers([=](auto &registers) {
		registers.data[1] = 0x8f;
	});
	self.machine->run_for_instructions(1);

	const auto state = self.machine->get_processor_state();
	XCTAssertEqual(state.registers.data[1], 0x8f);
	XCTAssertEqual(state.registers.status & ConditionCode::AllConditions, ConditionCode::Negative | ConditionCode::Carry);
	XCTAssertEqual(8, self.machine->get_cycle_count());
}

@end

// MARK: - CMPM

@interface M68000CMPMTests : M68000ArithmeticTests
@end
@implementation M68000CMPMTests

- (void)testCMPMl {
	self.machine->set_program({
		0xb389		// CMPM.L (A1)+, (A1)+
	});
	self.machine->set_registers([=](auto &registers) {
		registers.address[1] = 0x3000;
		registers.status |= ConditionCode::AllConditions;
	});
	*self.machine->ram_at(0x3000) = 0x7000;
	*self.machine->ram_at(0x3002) = 0x1ff1;
	*self.machine->ram_at(0x3004) = 0x1000;
	*self.machine->ram_at(0x3006) = 0x0000;
	self.machine->run_for_instructions(1);

	const auto state = self.machine->get_processor_state();
	XCTAssertEqual(state.registers.address[1], 0x3008);
	XCTAssertEqual(state.registers.status & ConditionCode::AllConditions, ConditionCode::Negative | ConditionCode::Extend | ConditionCode::Carry);
	XCTAssertEqual(20, self.machine->get_cycle_count());
}

- (void)testCMPMw {
	self.machine->set_program({
		0xb549		// CMPM.w (A1)+, (A2)+
	});
	self.machine->set_registers([=](auto &registers) {
		registers.address[1] = 0x3000;
		registers.address[2] = 0x3002;
		registers.status |= ConditionCode::AllConditions;
	});
	*self.machine->ram_at(0x3000) = 0;
	*self.machine->ram_at(0x3002) = 0;
	self.machine->run_for_instructions(1);

	const auto state = self.machine->get_processor_state();
	XCTAssertEqual(state.registers.address[1], 0x3002);
	XCTAssertEqual(state.registers.address[2], 0x3004);
	XCTAssertEqual(state.registers.status & ConditionCode::AllConditions, ConditionCode::Zero | ConditionCode::Extend);
	XCTAssertEqual(12, self.machine->get_cycle_count());
}

- (void)testCMPMb {
	self.machine->set_program({
		0xb509		// CMPM.b (A1)+, (A2)+
	});
	self.machine->set_registers([=](auto &registers) {
		registers.address[1] = 0x3000;
		registers.address[2] = 0x3001;
		registers.status |= ConditionCode::AllConditions;
	});
	*self.machine->ram_at(0x3000) = 0x807f;
	self.machine->run_for_instructions(1);

	const auto state = self.machine->get_processor_state();
	XCTAssertEqual(state.registers.address[1], 0x3001);
	XCTAssertEqual(state.registers.address[2], 0x3002);
	XCTAssertEqual(state.registers.status & ConditionCode::AllConditions, ConditionCode::Negative | ConditionCode::Extend | ConditionCode::Carry | ConditionCode::Overflow);
	XCTAssertEqual(12, self.machine->get_cycle_count());
}

@end

// MARK: - DIVS

@interface M68000DIVSTests : M68000ArithmeticTests
@end
@implementation M68000DIVSTests

- (void)performDIVS:(uint16_t)divisor d1:(uint32_t)d1 sp:(uint32_t)sp {
	self.machine->set_program({
		0x83fc, divisor		// DIVS #divisor, D1
	}, sp);
	self.machine->set_registers([=](auto &registers) {
		registers.data[1] = d1;
		registers.status |= ConditionCode::AllConditions;
	});
	self.machine->run_for_instructions(1);
}

- (void)performDIVS:(uint16_t)divisor d1:(uint32_t)d1 {
	[self performDIVS:divisor d1:d1 sp:0];
}

- (void)performDIVSOverflowTestDivisor:(uint16_t)divisor {
	[self performDIVS:divisor d1:0x4768f231];

	const auto state = self.machine->get_processor_state();
	XCTAssertEqual(state.registers.data[1], 0x4768f231);

	// N and Z are officially undefined upon DIVS overflow, and I've found no
	// other relevant information.
	XCTAssertEqual(
		state.registers.status & (ConditionCode::Extend | ConditionCode::Overflow | ConditionCode::Carry),
		ConditionCode::Extend | ConditionCode::Overflow);
	XCTAssertEqual(20, self.machine->get_cycle_count());
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

	const auto state = self.machine->get_processor_state();
	XCTAssertEqual(state.registers.data[1], 0x026190D3);
	XCTAssertEqual(state.registers.status & ConditionCode::AllConditions, ConditionCode::Extend | ConditionCode::Negative);
	XCTAssertEqual(138, self.machine->get_cycle_count());
}

- (void)testDIVS_3 {
	[self performDIVS:0xffff d1:0xffffffff];

	const auto state = self.machine->get_processor_state();
	XCTAssertEqual(state.registers.data[1], 1);
	XCTAssertEqual(state.registers.status & ConditionCode::AllConditions, ConditionCode::Extend);
	XCTAssertEqual(158, self.machine->get_cycle_count());
}

- (void)testDIVS_4 {
	[self performDIVS:0x3333 d1:0xffff0000];

	const auto state = self.machine->get_processor_state();
	XCTAssertEqual(state.registers.data[1], 0xfffffffb);
	XCTAssertEqual(state.registers.status & ConditionCode::AllConditions, ConditionCode::Extend | ConditionCode::Negative);
	XCTAssertEqual(158, self.machine->get_cycle_count());
}

- (void)testDIVS_5 {
	[self performDIVS:0x23 d1:0x8765];

	const auto state = self.machine->get_processor_state();
	XCTAssertEqual(state.registers.data[1], 0xb03de);
	XCTAssertEqual(state.registers.status & ConditionCode::AllConditions, ConditionCode::Extend);
	XCTAssertEqual(138, self.machine->get_cycle_count());
}

- (void)testDIVS_6 {
	[self performDIVS:0x8765 d1:0x65];

	const auto state = self.machine->get_processor_state();
	XCTAssertEqual(state.registers.data[1], 0x650000);
	XCTAssertEqual(state.registers.status & ConditionCode::AllConditions, ConditionCode::Extend | ConditionCode::Zero);
	XCTAssertEqual(156, self.machine->get_cycle_count());
}

- (void)testDIVSExpensiveOverflow {
	// DIVS.W #$ffff, D1 alt
	[self performDIVS:0xffff d1:0x80000000];

	const auto state = self.machine->get_processor_state();
	XCTAssertEqual(state.registers.data[1], 0x80000000);
	XCTAssertEqual(state.registers.status & ConditionCode::AllConditions, ConditionCode::Extend | ConditionCode::Negative | ConditionCode::Overflow);
	XCTAssertEqual(158, self.machine->get_cycle_count());
}

- (void)testDIVS_8 {
	// DIVS.W #$fffd, D1
	[self performDIVS:0xfffd d1:0xfffffffd];

	const auto state = self.machine->get_processor_state();
	XCTAssertEqual(state.registers.data[1], 0x1);
	XCTAssertEqual(state.registers.status & ConditionCode::AllConditions, ConditionCode::Extend);
	XCTAssertEqual(158, self.machine->get_cycle_count());
}

- (void)testDIVS_9 {
	// DIVS.W #$7aee, D1
	[self performDIVS:0x7aee d1:0xdaaa00fe];

	const auto state = self.machine->get_processor_state();
	XCTAssertEqual(state.registers.data[1], 0xc97eb240);
	XCTAssertEqual(state.registers.status & ConditionCode::AllConditions, ConditionCode::Extend | ConditionCode::Negative);
	XCTAssertEqual(148, self.machine->get_cycle_count());
}

- (void)testDIVS_10 {
	// DIVS.W #$7fff, D1
	[self performDIVS:0x7fff d1:0x82f9fff];

	const auto state = self.machine->get_processor_state();
	XCTAssertEqual(state.registers.data[1], 0x305e105f);
	XCTAssertEqual(state.registers.status & ConditionCode::AllConditions, ConditionCode::Extend);
	XCTAssertEqual(142, self.machine->get_cycle_count());
}

- (void)testDIVS_11 {
	// DIVS.W #$f32, D1
	[self performDIVS:0xf32 d1:0x00e1d44];

	const auto state = self.machine->get_processor_state();
	XCTAssertEqual(state.registers.data[1], 0x0bfa00ed);
	XCTAssertEqual(state.registers.status & ConditionCode::AllConditions, ConditionCode::Extend);
	XCTAssertEqual(144, self.machine->get_cycle_count());
}

- (void)testDIVS_12 {
	// DIVS.W #$af32, D1
	[self performDIVS:0xaf32 d1:0xe1d44 sp:0];

	const auto state = self.machine->get_processor_state();
	XCTAssertEqual(state.registers.data[1], 0x39dcffd4);
	XCTAssertEqual(state.registers.status & ConditionCode::AllConditions, ConditionCode::Extend | ConditionCode::Negative);
	XCTAssertEqual(150, self.machine->get_cycle_count());
}

- (void)testDIVSException {
	// DIVS.W #0, D1
	const uint32_t initial_sp = 0x5000;
	[self performDIVS:0x0 d1:0x1fffffff sp:initial_sp];

	// Check register state.registers.
	const auto state = self.machine->get_processor_state();
	XCTAssertEqual(state.registers.data[1], 0x1fffffff);
	XCTAssertEqual(state.registers.supervisor_stack_pointer, initial_sp - 6);
	XCTAssertEqual(state.registers.status & ConditionCode::AllConditions, ConditionCode::Extend);

	// Total expected bus pattern:
	//
	// np | nn nn | nw nw nw np np np n np = 42 cycles
	//
	// Noted: Yacht shows a total of three nps for a DIVS #;
	// I believe this is incorrect as it includes two in the
	// '1st op (ea)' stage, but an immediate word causes
	// only one elsewhere.
	XCTAssertEqual(42, self.machine->get_cycle_count());

	// Check stack contents; should be PC.l, PC.h and status register.
	// Assumed: the program counter on the stack is that of the
	// failing instrustion.
	const uint16_t pc_h = *self.machine->ram_at(initial_sp-4);
	const uint16_t pc_l = *self.machine->ram_at(initial_sp-2);
//	const uint16_t status = *self.machine->ram_at(initial_sp);
	const uint32_t initial_pc = self.machine->initial_pc();
	XCTAssertEqual(pc_l, initial_pc & 0xffff);
	XCTAssertEqual(pc_h, initial_pc >> 16);
//	XCTAssertEqual(status, 9);
}

@end

// MARK: - DIVU

@interface M68000DIVUTests : M68000ArithmeticTests
@end
@implementation M68000DIVUTests

- (void)performDIVU:(uint16_t)divisor d1:(uint32_t)d1 {
	self.machine->set_program({
		0x82fc, divisor		// DIVU #$eef0, D1
	});
	self.machine->set_registers([=](auto &registers) {
		registers.data[1] = d1;
		registers.status |= ConditionCode::AllConditions;
	});
	self.machine->run_for_instructions(1);
}

- (void)testDIVU_0001 {
	[self performDIVU:1 d1:0x4768f231];

	const auto state = self.machine->get_processor_state();
	XCTAssertEqual(state.registers.data[1], 0x4768f231);
	XCTAssertEqual(state.registers.status & ConditionCode::AllConditions, ConditionCode::Extend | ConditionCode::Negative | ConditionCode::Overflow);
	XCTAssertEqual(14, self.machine->get_cycle_count());
}

- (void)testDIVU_1234 {
	[self performDIVU:0x1234 d1:0x4768f231];

	const auto state = self.machine->get_processor_state();
	XCTAssertEqual(state.registers.data[1], 0x4768f231);
	XCTAssertEqual(state.registers.status & ConditionCode::AllConditions, ConditionCode::Extend | ConditionCode::Negative | ConditionCode::Overflow);
	XCTAssertEqual(14, self.machine->get_cycle_count());
}

- (void)testDIVU_eeff {
	[self performDIVU:0xeeff d1:0x4768f231];

	const auto state = self.machine->get_processor_state();
	XCTAssertEqual(state.registers.data[1], 0x8bae4c7d);
	XCTAssertEqual(state.registers.status & ConditionCode::AllConditions, ConditionCode::Extend);
	XCTAssertEqual(108, self.machine->get_cycle_count());
}

- (void)testDIVU_3333 {
	[self performDIVU:0x3333 d1:0x1fffffff];

	const auto state = self.machine->get_processor_state();
	XCTAssertEqual(state.registers.data[1], 0x1fffa000);
	XCTAssertEqual(state.registers.status & ConditionCode::AllConditions, ConditionCode::Extend | ConditionCode::Negative);
	XCTAssertEqual(136, self.machine->get_cycle_count());
}

// Omitted: divide by zero test.

@end

// MARK: - EXT

@interface M68000EXTTests : M68000ArithmeticTests
@end
@implementation M68000EXTTests

- (void)performEXTwd3:(uint32_t)d3 {
	self.machine->set_program({
		0x4883		// EXT.W D3
	});

	self.machine->set_registers([=](auto &registers) {
		registers.data[3] = d3;
		registers.status = 0x13;
	});
	self.machine->run_for_instructions(1);

	XCTAssertEqual(4, self.machine->get_cycle_count());
}

- (void)testEXTw_78 {
	[self performEXTwd3:0x12345678];

	const auto state = self.machine->get_processor_state();
	XCTAssertEqual(state.registers.data[3], 0x12340078);
	XCTAssertEqual(state.registers.status & ConditionCode::AllConditions, ConditionCode::Extend);
}

- (void)testEXTw_00 {
	[self performEXTwd3:0x12345600];

	const auto state = self.machine->get_processor_state();
	XCTAssertEqual(state.registers.data[3], 0x12340000);
	XCTAssertEqual(state.registers.status & ConditionCode::AllConditions, ConditionCode::Extend | ConditionCode::Zero);
}

- (void)testEXTw_f0 {
	[self performEXTwd3:0x123456f0];

	const auto state = self.machine->get_processor_state();
	XCTAssertEqual(state.registers.data[3], 0x1234fff0);
	XCTAssertEqual(state.registers.status & ConditionCode::AllConditions, ConditionCode::Extend | ConditionCode::Negative);
}

- (void)testEXTl {
	self.machine->set_program({
		0x48c3		// EXT.L D3
	});

	self.machine->set_registers([=](auto &registers) {
		registers.data[3] = 0x1234f6f0;
		registers.status = 0x13;
	});
	self.machine->run_for_instructions(1);

	XCTAssertEqual(4, self.machine->get_cycle_count());

	const auto state = self.machine->get_processor_state();
	XCTAssertEqual(state.registers.data[3], 0xfffff6f0);
	XCTAssertEqual(state.registers.status & ConditionCode::AllConditions, ConditionCode::Extend | ConditionCode::Negative);
}

@end

// MARK: - MULS

@interface M68000MULSTests : M68000ArithmeticTests
@end
@implementation M68000MULSTests

- (void)performMULSd1:(uint32_t)d1 d2:(uint32_t)d2 ccr:(uint8_t)ccr {
	self.machine->set_program({
		0xc5c1		// MULS D1, D2
	});
	self.machine->set_registers([=](auto &registers) {
		registers.data[1] = d1;
		registers.data[2] = d2;
		registers.status = ccr;
	});
	self.machine->run_for_instructions(1);
}

- (void)performMULSConstant:(uint16_t)constant d2:(uint32_t)d2 {
	self.machine->set_program({
		0xc5fc, constant		// MULS #constant, D2
	});
	self.machine->set_registers([=](auto &registers) {
		registers.data[2] = d2;
		registers.status = ConditionCode::Carry | ConditionCode::Extend | ConditionCode::Overflow;
	});
	self.machine->run_for_instructions(1);
}

- (void)testMULS {
	[self performMULSd1:0x12345678 d2:0x12345678 ccr:0];

	const auto state = self.machine->get_processor_state();
	XCTAssertEqual(state.registers.data[1], 0x12345678);
	XCTAssertEqual(state.registers.data[2], 0x1d34d840);
	XCTAssertEqual(state.registers.status & ConditionCode::AllConditions, 0);
	XCTAssertEqual(54, self.machine->get_cycle_count());
}

- (void)testMULS_2 {
	[self performMULSd1:0x82348678 d2:0x823486ff ccr:0];

	const auto state = self.machine->get_processor_state();
	XCTAssertEqual(state.registers.data[1], 0x82348678);
	XCTAssertEqual(state.registers.data[2], 0x3971c188);
	XCTAssertEqual(state.registers.status & ConditionCode::AllConditions, 0);
	XCTAssertEqual(48, self.machine->get_cycle_count());
}

- (void)testMULSZero {
	[self performMULSd1:1 d2:0 ccr:ConditionCode::Carry | ConditionCode::Overflow | ConditionCode::Extend];

	const auto state = self.machine->get_processor_state();
	XCTAssertEqual(state.registers.data[1], 1);
	XCTAssertEqual(state.registers.data[2], 0);
	XCTAssertEqual(state.registers.status & ConditionCode::AllConditions, ConditionCode::Extend | ConditionCode::Zero);
	XCTAssertEqual(42, self.machine->get_cycle_count());
}

- (void)testMULSFFFF {
	[self performMULSConstant:0xffff d2:0xffff];

	const auto state = self.machine->get_processor_state();
	XCTAssertEqual(state.registers.data[2], 1);
	XCTAssertEqual(state.registers.status & ConditionCode::AllConditions, ConditionCode::Extend);
	XCTAssertEqual(44, self.machine->get_cycle_count());
}

- (void)testMULSNegative {
	[self performMULSConstant:0x1fff d2:0x8fff];

	const auto state = self.machine->get_processor_state();
	XCTAssertEqual(state.registers.data[2], 0xf2005001);
	XCTAssertEqual(state.registers.status & ConditionCode::AllConditions, ConditionCode::Extend | ConditionCode::Negative);
	XCTAssertEqual(46, self.machine->get_cycle_count());
}

@end

// MARK: - MULU

@interface M68000MULUTests : M68000ArithmeticTests
@end
@implementation M68000MULUTests

- (void)performMULUd1:(uint32_t)d1 d2:(uint32_t)d2 ccr:(uint8_t)ccr {
	self.machine->set_program({
		0xc4c1		// MULU D1, D2
	});
	self.machine->set_registers([=](auto &registers) {
		registers.data[1] = d1;
		registers.data[2] = d2;
		registers.status |= ccr;
	});
	self.machine->run_for_instructions(1);
}

- (void)testMULU_Dn {
	[self performMULUd1:0x12345678 d2:0x12345678 ccr:0];

	const auto state = self.machine->get_processor_state();
	XCTAssertEqual(state.registers.data[2], 0x1d34d840);
	XCTAssertEqual(state.registers.status & ConditionCode::AllConditions, 0);
	XCTAssertEqual(54, self.machine->get_cycle_count());
}

- (void)testMULU_Dn_Zero {
	[self performMULUd1:1 d2:0 ccr:ConditionCode::Extend | ConditionCode::Overflow | ConditionCode::Carry];

	const auto state = self.machine->get_processor_state();
	XCTAssertEqual(state.registers.data[1], 1);
	XCTAssertEqual(state.registers.data[2], 0);
	XCTAssertEqual(state.registers.status & ConditionCode::AllConditions, ConditionCode::Extend | ConditionCode::Zero);
	XCTAssertEqual(40, self.machine->get_cycle_count());
}

- (void)testMULU_Imm {
	self.machine->set_program({
		0xc4fc, 0xffff		// MULU.W	#$ffff, D2
	});
	self.machine->set_registers([=](auto &registers) {
		registers.data[2] = 0xffff;
		registers.status |= ConditionCode::Extend | ConditionCode::Overflow | ConditionCode::Carry;
	});
	self.machine->run_for_instructions(1);

	const auto state = self.machine->get_processor_state();
	XCTAssertEqual(state.registers.data[2], 0xfffe0001);
	XCTAssertEqual(state.registers.status & ConditionCode::AllConditions, ConditionCode::Extend | ConditionCode::Negative);
	XCTAssertEqual(74, self.machine->get_cycle_count());
}

@end

// MARK: - NEG

@interface M68000NEGTests : M68000ArithmeticTests
@end
@implementation M68000NEGTests

- (void)performNEGb:(uint32_t)value {
	self.machine->set_program({
		0x4400		// NEG.b D0
	});
	self.machine->set_registers([=](auto &registers) {
		registers.data[0] = value;
	});
	self.machine->run_for_instructions(1);

	XCTAssertEqual(4, self.machine->get_cycle_count());
}

- (void)testNEGb_78 {
	[self performNEGb:0x12345678];

	const auto state = self.machine->get_processor_state();
	XCTAssertEqual(state.registers.data[0], 0x12345688);
	XCTAssertEqual(state.registers.status & ConditionCode::AllConditions, ConditionCode::Carry | ConditionCode::Extend | ConditionCode::Negative);
}

- (void)testNEGb_00 {
	[self performNEGb:0x12345600];

	const auto state = self.machine->get_processor_state();
	XCTAssertEqual(state.registers.data[0], 0x12345600);
	XCTAssertEqual(state.registers.status & ConditionCode::AllConditions, ConditionCode::Zero);
}

- (void)testNEGb_80 {
	[self performNEGb:0x12345680];

	const auto state = self.machine->get_processor_state();
	XCTAssertEqual(state.registers.data[0], 0x12345680);
	XCTAssertEqual(state.registers.status & ConditionCode::AllConditions, ConditionCode::Negative | ConditionCode::Overflow | ConditionCode::Extend | ConditionCode::Carry);
}

- (void)testNEGw {
	self.machine->set_program({
		0x4440		// NEG.w D0
	});
	self.machine->set_registers([=](auto &registers) {
		registers.data[0] = 0x12348000;
	});
	self.machine->run_for_instructions(1);

	const auto state = self.machine->get_processor_state();
	XCTAssertEqual(4, self.machine->get_cycle_count());
	XCTAssertEqual(state.registers.data[0], 0x12348000);
	XCTAssertEqual(state.registers.status & ConditionCode::AllConditions, ConditionCode::Negative | ConditionCode::Overflow | ConditionCode::Extend | ConditionCode::Carry);
}

- (void)performNEGl:(uint32_t)value {
	self.machine->set_program({
		0x4480		// NEG.l D0
	});
	self.machine->set_registers([=](auto &registers) {
		registers.data[0] = value;
	});
	self.machine->run_for_instructions(1);

	XCTAssertEqual(6, self.machine->get_cycle_count());
}

- (void)testNEGl_large {
	[self performNEGl:0x12345678];

	const auto state = self.machine->get_processor_state();
	XCTAssertEqual(state.registers.data[0], 0xedcba988);
	XCTAssertEqual(state.registers.status & ConditionCode::AllConditions, ConditionCode::Carry | ConditionCode::Extend | ConditionCode::Negative);
}

- (void)testNEGl_small {
	[self performNEGl:0xffffffff];

	const auto state = self.machine->get_processor_state();
	XCTAssertEqual(state.registers.data[0], 0x1);
	XCTAssertEqual(state.registers.status & ConditionCode::AllConditions, ConditionCode::Carry | ConditionCode::Extend);
}

- (void)testNEGl_XXXl {
	self.machine->set_program({
		0x44b9, 0x0000, 0x3000		// NEG.L ($3000).L
	});
	*self.machine->ram_at(0x3000) = 0xf001;
	*self.machine->ram_at(0x3002) = 0x2311;

	self.machine->run_for_instructions(1);

	const auto state = self.machine->get_processor_state();
	XCTAssertEqual(28, self.machine->get_cycle_count());
	XCTAssertEqual(*self.machine->ram_at(0x3000), 0x0ffe);
	XCTAssertEqual(*self.machine->ram_at(0x3002), 0xdcef);
	XCTAssertEqual(state.registers.status & ConditionCode::AllConditions, ConditionCode::Extend | ConditionCode::Carry);
}

@end

// MARK: - NEGX

@interface M68000NEGXTests : M68000ArithmeticTests
@end
@implementation M68000NEGXTests

- (void)performNEGXb:(uint32_t)value {
	self.machine->set_program({
		0x4000		// NEGX.b D0
	});
	self.machine->set_registers([=](auto &registers) {
		registers.data[0] = value;
		registers.status |= ConditionCode::Extend;
	});
	self.machine->run_for_instructions(1);

	XCTAssertEqual(4, self.machine->get_cycle_count());
}

- (void)testNEGXb_78 {
	[self performNEGXb:0x12345678];

	const auto state = self.machine->get_processor_state();
	XCTAssertEqual(state.registers.data[0], 0x12345687);
	XCTAssertEqual(state.registers.status & ConditionCode::AllConditions, ConditionCode::Carry | ConditionCode::Extend | ConditionCode::Negative);
}

- (void)testNEGXb_00 {
	[self performNEGXb:0x12345600];

	const auto state = self.machine->get_processor_state();
	XCTAssertEqual(state.registers.data[0], 0x123456ff);
	XCTAssertEqual(state.registers.status & ConditionCode::AllConditions, ConditionCode::Carry | ConditionCode::Extend | ConditionCode::Negative);
}

- (void)testNEGXb_80 {
	[self performNEGXb:0x12345680];

	const auto state = self.machine->get_processor_state();
	XCTAssertEqual(state.registers.data[0], 0x1234567f);
	XCTAssertEqual(state.registers.status & ConditionCode::AllConditions, ConditionCode::Extend | ConditionCode::Carry);
}

- (void)testNEGXw {
	self.machine->set_program({
		0x4040		// NEGX.w D0
	});
	self.machine->set_registers([=](auto &registers) {
		registers.data[0] = 0x12348000;
		registers.status |= ConditionCode::Extend;
	});
	self.machine->run_for_instructions(1);

	const auto state = self.machine->get_processor_state();
	XCTAssertEqual(4, self.machine->get_cycle_count());
	XCTAssertEqual(state.registers.data[0], 0x12347fff);
	XCTAssertEqual(state.registers.status & ConditionCode::AllConditions, ConditionCode::Extend | ConditionCode::Carry);
}

- (void)performNEGXl:(uint32_t)value {
	self.machine->set_program({
		0x4080		// NEGX.l D0
	});
	self.machine->set_registers([=](auto &registers) {
		registers.data[0] = value;
		registers.status |= ConditionCode::Extend;
	});
	self.machine->run_for_instructions(1);

	XCTAssertEqual(6, self.machine->get_cycle_count());
}

- (void)testNEGXl_large {
	[self performNEGXl:0x12345678];

	const auto state = self.machine->get_processor_state();
	XCTAssertEqual(state.registers.data[0], 0xedcba987);
	XCTAssertEqual(state.registers.status & ConditionCode::AllConditions, ConditionCode::Carry | ConditionCode::Extend | ConditionCode::Negative);
}

- (void)testNEGXl_small {
	[self performNEGXl:0xffffffff];

	const auto state = self.machine->get_processor_state();
	XCTAssertEqual(state.registers.data[0], 0x0);
	XCTAssertEqual(state.registers.status & ConditionCode::AllConditions, ConditionCode::Carry | ConditionCode::Extend);
}

- (void)testNEGXl_XXXl {
	self.machine->set_program({
		0x40b9, 0x0000, 0x3000		// NEGX.L ($3000).L
	});
	self.machine->set_registers([=](auto &registers) {
		registers.status |= ConditionCode::Extend;
	});
	*self.machine->ram_at(0x3000) = 0xf001;
	*self.machine->ram_at(0x3002) = 0x2311;
	self.machine->run_for_instructions(1);

	const auto state = self.machine->get_processor_state();
	XCTAssertEqual(28, self.machine->get_cycle_count());
	XCTAssertEqual(*self.machine->ram_at(0x3000), 0x0ffe);
	XCTAssertEqual(*self.machine->ram_at(0x3002), 0xdcee);
	XCTAssertEqual(state.registers.status & ConditionCode::AllConditions, ConditionCode::Extend | ConditionCode::Carry);
}

@end

// MARK: - SUB

@interface M68000SUBTests : M68000ArithmeticTests
@end
@implementation M68000SUBTests

- (void)performSUBbIMM:(uint16_t)value d2:(uint32_t)d2 {
	self.machine->set_program({
		0x0402, value		// SUB.b #value, D2
	});
	self.machine->set_registers([=](auto &registers) {
		registers.data[2] = d2;
	});
	self.machine->run_for_instructions(1);

	XCTAssertEqual(8, self.machine->get_cycle_count());
}

- (void)testSUBb_IMM_ff {
	[self performSUBbIMM:0xff d2:0x9ae];

	const auto state = self.machine->get_processor_state();
	XCTAssertEqual(state.registers.data[2], 0x9af);
	XCTAssertEqual(state.registers.status & ConditionCode::AllConditions, ConditionCode::Carry | ConditionCode::Negative | ConditionCode::Extend);
}

- (void)testSUBb_IMM_82 {
	[self performSUBbIMM:0x82 d2:0x0a];

	const auto state = self.machine->get_processor_state();
	XCTAssertEqual(state.registers.data[2], 0x88);
	XCTAssertEqual(state.registers.status & ConditionCode::AllConditions, ConditionCode::Carry | ConditionCode::Negative | ConditionCode::Overflow | ConditionCode::Extend);
}

- (void)testSUBb_IMM_f0 {
	[self performSUBbIMM:0xf0 d2:0x64];

	const auto state = self.machine->get_processor_state();
	XCTAssertEqual(state.registers.data[2], 0x74);
	XCTAssertEqual(state.registers.status & ConditionCode::AllConditions, ConditionCode::Carry | ConditionCode::Extend);
}

- (void)testSUBb_IMM_28 {
	[self performSUBbIMM:0x28 d2:0xff96];

	const auto state = self.machine->get_processor_state();
	XCTAssertEqual(state.registers.data[2], 0xff6e);
	XCTAssertEqual(state.registers.status & ConditionCode::AllConditions, ConditionCode::Overflow);
}

- (void)testSUBb_PreDec {
	self.machine->set_program({
		0x9427		// SUB.b -(A7), D2
	}, 0x2002);
	self.machine->set_registers([=](auto &registers) {
		registers.data[2] = 0x9c40;
	});
	*self.machine->ram_at(0x2000) = 0x2710;
	self.machine->run_for_instructions(1);

	const auto state = self.machine->get_processor_state();
	XCTAssertEqual(10, self.machine->get_cycle_count());
	XCTAssertEqual(state.registers.data[2], 0x9c19);
	XCTAssertEqual(state.registers.stack_pointer(), 0x2000);
}

// Omitted: SUB.w -(A6), D2, which is designed to trigger an address error.

- (void)testSUBw_XXXw {
	self.machine->set_program({
		0x9578, 0x3000		// SUB.w D2, ($3000).w
	});
	self.machine->set_registers([=](auto &registers) {
		registers.data[2] = 0x2711;
	});
	*self.machine->ram_at(0x3000) = 0x759f;
	self.machine->run_for_instructions(1);

	const auto state = self.machine->get_processor_state();
	XCTAssertEqual(16, self.machine->get_cycle_count());
	XCTAssertEqual(state.registers.data[2], 0x2711);
	XCTAssertEqual(state.registers.status & ConditionCode::AllConditions, 0);
	XCTAssertEqual(*self.machine->ram_at(0x3000), 0x4e8e);
}

- (void)testSUBl_dAn {
	self.machine->set_program({
		0x95ab, 0x0004		// SUB.l D2, 4(A3)
	});
	self.machine->set_registers([=](auto &registers) {
		registers.data[2] = 0x45fd5ab4;
		registers.address[3] = 0x3000;
	});
	*self.machine->ram_at(0x3004) = 0x327a;
	*self.machine->ram_at(0x3006) = 0x4ef3;
	self.machine->run_for_instructions(1);

	const auto state = self.machine->get_processor_state();
	XCTAssertEqual(24, self.machine->get_cycle_count());
	XCTAssertEqual(state.registers.data[2], 0x45fd5ab4);
	XCTAssertEqual(state.registers.address[3], 0x3000);
	XCTAssertEqual(state.registers.status & ConditionCode::AllConditions, ConditionCode::Carry | ConditionCode::Extend | ConditionCode::Negative);
	XCTAssertEqual(*self.machine->ram_at(0x3004), 0xec7c);
	XCTAssertEqual(*self.machine->ram_at(0x3006), 0xf43f);
}

@end

// MARK: - SUBA

@interface M68000SUBATests : M68000ArithmeticTests
@end
@implementation M68000SUBATests

- (void)testSUBAl_Imm {
	self.machine->set_program({
		0x95fc, 0x1234, 0x5678		// SUBA.l #$12345678, A2
	});
	self.machine->set_registers([=](auto &registers) {
		registers.address[2] = 0xae43ab1d;
		registers.status |= ConditionCode::AllConditions;
	});
	self.machine->run_for_instructions(1);

	const auto state = self.machine->get_processor_state();
	XCTAssertEqual(16, self.machine->get_cycle_count());
	XCTAssertEqual(state.registers.address[2], 0x9c0f54a5);
	XCTAssertEqual(state.registers.status & ConditionCode::AllConditions, ConditionCode::AllConditions);
}

- (void)testSUBAw_ImmPositive {
	self.machine->set_program({
		0x94fc, 0x5678		// SUBA.w #$5678, A2
	});
	self.machine->set_registers([=](auto &registers) {
		registers.address[2] = 0xae43ab1d;
	});
	self.machine->run_for_instructions(1);

	const auto state = self.machine->get_processor_state();
	XCTAssertEqual(12, self.machine->get_cycle_count());
	XCTAssertEqual(state.registers.address[2], 0xae4354a5);
	XCTAssertEqual(state.registers.status & ConditionCode::AllConditions, 0);
}

- (void)testSUBAw_ImmNegative {
	self.machine->set_program({
		0x94fc, 0xf678		// SUBA.w #$5678, A2
	});
	self.machine->set_registers([=](auto &registers) {
		registers.address[2] = 0xae43ab1d;
	});
	self.machine->run_for_instructions(1);

	const auto state = self.machine->get_processor_state();
	XCTAssertEqual(12, self.machine->get_cycle_count());
	XCTAssertEqual(state.registers.address[2], 0xae43b4a5);
	XCTAssertEqual(state.registers.status & ConditionCode::AllConditions, 0);
}

- (void)testSUBAw_PreDec {
	self.machine->set_program({
		0x95e2		// SUBA.l -(A2), A2
	});
	self.machine->set_registers([=](auto &registers) {
		registers.address[2] = 0x2004;
	});
	*self.machine->ram_at(0x2000) = 0x7002;
	*self.machine->ram_at(0x2002) = 0x0000;
	self.machine->run_for_instructions(1);

	const auto state = self.machine->get_processor_state();
	XCTAssertEqual(16, self.machine->get_cycle_count());
	XCTAssertEqual(state.registers.address[2], 0x8ffe2000);
	XCTAssertEqual(state.registers.status & ConditionCode::AllConditions, 0);
}

@end

// MARK: - SUBI

@interface M68000SUBITests : M68000ArithmeticTests
@end
@implementation M68000SUBITests

- (void)testSUBIl {
	self.machine->set_program({
		0x0481, 0xf111, 0x1111		// SUBI.L #$f1111111, D1
	});
	self.machine->set_registers([=](auto &registers) {
		registers.data[1] = 0x300021b3;
	});
	self.machine->run_for_instructions(1);

	const auto state = self.machine->get_processor_state();
	XCTAssertEqual(16, self.machine->get_cycle_count());
	XCTAssertEqual(state.registers.data[1], 0x3eef10a2);
	XCTAssertEqual(state.registers.status & ConditionCode::AllConditions, ConditionCode::Carry | ConditionCode::Extend);
}

// Test of SUBI.W #$7aaa, ($3000).W omitted; that doesn't appear to be a valid instruction?

@end

// MARK: - SUBQ

@interface M68000SUBQTests : M68000ArithmeticTests
@end
@implementation M68000SUBQTests

- (void)testSUBQl {
	self.machine->set_program({
		0x5f81		// SUBQ.L #$7, D1
	});
	self.machine->set_registers([=](auto &registers) {
		registers.data[1] = 0xffffffff;
	});
	self.machine->run_for_instructions(1);

	const auto state = self.machine->get_processor_state();
	XCTAssertEqual(8, self.machine->get_cycle_count());
	XCTAssertEqual(state.registers.data[1], 0xfffffff8);
	XCTAssertEqual(state.registers.status & ConditionCode::AllConditions, ConditionCode::Negative);
}

- (void)testSUBQw {
	self.machine->set_program({
		0x5f49		// SUBQ.W #$7, A1
	});
	self.machine->set_registers([=](auto &registers) {
		registers.address[1] = 0xffff0001;
	});
	self.machine->run_for_instructions(1);

	const auto state = self.machine->get_processor_state();
	XCTAssertEqual(8, self.machine->get_cycle_count());
	XCTAssertEqual(state.registers.address[1], 0xfffefffa);
	XCTAssertEqual(state.registers.status & ConditionCode::AllConditions, 0);
}

- (void)testSUBQb {
	self.machine->set_program({
		0x5f38, 0x3000		// SUBQ.b #$7, ($3000).w
	});
	*self.machine->ram_at(0x3000) = 0x0600;

	self.machine->run_for_instructions(1);

	const auto state = self.machine->get_processor_state();
	XCTAssertEqual(16, self.machine->get_cycle_count());
	XCTAssertEqual(*self.machine->ram_at(0x3000), 0xff00);
	XCTAssertEqual(state.registers.status & ConditionCode::AllConditions, ConditionCode::Extend | ConditionCode::Negative | ConditionCode::Carry);
}

@end

// MARK: - SUBX

@interface M68000SUBXTests : M68000ArithmeticTests
@end
@implementation M68000SUBXTests

- (void)testSUBXl_Dn {
	self.machine->set_program({
		0x9581		// SUBX.l D1, D2
	});
	self.machine->set_registers([=](auto &registers) {
		registers.data[1] = 0x12345678;
		registers.data[2] = 0x12345678;
		registers.status |= ConditionCode::AllConditions;
	});
	self.machine->run_for_instructions(1);

	const auto state = self.machine->get_processor_state();
	XCTAssertEqual(8, self.machine->get_cycle_count());
	XCTAssertEqual(state.registers.data[1], 0x12345678);
	XCTAssertEqual(state.registers.data[2], 0xffffffff);
	XCTAssertEqual(state.registers.status & ConditionCode::AllConditions, ConditionCode::Extend | ConditionCode::Carry | ConditionCode::Negative);
}

- (void)testSUBXb_Dn {
	self.machine->set_program({
		0x9501		// SUBX.b D1, D2
	});
	self.machine->set_registers([=](auto &registers) {
		registers.data[1] = 0x80;
		registers.data[2] = 0x01;
	});
	self.machine->run_for_instructions(1);

	const auto state = self.machine->get_processor_state();
	XCTAssertEqual(4, self.machine->get_cycle_count());
	XCTAssertEqual(state.registers.data[1], 0x80);
	XCTAssertEqual(state.registers.data[2], 0x81);
	XCTAssertEqual(state.registers.status & ConditionCode::AllConditions, ConditionCode::Extend | ConditionCode::Carry | ConditionCode::Negative | ConditionCode::Overflow);
}

- (void)testSUBXl_PreDec {
	self.machine->set_program({
		0x9389		// SUBX.l -(A1), -(A1)
	});
	self.machine->set_registers([=](auto &registers) {
		registers.address[1] = 0x3000;
		registers.status |= ConditionCode::AllConditions;
	});
	*self.machine->ram_at(0x2ff8) = 0x1000;
	*self.machine->ram_at(0x2ffa) = 0x0000;
	*self.machine->ram_at(0x2ffc) = 0x7000;
	*self.machine->ram_at(0x2ffe) = 0x1ff1;
	self.machine->run_for_instructions(1);

	const auto state = self.machine->get_processor_state();
	XCTAssertEqual(30, self.machine->get_cycle_count());
	XCTAssertEqual(state.registers.address[1], 0x2ff8);
	XCTAssertEqual(state.registers.status & ConditionCode::AllConditions, ConditionCode::Extend | ConditionCode::Carry | ConditionCode::Negative );
	XCTAssertEqual(*self.machine->ram_at(0x2ff8), 0x9fff);
	XCTAssertEqual(*self.machine->ram_at(0x2ffa), 0xe00e);
	XCTAssertEqual(*self.machine->ram_at(0x2ffc), 0x7000);
	XCTAssertEqual(*self.machine->ram_at(0x2ffe), 0x1ff1);
}

@end
