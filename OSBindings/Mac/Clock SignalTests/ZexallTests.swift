//
//  ZexallTests.swift
//  Clock Signal
//
//  Created by Thomas Harte on 16/05/2017.
//  Copyright © 2017 Thomas Harte. All rights reserved.
//

import XCTest
import Foundation

class ZexallTests: XCTestCase, CSTestMachineTrapHandler {

	func testZexall() {
		if let filename = Bundle(for: type(of: self)).path(forResource: "zexall", ofType: "com") {
			if let testData = try? Data(contentsOf: URL(fileURLWithPath: filename)) {

				// install test program, at the usual CP/M place
				let machine = CSTestMachineZ80()
				machine.setData(testData, atAddress: 0x0100)

				// add a RET at the CP/M entry location, and establish it as a trap location
				machine.setValue(0xc9, atAddress: 0x0005)
				machine.addTrapAddress(0x0005);
				machine.trapHandler = self

				// seed execution at 0x0100
				machine.setValue(0x0100, for: .programCounter)

				// run!
				machine.runForNumber(ofCycles: 20)
			}
		}
	}

	func testMachine(_ testMachine: CSTestMachineZ80!, didTrapAtAddress address: UInt16) {
		// only 0x0005 was registered as a trap address, so no need further to inspect
		let cRegister = testMachine.value(for: .C)
		if cRegister == 9 {
			var address = testMachine.value(for: .DE)
			var character: Character = " "
			var output = ""
			while true {
				character = Character(UnicodeScalar(testMachine.value(atAddress: address)))
				if character == "$" {
					break
				}
				output = output + String(character)
				address = address + 1
			}
			print(output)
		}
		if cRegister == 5 {
			print(String(describing: UnicodeScalar(testMachine.value(for: .E))))
		}
	}
}
