//
//  Z80InterruptTests.swift
//  Clock Signal
//
//  Created by Thomas Harte on 03/06/2017.
//  Copyright © 2017 Thomas Harte. All rights reserved.
//

import XCTest

class Z80InterruptTests: XCTestCase {

	func testNMI() {
		let machine = CSTestMachineZ80()

		// start the PC at 0x0100 and install two NOPs for it
		machine.setValue(0x0100, for: .programCounter)
		machine.setValue(0, for: .IFF1)
		machine.setValue(1, for: .IFF2)
		machine.setValue(0x00, atAddress: 0x0100)
		machine.setValue(0x00, atAddress: 0x0101)

		// put the stack at the top of memory
		machine.setValue(0, for: .stackPointer)

		// run for four cycles, and signal an NMI
		machine.runForNumber(ofCycles: 4)
		machine.nmiLine = true

		// run for four more cycles to get to where the NMI should be recognised
		machine.runForNumber(ofCycles: 4)
		XCTAssertEqual(machine.value(for: .programCounter), 0x0102)

		// run for eleven more cycles to allow the NMI to begin
		machine.runForNumber(ofCycles: 11)

		// confirm that the PC is now at 0x66, that the old is on the stack and
		// that IFF1 has migrated to IFF2
		XCTAssertEqual(machine.value(for: .programCounter), 0x66)
		XCTAssertEqual(machine.value(atAddress: 0xffff), 0x01)
		XCTAssertEqual(machine.value(atAddress: 0xfffe), 0x02)
		XCTAssertEqual(machine.value(for: .IFF2), 0)
	}

	func testIRQDisabled() {
		let machine = CSTestMachineZ80()

		// start the PC at 0x0100, interrupts disabled
		machine.setValue(0x0100, for: .programCounter)
		machine.setValue(0, for: .IFF1)
		machine.setValue(0, for: .IFF2)

		// install six NOPs
		for address: UInt16 in 0x0100 ..< 0x0106 {
			machine.setValue(0x00, atAddress: address)
		}

		// replace the fourth NOP with an EI
		machine.setValue(0xfb, atAddress: 0x0103)

		// run for four cycles, signal IRQ and run for 8 more
		machine.runForNumber(ofCycles: 4)
		machine.irqLine = true
		machine.runForNumber(ofCycles: 8)

		// confirm that the request was ignored
		XCTAssertEqual(machine.value(for: .programCounter), 0x0103)

		// run for 12 more cycles, hitting the EI and, if no interrupt occured, the two NOPs after it
		machine.runForNumber(ofCycles: 12)

		// confirm that an interruption occurred, causing the PC not yet to have proceeded beyond 0x0105
		XCTAssertEqual(machine.value(for: .programCounter), 0x0105)
	}

	func testIRQMode0() {
		// In interrupt mode 0, receipt of an IRQ causes an instruction to be read from the bus and
		// executed, with a two-cycle penalty. The test machine posts 0x21 during an interrupt acknowledge
		// cycle so the instruction that is executed will be LD HL, nnnn

		let machine = CSTestMachineZ80()

		// start the PC at 0x0100 and install two NOPs for it and then one copy of 0x83, then
		// something that isn't 0x83, and ensuring interrupts are enabled and in mode 1
		machine.setValue(0x0100, for: .programCounter)
		machine.setValue(1, for: .IFF1)
		machine.setValue(0, for: .IM)
		machine.setValue(0x1010, for: .HL)
		machine.setValue(0x00, atAddress: 0x0100)
		machine.setValue(0x00, atAddress: 0x0101)
		machine.setValue(0x83, atAddress: 0x0102)
		machine.setValue(0x9b, atAddress: 0x0103)

		// run for four cycles, and signal an IRQ
		machine.runForNumber(ofCycles: 4)
		machine.irqLine = true

		// run for four more cycles to get to where the IRQ should be recognised
		machine.runForNumber(ofCycles: 4)
		XCTAssertEqual(machine.value(for: .programCounter), 0x0102)

		// run for twelve more cycles, to complete a LD HL, nnnn with the additional two cycles
		// of cost for it being an IRQ program
		machine.runForNumber(ofCycles: 12)

		// confirm that the PC is still where it was, but HL is now 0x8383, and interrupts are disabled
		XCTAssertEqual(machine.value(for: .programCounter), 0x0102)
		XCTAssertEqual(machine.value(for: .HL), 0x8383)
		XCTAssertEqual(machine.value(for: .IFF1), 0)
	}

