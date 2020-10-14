//
//  6502InterruptTests.swift
//  Clock Signal
//
//  Created by Thomas Harte on 28/06/2016.
//  Copyright 2016 Thomas Harte. All rights reserved.
//

import XCTest

class MOS6502InterruptTests: XCTestCase {

	var machine: CSTestMachine6502! = nil
	override func setUp() {
		super.setUp()

		// create a machine full of NOPs
		machine = CSTestMachine6502(processor: .processor6502)
		for c in 0...65535 {
			machine.setValue(0xea, forAddress: UInt32(c))
		}

		// set the IRQ vector to be 0x1234
		machine.setValue(0x34, forAddress: 0xfffe)
		machine.setValue(0x12, forAddress: 0xffff)

		// add a CLI
		machine.setValue(0x58, forAddress: 0x4000)

		// pick things off at 0x4000
		machine.setValue(0x4000, for: CSTestMachine6502Register.programCounter)
	}

	func testIRQLine() {
		// run for six cycles; check that no interrupt has occurred
		machine.runForNumber(ofCycles: 6)
		XCTAssert(machine.value(for: .programCounter) == 0x4003, "No interrupt should have occurred with line low")

		// enable the interrupt line, check that it was too late
		machine.irqLine = true
		machine.runForNumber(ofCycles: 2)
		XCTAssert(machine.value(for: .programCounter) == 0x4004, "No interrupt should have occurred from interrupt raised between instructions")

		// run for a further 7 cycles, confirm that the IRQ vector was jumped to
		machine.runForNumber(ofCycles: 6)
		XCTAssert(machine.value(for: .programCounter) != 0x1234, "Interrupt routine should not yet have begun")
		machine.runForNumber(ofCycles: 1)
		XCTAssert(machine.value(for: .programCounter) == 0x1234, "Interrupt routine should just have begun")
	}

	func testIFlagSet() {
		// enable the interrupt line, run for eleven cycles to get past the CLIP and the following NOP and into the interrupt routine
		machine.irqLine = true
		machine.runForNumber(ofCycles: 11)

		XCTAssert(machine.value(for: .programCounter) == 0x1234, "Interrupt routine should just have begun")
		XCTAssert(machine.value(for: .flags) & 0x04 == 0x04, "Interrupt status flag should be set")
	}

	func testCLISEIFlagClear() {
		// set up an SEI as the second instruction, enable the IRQ line
		machine.setValue(0x78, forAddress: 0x4001)
		machine.irqLine = true

		// run for four cycles; the CLI and SEI should have been performed
		machine.runForNumber(ofCycles: 4)
		XCTAssert(machine.value(for: .programCounter) == 0x4002, "CLI/SEI pair should have been performed in their entirety")

		// run for seven more cycles
		machine.runForNumber(ofCycles: 7)

		// interrupt should have taken place despite SEI
		XCTAssert(machine.value(for: .programCounter) == 0x1234, "Interrupt routine should just have begun")
	}
}
