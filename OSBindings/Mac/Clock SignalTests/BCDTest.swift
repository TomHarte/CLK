//
//  BCDTest.swift
//  Clock Signal
//
//  Created by Thomas Harte on 04/10/2016.
//  Copyright © 2016 Thomas Harte. All rights reserved.
//

import Foundation
import XCTest

class BCDTest: XCTestCase, CSTestMachine6502JamHandler {

	func testBCD() {
		if let filename = Bundle(for: type(of: self)).path(forResource: "BCDTEST_beeb", ofType: nil) {
			if let bcdTest = try? Data(contentsOf: URL(fileURLWithPath: filename)) {
				let machine = CSTestMachine6502()
				machine.jamHandler = self

				machine.setData(bcdTest, atAddress: 0x2900)

				// install a launchpad
				machine.setValue(0x20, forAddress:0x200)	// JSR 0x2900
				machine.setValue(0x00, forAddress:0x201)
				machine.setValue(0x29, forAddress:0x202)
				machine.setValue(0x4c, forAddress:0x203)	// JMP 0x0203
				machine.setValue(0x03, forAddress:0x204)
				machine.setValue(0x02, forAddress:0x205)

				machine.setValue(0x200, for: CSTestMachine6502Register.programCounter)

				machine.setValue(CSTestMachine6502JamOpcode, forAddress:0xffee)	// OSWRCH
				machine.setValue(CSTestMachine6502JamOpcode, forAddress:0xffff)	// end of test

				while(machine.value(for: .programCounter) != 0x203) {
					machine.runForNumber(ofCycles: 1000)
				}
				XCTAssert(machine.value(forAddress:0x84) == 0, output)
			}
		}
	}

	fileprivate var output: String = ""
	func testMachine(_ machine: CSTestMachine6502!, didJamAtAddress address: UInt16) {

		switch address {
			case 0xffee:
				let character = machine.value(for: CSTestMachine6502Register.A)
				output.append(Character(UnicodeScalar(character)!))

				machine.returnFromSubroutine()

			default:
				let hexAddress = String(format:"%04x", address)
				NSException(name: NSExceptionName(rawValue: "Failed Test"), reason: "Processor jammed unexpectedly at \(hexAddress)", userInfo: nil).raise()
		}
	}
}
