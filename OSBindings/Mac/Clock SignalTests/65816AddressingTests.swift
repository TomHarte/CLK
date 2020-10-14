//
//  WDC65816AddressingTests.swift
//  Clock Signal
//
//  Created by Thomas Harte on 13/10/2020.
//  Copyright 2020 Thomas Harte. All rights reserved.
//

import Foundation
import XCTest

// This exactly transcribes the examples given in http://6502.org/tutorials/65c816opcodes.html#5.9
class WDC65816AddressingTests: XCTestCase {

	private func machine16() -> CSTestMachine6502 {
		let machine = CSTestMachine6502(processor: .processor65816)
		machine.setValue(0, for: .emulationFlag)
		machine.setValue(0, for: .flags)
		return machine
	}

	func testAbsolute() {
		let machine = machine16()

		machine.setValue(0x12, for: .dataBank)

		machine.setValue(0xab, forAddress: 0x12ffff)
		machine.setValue(0xcd, forAddress: 0x130000)

		// LDA $ffff; NOP
		machine.setData(Data([0xad, 0xff, 0xff, 0xea]), atAddress: 0x200)

		machine.setValue(0x200, for: .programCounter)
		machine.runForNumber(ofCycles: 6)

		XCTAssertEqual(machine.value(for: .A), 0xcdab)
	}

	func testAbsoluteX() {
		let machine = machine16()

		machine.setValue(0x12, for: .dataBank)
		machine.setValue(0x0a, for: .X)

		machine.setValue(0xab, forAddress: 0x130008)
		machine.setValue(0xcd, forAddress: 0x130009)

		// LDA $fffe, x; NOP
		machine.setData(Data([0xbd, 0xfe, 0xff, 0xea]), atAddress: 0x200)

		machine.setValue(0x200, for: .programCounter)
		machine.runForNumber(ofCycles: 7)

		XCTAssertEqual(machine.value(for: .A), 0xcdab)
	}

	func testJMPAbsoluteIndirect() {
		let machine = machine16()

		machine.setValue(0x12, for: .programBank)
		machine.setValue(0x0a, for: .X)

		machine.setValue(0x34, forAddress: 0x0000)
		machine.setValue(0x56, forAddress: 0xffff)

		// JMP ($ffff); NOP
		machine.setData(Data([0x6c, 0xff, 0xff, 0xea]), atAddress: 0x120200)

		machine.setValue(0x200, for: .programCounter)
		machine.runForNumber(ofCycles: 6)

		XCTAssertEqual(machine.value(for: .programCounter), 0x3456 + 1)
		XCTAssertEqual(machine.value(for: .programBank), 0x12)
	}

	func testIndirectAbsoluteX() {
		let machine = machine16()

		machine.setValue(0x12, for: .programBank)
		machine.setValue(0x0a, for: .X)
		machine.setValue(0x56, forAddress: 0x120008)
		machine.setValue(0x34, forAddress: 0x120009)

		// JMP ($fffe, x); NOP
		machine.setData(Data([0x7c, 0xfe, 0xff, 0xea]), atAddress: 0x120200)

		machine.setValue(0x200, for: .programCounter)
		machine.runForNumber(ofCycles: 7)

		XCTAssertEqual(machine.value(for: .programCounter), 0x3456 + 1)
		XCTAssertEqual(machine.value(for: .programBank), 0x12)
	}

	// TODO:
	//	Direct			[x2]
	//	Direct, X		[x2]
	// 	(Direct)		[x2]
	//	[Direct]
	//	(Direct, X)		[x2]
	//	(Direct), Y		[x2]
	//	[Direct], Y
	//	Long
	//	Long, X
	//	Stack, S
	//	(Stack, S), Y
}
