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

}
