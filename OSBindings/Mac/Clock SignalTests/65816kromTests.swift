//
//  krom65816Tests.swift
//  Clock Signal
//
//  Created by Thomas Harte on 02/11/2020.
//  Copyright 2020 Thomas Harte. All rights reserved.
//

import Foundation
import XCTest

// This utilises krom's SNES-centric 65816 tests, comparing step-by-step to
// the traces offered by LilaQ at emudev.de as I don't want to implement a
// SNES just for the sake of result inspection.
//
// So:
// https://github.com/PeterLemon/SNES/tree/master/CPUTest/CPU for the tests;
// https://emudev.de/q00-snes/65816-the-cpu/ for the traces.
class Krom65816Tests: XCTestCase {

	// MARK: - Test Machine

	func runTest(_ name: String) {
		var testData: Data?
		if let filename = Bundle(for: type(of: self)).path(forResource: name, ofType: "sfc") {
			testData = try? Data(contentsOf: URL(fileURLWithPath: filename))
		}

		var testOutput: String?
		if let filename = Bundle(for: type(of: self)).path(forResource: name + "-trace_compare", ofType: "log") {
			testOutput = try? String(contentsOf: URL(fileURLWithPath: filename))
		}

		XCTAssertNotNil(testData)
		XCTAssertNotNil(testOutput)

		let outputLines = testOutput!.components(separatedBy: "\r\n")

		// Assumptions about the SFC file format follow; I couldn't find a spec but those
		// produced by krom appear just to be binary dumps. Fingers crossed!
		let machine = CSTestMachine6502(processor: .processor65816)
		machine.setData(testData!, atAddress: 0x8000)

		// This reproduces the state seen at the first line of all of LilaQ's traces;
		// TODO: determine whether (i) this is the SNES state at reset, or merely how
		// some sort of BIOS leaves it; and (ii) if the former, whether I have post-reset
		// state incorrect. I strongly suspect it's a SNES-specific artefact?
		machine.setValue(0x8000, for: .programCounter)
		machine.setValue(0x0000, for: .A)
		machine.setValue(0x0000, for: .X)
		machine.setValue(0x0000, for: .Y)
		machine.setValue(0x00ff, for: .stackPointer)
		machine.setValue(0x34, for: .flags)

		// There seems to be some Nintendo-special register at address 0x0000.
		machine.setValue(0xb5, forAddress: 0x0000)

		// Poke some fixed values for SNES registers to get past initial setup.
		machine.setValue(0x42, forAddress: 0x4210)	// "RDNMI", apparently; this says: CPU version 2, vblank interrupt request.
		var allowNegativeError = false

		var lineNumber = 1
		var previousPC = 0
		for line in outputLines {
			// At least one of the traces ends with an empty line; my preference is not to
			// modify the originals if possible.
			if line == "" {
				break
			}

			machine.runForNumber(ofInstructions: 1)

			func machineState() -> String {
				// Formulate my 65816 state in the same form as the test machine
				var cpuState = ""
				let emulationFlag = machine.value(for: .emulationFlag) != 0
				cpuState += String(format: "%06x ", machine.value(for: .lastOperationAddress))
				cpuState += String(format: "A:%04x ", machine.value(for: .A))
				cpuState += String(format: "X:%04x ", machine.value(for: .X))
				cpuState += String(format: "Y:%04x ", machine.value(for: .Y))
				if emulationFlag {
					cpuState += String(format: "S:01%02x ", machine.value(for: .stackPointer))
				} else {
					cpuState += String(format: "S:%04x ", machine.value(for: .stackPointer))
				}
				cpuState += String(format: "D:%04x ", machine.value(for: .direct))
				cpuState += String(format: "DB:%02x ", machine.value(for: .dataBank))

				let flags = machine.value(for: .flags)
				cpuState += (flags & 0x80) != 0 ? "N" : "n"
				cpuState += (flags & 0x40) != 0 ? "V" : "v"
				if emulationFlag {
					cpuState += "1B"
				} else {
					cpuState += (flags & 0x20) != 0 ? "M" : "m"
					cpuState += (flags & 0x10) != 0 ? "X" : "x"
				}
				cpuState += (flags & 0x08) != 0 ? "D" : "d"
				cpuState += (flags & 0x04) != 0 ? "I" : "i"
				cpuState += (flags & 0x02) != 0 ? "Z" : "z"
				cpuState += (flags & 0x01) != 0 ? "C" : "c"

				cpuState += " "

				return cpuState
			}

			// Permit a fix-up of the negative flag only if this line followed a test of $4210.
			var cpuState = machineState()
			if cpuState != line && allowNegativeError {
				machine.setValue(machine.value(for: .flags) ^ 0x80, for: .flags)
				cpuState = machineState()
			}

			XCTAssertEqual(cpuState, line, "Mismatch on line #\(lineNumber); after instruction #\(String(format:"%02x", machine.value(forAddress: UInt32(previousPC))))")
			if cpuState != line {
				break
			}
			lineNumber += 1
			previousPC = Int(machine.value(for: .lastOperationAddress))

			// Check whether a 'RDNMI' toggle needs to happen by peeking at the next instruction;
			// if it's BIT $4210 then toggle the top bit at address $4210.
			//
			// Coupling here: assume that by the time the test 65816 is aware it's on a new instruction
			// it's because the actual 65816 has read a new opcode, and that if the 65816 has just read
			// a new opcode then it has already advanced the program counter.
			let programCounter = machine.value(for: .programCounter)
			let nextInstr = [
				machine.value(forAddress: UInt32(programCounter - 1)),
				machine.value(forAddress: UInt32(programCounter + 0)),
				machine.value(forAddress: UInt32(programCounter + 1))
			]
			allowNegativeError = nextInstr[0] == 0x2c && nextInstr[1] == 0x10 && nextInstr[2] == 0x42
		}
	}

	// MARK: - Tests

	func testADC() {	runTest("CPUADC")	}
	func testAND() {	runTest("CPUAND")	}
	func testASL() {	runTest("CPUASL")	}
	func testBIT() {	runTest("CPUBIT")	}
	func testBRA() {	runTest("CPUBRA")	}
	func testCMP() {	runTest("CPUCMP")	}
	func testDEC() {	runTest("CPUDEC")	}
	func testEOR() {	runTest("CPUEOR")	}
	func testINC() {	runTest("CPUINC")	}
	func testJMP() {	runTest("CPUJMP")	}
	func testLDR() {	runTest("CPULDR")	}
	func testLSR() {	runTest("CPULSR")	}
	func testMOV() {	runTest("CPUMOV")	}
	func testORA() {	runTest("CPUORA")	}
	func testPHL() {	runTest("CPUPHL")	}
	func testPSR() {	runTest("CPUPSR")	}
	func testROL() {	runTest("CPUROL")	}
	func testROR() {	runTest("CPUROR")	}
	func testSBC() {	runTest("CPUSBC")	}
	func testSTR() {	runTest("CPUSTR")	}
	func testTRN() {	runTest("CPUTRN")	}

	// MSC isn't usable because it tests WAI and STP, having set itself up to receive
	// some SNES-specific interrupts. Luckily those are the two final things it tests,
	// and life seems to be good until there.
	//
	// TODO: reintroduce with a special stop-before inserted?
//	func testMSC() {	runTest("CPUMSC")	}
}