	func testIRQMode1() {
		// In interrupt mode 1, receipt of an IRQ means that the interrupt flag is disabled,
		// the PC is pushed to the stack and execution resumes at 0x38.

		let machine = CSTestMachineZ80()

		// start the PC at 0x0100 and install three NOPs for it, ensuring interrupts are enabled
		// and in mode 1
		machine.setValue(0x0100, for: .programCounter)
		machine.setValue(1, for: .IFF1)
		machine.setValue(1, for: .IM)
		machine.setValue(0x00, atAddress: 0x0100)
		machine.setValue(0x00, atAddress: 0x0101)
		machine.setValue(0x00, atAddress: 0x0102)

		// put the stack at the top of memory
		machine.setValue(0, for: .stackPointer)

		// run for four cycles, and signal an IRQ
		machine.runForNumber(ofCycles: 4)
		machine.irqLine = true

		// run for four more cycles to get to where the IRQ should be recognised
		machine.runForNumber(ofCycles: 4)
		XCTAssertEqual(machine.value(for: .programCounter), 0x0102)

		// run for eleven more cycles to allow the IRQ to begin
		machine.runForNumber(ofCycles: 13)

		// confirm that the PC is now at 0x38, that the old is on the stack and
		// that interrupts are now disabled
		XCTAssertEqual(machine.value(for: .programCounter), 0x38)
		XCTAssertEqual(machine.value(atAddress: 0xffff), 0x01)
		XCTAssertEqual(machine.value(atAddress: 0xfffe), 0x02)
		XCTAssertEqual(machine.value(for: .IFF1), 0)
	}

	func testIRQMode2() {
		// In interrupt mode 2, the current bus value is combined with the I register to look
		// up a vector from memory; the PC calls that vector. Total cost: 19 cycles.

		let machine = CSTestMachineZ80()

		// start the PC at 0x0100 and install two NOPs for it and then one copy of 0x83, then
		// something that isn't 0x83, and ensuring interrupts are enabled and in mode 1
		machine.setValue(0x0100, for: .programCounter)
		machine.setValue(1, for: .IFF1)
		machine.setValue(2, for: .IM)
		machine.setValue(0x00, atAddress: 0x0100)
		machine.setValue(0x00, atAddress: 0x0101)

		// set I to 0x0200 to establish the location of our vector table, and because the test
		// machine will post a 0x21 in response to the interrupt cycle, at 0x0221 put the
		// arbitrarily-chosen address 0x8049
		machine.setValue(0x02, for: .I)
		machine.setValue(0x49, atAddress: 0x0221)
		machine.setValue(0x80, atAddress: 0x0222)

		// put the stack at the top of memory
		machine.setValue(0, for: .stackPointer)

		// run for four cycles, and signal an IRQ
		machine.runForNumber(ofCycles: 4)
		machine.irqLine = true

		// run for four more cycles to get to where the IRQ should be recognised
		machine.runForNumber(ofCycles: 4)
		XCTAssertEqual(machine.value(for: .programCounter), 0x0102)

		// run for nineteen more cycles, to complete the interrupt beginning
		machine.runForNumber(ofCycles: 19)

		// confirm that the PC is now at 0x8049, the old is on the stack, and interrupts
		// are disabled
		XCTAssertEqual(machine.value(for: .programCounter), 0x8049)
		XCTAssertEqual(machine.value(atAddress: 0xffff), 0x01)
		XCTAssertEqual(machine.value(atAddress: 0xfffe), 0x02)
		XCTAssertEqual(machine.value(for: .IFF1), 0)
	}
}
