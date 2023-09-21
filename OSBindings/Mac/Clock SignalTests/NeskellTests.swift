//
//  KlausDormanTests.swift
//  Clock Signal
//
//  Created by Thomas Harte on 16/07/2015.
//  Copyright 2015 Thomas Harte. All rights reserved.
//

import Foundation
import XCTest

// The below reuses the Neskell — https://github.com/blitzcode/neskell — tests and therefore attempts to transcribe
// https://github.com/blitzcode/neskell/blob/b4bfec6d6f0cf88d8d2de61585017d16a37e3b9a/src/Test.hs

class NeskellTests: XCTestCase {
	private func runTest(resource: String, codeAddress: UInt32) -> CSTestMachine6502? {
		if let filename = Bundle(for: type(of: self)).path(forResource: resource, ofType: "bin") {
			if let functionalTest = try? Data(contentsOf: URL(fileURLWithPath: filename)) {
				let machine = CSTestMachine6502(processor: .processor6502)

				machine.setData(functionalTest, atAddress: codeAddress)
				machine.setValue(UInt16(codeAddress), for: .programCounter)
				machine.setValue(0xff, for: .stackPointer)

				// Install the halt-forever trailer.
				let targetAddress = UInt32(codeAddress + UInt32(functionalTest.count))
				let infiniteStop = Data([0x4c, UInt8(targetAddress & 0xff), UInt8((targetAddress >> 8) & 0xff)])	// i.e. JMP to self.
				machine.setData(infiniteStop, atAddress: targetAddress)

				while true {
					let oldPC = machine.value(for: .lastOperationAddress)
					machine.runForNumber(ofCycles: 1000)
					let newPC = machine.value(for: .lastOperationAddress)

					if newPC == oldPC {
						return machine
					}
				}
			}
		}

		return nil
	}

	private func assertStack(machine: CSTestMachine6502, contents: [UInt8]) {
		let stackStart = UInt32(0x101 + machine.value(for: .stackPointer))
		let stackLength = UInt32(0x200 - stackStart)
		let stackData = machine.data(atAddress: stackStart, length: stackLength)
		XCTAssertEqual(stackData, Data(contents));
	}

	func testAHX_TAS_SHX_SHY() {
		if let result = runTest(resource: "ahx_tas_shx_shy_test", codeAddress: 0x600) {
			XCTAssertEqual(result.value(for: .stackPointer), 0xf2)
			assertStack(machine: result, contents: [0x01, 0xC9, 0x01, 0x80, 0xC0, 0xE0, 0x01, 0x55, 0x80, 0x80, 0x01, 0x34, 0x10]);
		}
	}

	func testAHX_TAS_SHX_SHY_pagecross() {
		if let result = runTest(resource: "ahx_tas_shx_shy_pagecross_test", codeAddress: 0x600) {
			XCTAssertEqual(result.value(for: .stackPointer), 0xf5)
			assertStack(machine: result, contents: [0x42, 0x42, 0x41, 0x41, 0x00, 0x01, 0xCE, 0xCF, 0xC0, 0xD0]);
		}
	}

	func testBCD() {
		if let result = runTest(resource: "bcd_add_sub_test", codeAddress: 0x600) {
			XCTAssertEqual(result.value(for: .stackPointer), 0xf6)
			XCTAssertEqual(result.value(for: .flags), 0xbc)	// Diverge from original in taking B as set when magically
															// peaking inside.
			assertStack(machine: result, contents: [0x87, 0x91, 0x29, 0x27, 0x34, 0x73, 0x41, 0x46, 0x05]);
		}
	}
}
