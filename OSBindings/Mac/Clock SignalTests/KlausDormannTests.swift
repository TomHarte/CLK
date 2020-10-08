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

	fileprivate func runTest(resource: String, processor: CSTestMachine6502Processor) -> UInt16 {
		if let filename = Bundle(for: type(of: self)).path(forResource: resource, ofType: "bin") {
			if let functionalTest = try? Data(contentsOf: URL(fileURLWithPath: filename)) {
				let machine = CSTestMachine6502(processor: processor)

				machine.setData(functionalTest, atAddress: 0)
				machine.setValue(0x400, for: .programCounter)

				while true {
					let oldPC = machine.value(for: .lastOperationAddress)
					machine.runForNumber(ofCycles: 1000)
					let newPC = machine.value(for: .lastOperationAddress)

					if newPC == oldPC {
						machine.runForNumber(ofCycles: 7)

						let retestPC = machine.value(for: .lastOperationAddress)
						if retestPC == oldPC {
							return newPC
						}
					}
				}
			}
		}

		return 0
	}

	func runTest6502(processor: CSTestMachine6502Processor) {
		func errorForTrapAddress(_ address: UInt16) -> String? {
			switch address {
				case 0x3399: return nil // success!

				case 0x052a: return "TAX, DEX or LDA did not correctly set flags, or BEQ did not branch correctly"
				case 0x05db: return "PLP did not affect N flag correctly"
				case 0x26d2: return "ASL zpg,x produced incorrect flags"
				case 0x33a7: return "Decimal ADC result has wrong value"
				case 0x33b9: return "Decimal SBC result has wrong value"
				case 0x33c0: return "Decimal SBC wrong carry flag"
				case 0x3502: return "Binary SBC result has wrong value"
				case 0x364a: return "JMP (addr) acted as JMP addr"
				case 0x36ac, 0x36f6: return "Improper JSR return address on stack"
				case 0x36c6: return "Unexpected RESET"
				case 0x36d1: return "BRK: unexpected BRK or IRQ"
				case 0x36e5: return "BRK flag not set on stack following BRK"
				case 0x36ea: return "BRK did not set the I flag"
				case 0x36fd: return "Wrong address put on stack by BRK"

				case 0: return "Didn't find tests"

				default: return "Unknown error at \(String(format:"%04x", address))"
			}
		}

		let destination = runTest(resource: "6502_functional_test", processor: processor)
		let error = errorForTrapAddress(destination)
		XCTAssert(error == nil, "Failed with error \(error!)")
	}

	func runTest65C02(processor: CSTestMachine6502Processor) {
		func errorForTrapAddress(_ address: UInt16) -> String? {
			switch address {
				case 0x24f1: return nil // success!

				case 0x0423: return "PHX: value of X not on stack page"
				case 0x0428: return "PHX: stack pointer not decremented"
				case 0x042d: return "PLY: didn't acquire value 0xaa from stack"
				case 0x0432: return "PLY: didn't acquire value 0x55 from stack"
				case 0x0437: return "PLY: stack pointer not incremented"
				case 0x043c: return "PLY: stack pointer not incremented"

				case 0x066a: return "BRA: branch not taken"
				case 0x0730: return "BBS: branch not taken"
				case 0x0733: return "BBR: branch taken"

				case 0x2884: return "JMP (abs) exhibited 6502 page-crossing bug"
				case 0x16ca: return "JMP (abs, x) failed"

				case 0x2785: return "BRK didn't clear the decimal mode flag"

				case 0x177b: return "INC A didn't function"

				case 0x1834: return "LDA (zp) acted as JAM"
				case 0x183a: return "STA (zp) acted as JAM"
				case 0x1849: return "LDA/STA (zp) left flags in incorrect state"

				case 0x1983: return "STZ didn't store zero"

				case 0x1b03: return "BIT didn't set flags correctly"
				case 0x1c6c: return "BIT immediate didn't set flags correctly"

				case 0x1d88: return "TRB set Z flag incorrectly"
				case 0x1e7c: return "RMB set flags incorrectly"

				case 0x2245: return "CMP (zero) didn't work"
				case 0x2506: return "Decimal ADC set flags incorrectly"

				case 0: return "Didn't find tests"
				default: return "Unknown error at \(String(format:"%04x", address))"
			}
		}

		let destination = runTest(resource: "65C02_extended_opcodes_test", processor: processor)
		let error = errorForTrapAddress(destination)
		XCTAssert(error == nil, "Failed with error \(error!)")
	}


	/// Runs Klaus Dormann's 6502 tests.
	func test6502() {
		runTest6502(processor: .processor6502)
	}

	/// Runs Klaus Dormann's standard 6502 tests on a 65816.
	func test65816As6502() {
		runTest6502(processor: .processor65816)
	}

	/// Runs Klaus Dormann's 65C02 tests.
	func test65C02() {
		runTest65C02(processor: .processor65C02)
	}

	/// Runs Klaus Dormann's 65C02 tests on a 65816.
	func test65816As65C02() {
		runTest65C02(processor: .processor65816)
	}
}
