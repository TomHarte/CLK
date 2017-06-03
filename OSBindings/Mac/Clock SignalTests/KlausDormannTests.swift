//
//  KlausDormanTests.swift
//  Clock Signal
//
//  Created by Thomas Harte on 16/07/2015.
//  Copyright © 2015 Thomas Harte. All rights reserved.
//

import Foundation
import XCTest

class KlausDormannTests: XCTestCase {

	func testKlausDormann() {

		func errorForTrapAddress(_ address: UInt16) -> String? {
			let hexAddress = String(format:"%04x", address)
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

				default: return "Unknown error at \(hexAddress)"
			}
		}

		if let filename = Bundle(for: type(of: self)).path(forResource: "6502_functional_test", ofType: "bin") {
			if let functionalTest = try? Data(contentsOf: URL(fileURLWithPath: filename)) {
				let machine = CSTestMachine6502()

				machine.setData(functionalTest, atAddress: 0)
				machine.setValue(0x400, for: .programCounter)

				while true {
					let oldPC = machine.value(for: .lastOperationAddress)
					machine.runForNumber(ofCycles: 1000)
					let newPC = machine.value(for: .lastOperationAddress)

					if newPC == oldPC {
						let error = errorForTrapAddress(oldPC)
						XCTAssert(error == nil, "Failed with error \(error!)")
						return
					}
				}
			}
		}
	}
}
