//
//  KlausDormanTests.swift
//  Clock Signal
//
//  Created by Thomas Harte on 16/07/2015.
//  Copyright 2015 Thomas Harte. All rights reserved.
//

import Foundation
import XCTest

class KlausDormannTests: XCTestCase {

	fileprivate func runTest(resource: String, is65C02: Bool) -> UInt16 {
		if let filename = Bundle(for: type(of: self)).path(forResource: resource, ofType: "bin") {
			if let functionalTest = try? Data(contentsOf: URL(fileURLWithPath: filename)) {
				let machine = CSTestMachine6502(is65C02: is65C02)

				machine.setData(functionalTest, atAddress: 0)
				machine.setValue(0x400, for: .programCounter)

				while true {
					let oldPC = machine.value(for: .lastOperationAddress)
					machine.runForNumber(ofCycles: 1000)
					let newPC = machine.value(for: .lastOperationAddress)

					if newPC == oldPC {
						return newPC
					}
				}
			}
		}

		return 0
	}

	/// Runs Klaus Dorman's 6502 tests.
	func test6502() {
		func errorForTrapAddress(_ address: UInt16) -> String? {
			switch address {
				case 0x3399: return nil // success!

				case 0x33a7: return "Decimal ADC result has wrong value"
				case 0x3502: return "Binary SBC result has wrong value"
				case 0x33b9: return "Decimal SBC result has wrong value"
				case 0x33c0: return "Decimal SBC wrong carry flag"
				case 0x36d1: return "BRK: unexpected BRK or IRQ"
				case 0x36ac, 0x36f6: return "Improper JSR return address on stack"
				case 0x36e5: return "BRK flag not set on stack"
				case 0x26d2: return "ASL zpg,x produced incorrect flags"
				case 0x36c6: return "Unexpected RESET"

				case 0: return "Didn't find tests"

				default: return "Unknown error at \(String(format:"%04x", address))"
			}
		}

		let destination = runTest(resource: "6502_functional_test", is65C02: false)
		let error = errorForTrapAddress(destination)
		XCTAssert(error == nil, "Failed with error \(error!)")
	}

	/// Runs Klaus Dorman's 65C02 tests.
	func test65C02() {
		func errorForTrapAddress(_ address: UInt16) -> String? {
			switch address {
				case 0x0423: return "PHX: value of X not on stack page"
				case 0x0428: return "PHX: stack pointer not decremented"
				case 0x042d: return "PLY: didn't acquire value 0xaa from stack"
				case 0x0432: return "PLY: didn't acquire value 0x55 from stack"
				case 0x0437: return "PLY: stack pointer not incremented"
				case 0x043c: return "PLY: stack pointer not incremented"

				case 0: return "Didn't find tests"
				default: return "Unknown error at \(String(format:"%04x", address))"
			}
		}

		let destination = runTest(resource: "65C02_extended_opcodes_test", is65C02: true)
		let error = errorForTrapAddress(destination)
		XCTAssert(error == nil, "Failed with error \(error!)")
	}
}
