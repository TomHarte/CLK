//
//  WolfgangLorenzTests.swift
//  Clock Signal
//
//  Created by Thomas Harte on 16/07/2015.
//  Copyright 2015 Thomas Harte. All rights reserved.
//

import XCTest
import Foundation

class WolfgangLorenzTests: XCTestCase, CSTestMachineTrapHandler {

	func testWolfgangLorenzStart()	{
		self.runWolfgangLorenzTest(" start")
	}
	func testWolfgangLorenzLDA()	{
		self.runWolfgangLorenzTest("lda", suffixes: ["b", "z", "zx", "a", "ax", "ay", "ix", "iy"])
	}
	func testWolfgangLorenzSTA()	{
		self.runWolfgangLorenzTest("sta", suffixes: ["z", "zx", "a", "ax", "ay", "ix", "iy"])
	}
	func testWolfgangLorenzLDX()	{
		self.runWolfgangLorenzTest("ldx", suffixes: ["b", "z", "zy", "a", "ay"])
	}
	func testWolfgangLorenzSTX()	{
		self.runWolfgangLorenzTest("stx", suffixes: ["z", "zy", "a"])
	}
	func testWolfgangLorenzLDY()	{
		self.runWolfgangLorenzTest("ldy", suffixes: ["b", "z", "zx", "a", "ax"])
	}
	func testWolfgangLorenzSTY()	{
		self.runWolfgangLorenzTest("sty", suffixes: ["z", "zx", "a"])
	}
	func testWolfgangLorenzTransfers()	{
		self.runWolfgangLorenzTest("taxn")
		self.runWolfgangLorenzTest("tayn")
		self.runWolfgangLorenzTest("txan")
		self.runWolfgangLorenzTest("tyan")
		self.runWolfgangLorenzTest("tsxn")
		self.runWolfgangLorenzTest("txsn")
	}
	func testWolfgangLorenzStack()	{
		self.runWolfgangLorenzTest("phan")
		self.runWolfgangLorenzTest("plan")
		self.runWolfgangLorenzTest("phpn")
		self.runWolfgangLorenzTest("plpn")
	}
	func testWolfgangLorenzIncsAndDecs()	{
		self.runWolfgangLorenzTest("inxn")
		self.runWolfgangLorenzTest("inyn")
		self.runWolfgangLorenzTest("dexn")
		self.runWolfgangLorenzTest("deyn")
		self.runWolfgangLorenzTest("incz")
		self.runWolfgangLorenzTest("inczx")
		self.runWolfgangLorenzTest("inca")
		self.runWolfgangLorenzTest("incax")
		self.runWolfgangLorenzTest("decz")
		self.runWolfgangLorenzTest("deczx")
		self.runWolfgangLorenzTest("deca")
		self.runWolfgangLorenzTest("decax")
	}
	func testWolfgangLorenzASL()	{
		self.runWolfgangLorenzTest("asl", suffixes: ["n", "z", "zx", "a", "ax"])
	}
	func testWolfgangLorenzLSR()	{
		self.runWolfgangLorenzTest("lsr", suffixes: ["n", "z", "zx", "a", "ax"])
	}
	func testWolfgangLorenzROL()	{
		self.runWolfgangLorenzTest("rol", suffixes: ["n", "z", "zx", "a", "ax"])
	}
	func testWolfgangLorenzROR()	{
		self.runWolfgangLorenzTest("ror", suffixes: ["n", "z", "zx", "a", "ax"])
	}
	func testWolfgangLorenzAND()	{
		self.runWolfgangLorenzTest("and", suffixes: ["b", "z", "zx", "a", "ax", "ay", "ix", "iy"])
	}
	func testWolfgangLorenzORA()	{
		self.runWolfgangLorenzTest("ora", suffixes: ["b", "z", "zx", "a", "ax", "ay", "ix", "iy"])
	}
	func testWolfgangLorenzEOR()	{
		self.runWolfgangLorenzTest("eor", suffixes: ["b", "z", "zx", "a", "ax", "ay", "ix", "iy"])
	}
	func testWolfgangLorenzFlagManipulation()	{
		self.runWolfgangLorenzTest("clcn")
		self.runWolfgangLorenzTest("secn")
		self.runWolfgangLorenzTest("cldn")
		self.runWolfgangLorenzTest("sedn")
		self.runWolfgangLorenzTest("clin")
		self.runWolfgangLorenzTest("sein")
		self.runWolfgangLorenzTest("clvn")
	}
	func testWolfgangLorenzADC()	{
		self.runWolfgangLorenzTest("adc", suffixes: ["b", "z", "zx", "a", "ax", "ay", "ix", "iy"])
	}
	func testWolfgangLorenzSBC()	{
		self.runWolfgangLorenzTest("sbc", suffixes: ["b", "z", "zx", "a", "ax", "ay", "ix", "iy"])
	}
	func testWolfgangLorenzCompare()	{
		self.runWolfgangLorenzTest("cmp", suffixes: ["b", "z", "zx", "a", "ax", "ay", "ix", "iy"])
		self.runWolfgangLorenzTest("cpx", suffixes: ["b", "z", "a"])
		self.runWolfgangLorenzTest("cpy", suffixes: ["b", "z", "a"])
	}
	func testWolfgangLorenzBIT()	{
		self.runWolfgangLorenzTest("bit", suffixes: ["z", "a"])
	}
	func testWolfgangLorenzFlow()	{
		self.runWolfgangLorenzTest("brkn")
		self.runWolfgangLorenzTest("rtin")
		self.runWolfgangLorenzTest("jsrw")
		self.runWolfgangLorenzTest("rtsn")
		self.runWolfgangLorenzTest("jmpw")
		self.runWolfgangLorenzTest("jmpi")
	}
	func testWolfgangLorenzBranch()	{
		self.runWolfgangLorenzTest("beqr")
		self.runWolfgangLorenzTest("bner")
		self.runWolfgangLorenzTest("bmir")
		self.runWolfgangLorenzTest("bplr")
		self.runWolfgangLorenzTest("bcsr")
		self.runWolfgangLorenzTest("bccr")
		self.runWolfgangLorenzTest("bvsr")
		self.runWolfgangLorenzTest("bvcr")
	}
	func testWolfgangLorenzNOP()	{
		self.runWolfgangLorenzTest("nop", suffixes: ["n", "b", "z", "zx", "a", "ax"])
	}
	func testWolfgangLorenzASO()	{
		self.runWolfgangLorenzTest("aso", suffixes: ["z", "zx", "a", "ax", "ay", "ix", "iy"])
	}
	func testWolfgangLorenzRLA()	{
		self.runWolfgangLorenzTest("rla", suffixes: ["z", "zx", "a", "ax", "ay", "ix", "iy"])
	}
	func testWolfgangLorenzLSE()	{
		self.runWolfgangLorenzTest("lse", suffixes: ["z", "zx", "a", "ax", "ay", "ix", "iy"])
	}
	func testWolfgangLorenzRRA()	{
		self.runWolfgangLorenzTest("rra", suffixes: ["z", "zx", "a", "ax", "ay", "ix", "iy"])
	}
	func testWolfgangLorenzDCM()	{
		self.runWolfgangLorenzTest("dcm", suffixes: ["z", "zx", "a", "ax", "ay", "ix", "iy"])
	}
	func testWolfgangLorenzINS()	{
		self.runWolfgangLorenzTest("ins", suffixes: ["z", "zx", "a", "ax", "ay", "ix", "iy"])
	}
	func testWolfgangLorenzLAX()	{
		self.runWolfgangLorenzTest("lax", suffixes: ["z", "zy", "a", "ay", "ix", "iy"])
	}
	func testWolfgangLorenzAXS()	{
		self.runWolfgangLorenzTest("axs", suffixes: ["z", "zy", "a", "ix"])
	}
	func testWolfgangLorenzALR()	{
		self.runWolfgangLorenzTest("alrb")
	}
	func testWolfgangLorenzARR()	{
		self.runWolfgangLorenzTest("arrb")
	}
	func testWolfgangLorenzSBX()	{
		self.runWolfgangLorenzTest("sbxb")
	}
	func testWolfgangLorenzSHA()	{
		self.runWolfgangLorenzTest("sha", suffixes: ["ay", "iy"])
	}
	func testWolfgangLorenzSHX()	{
		self.runWolfgangLorenzTest("shxay")
	}
	func testWolfgangLorenzSHY()	{
		self.runWolfgangLorenzTest("shyax")
	}
	func testWolfgangLorenzSHS()	{
		self.runWolfgangLorenzTest("shsay")
	}
	func testWolfgangLorenzLXA()	{
		self.runWolfgangLorenzTest("lxab")
	}
	func testWolfgangLorenzANE()	{
		self.runWolfgangLorenzTest("aneb")
	}
	func testWolfgangLorenzANC()	{
		self.runWolfgangLorenzTest("ancb")
	}
	func testWolfgangLorenzLAS()	{
		self.runWolfgangLorenzTest("lasay")
	}
	func testWolfgangLorenzSBCB()	{
		self.runWolfgangLorenzTest("sbcb(eb)")
	}

	fileprivate func runWolfgangLorenzTest(_ name: String, suffixes: [String]) {
		for suffix in suffixes {
			let testName = name + suffix
			self.runWolfgangLorenzTest(testName)
		}
	}

	fileprivate var output: String = ""
	fileprivate func runWolfgangLorenzTest(_ name: String) {
		var machine: CSTestMachine6502!

		if let filename = Bundle(for: type(of: self)).path(forResource: name, ofType: nil) {
			if let testData = try? Data(contentsOf: URL(fileURLWithPath: filename)) {

				machine = CSTestMachine6502(processor: .processor6502)
				machine.trapHandler = self
//				machine.logActivity = true
				output = ""

				let dataPointer = (testData as NSData).bytes.bindMemory(to: UInt8.self, capacity: testData.count)
				let loadAddress = UInt16(dataPointer[0]) | (UInt16(dataPointer[1]) << 8)
				let contents = testData.subdata(in: 2..<(testData.count - 2))

				machine.setData(contents, atAddress: loadAddress)

				machine.setValue(0x00, forAddress: 0x0002)
				machine.setValue(0x00, forAddress: 0xa002)
				machine.setValue(0x80, forAddress: 0xa003)
				machine.setValue(0xff, forAddress: 0x01fe)
				machine.setValue(0x7f, forAddress: 0x01ff)
				machine.setValue(0x48, forAddress: 0xfffe)
				machine.setValue(0xff, forAddress: 0xffff)

				let irqHandler = Data(bytes: UnsafePointer<UInt8>([
					0x48, 0x8a, 0x48, 0x98, 0x48, 0xba, 0xbd, 0x04, 0x01,
					0x29, 0x10, 0xf0, 0x03, 0x6c, 0x16, 0x03, 0x6c, 0x14, 0x03
				] as [UInt8]), count: 19)
				machine.setData( irqHandler, atAddress: 0xff48)

				machine.addTrapAddress(0xffd2)	// print character
				machine.addTrapAddress(0xffe4)	// scan keyboard

				machine.addTrapAddress(0x8000)	// exit
				machine.addTrapAddress(0xa474)	// exit

				machine.setValue(0x60, forAddress:0xffd2)	// 0x60 is RTS
				machine.setValue(0x60, forAddress:0xffe4)
				machine.setValue(0x60, forAddress:0x8000)
				machine.setValue(0x60, forAddress:0xa474)

				machine.setValue(CSTestMachine6502JamOpcode, forAddress:0xe16f)	// load

				machine.setValue(0x0801, for: CSTestMachine6502Register.programCounter)
				machine.setValue(0xfd, for: CSTestMachine6502Register.stackPointer)
				machine.setValue(0x04, for: CSTestMachine6502Register.flags)
			}
		}

		if machine == nil {
			NSException(name: NSExceptionName(rawValue: "Failed Test"), reason: "Couldn't load file \(name)", userInfo: nil).raise()
		}

		while !machine.isJammed {
			machine.runForNumber(ofCycles: 1000)
		}

		let jammedPC = machine.value(for: CSTestMachine6502Register.lastOperationAddress)
		if jammedPC != 0xe16f {
			let hexAddress = String(format:"%04x", jammedPC)
			NSException(name: NSExceptionName(rawValue: "Failed Test"), reason: "Processor jammed unexpectedly at \(hexAddress)", userInfo: nil).raise()
		}
	}

// MARK: MachineJamHandler

	func petsciiToString(_ string: String) -> String {
		let petsciiToCharCommon: [String] = [
			"?", "?", "?", "[RUN/STOP]", "?", "[WHT]", "?", "?", "[SHIFT DISABLE]", "[SHIFT ENABLE]", "?", "?", "?", "\r", "[TEXT MODE]", "?",
			"?", "\n", "[RVS ON]", "[HOME]", "[DEL]", "?", "?", "?", "?", "?", "?", "?", "[RED]", "[RIGHT]", "[GRN]", "[BLU]",

			" ", "!", "\"", "#", "$", "%", "&", "'", "(", ")", "*", "+", ",", "-", ".", "/",
			"0", "1", "2", "3", "4", "5", "6", "7", "8", "9", ":", ";", "<", "=", ">", "?",
		];

		let petsciiToCharRegular: [String] = [
			"@", "a", "b", "c", "d", "e", "f", "g", "h", "i", "j", "k", "l", "m", "n", "o",
			"p", "q", "r", "s", "t", "u", "v", "w", "x", "y", "z", "[", "£", "]", "↑", "←",
		]
		let petsciiToCharInverse: [String] = [
			"@", "A", "B", "C", "D", "E", "F", "G", "H", "I", "J", "K", "L", "M", "N", "O",
			"P", "Q", "R", "S", "T", "U", "V", "W", "X", "Y", "Z", "[", "£", "]", "↑", "←",
		]

		var result: String = ""
		for character in string.utf16 {
			let charInt: Int = Int(character)

			var stringToAppend = ""

			if charInt&0x7f < petsciiToCharCommon.count {
				stringToAppend = petsciiToCharCommon[charInt&0x7f]
			} else {
				let lookupTable = (charInt > 0x80) ? petsciiToCharInverse : petsciiToCharRegular
				let lookupIndex = (charInt&0x7f) - petsciiToCharCommon.count
				if lookupIndex < lookupTable.count {
					stringToAppend = lookupTable[lookupIndex]
				} else {
					stringToAppend += "!"
				}
			}

			result += stringToAppend
		}

		return result
	}

	func testMachine(_ testMachine: CSTestMachine, didTrapAtAddress address: UInt16) {
		let testMachine6502 = testMachine as! CSTestMachine6502
		switch address {
			case 0xffd2:
				testMachine6502.setValue(0x00, forAddress: 0x030c)

				let character = testMachine6502.value(for: CSTestMachine6502Register.A)
				output.append(Character(UnicodeScalar(character)!))

			case 0xffe4:
				testMachine6502.setValue(0x3, for:CSTestMachine6502Register.A)

			case 0x8000, 0xa474:
				NSException(name: NSExceptionName(rawValue: "Failed test"), reason: self.petsciiToString(output), userInfo: nil).raise()

			case 0x0000:
				NSException(name: NSExceptionName(rawValue: "Failed test"), reason: "Execution hit 0000", userInfo: nil).raise()

			default:
				let hexAddress = String(format:"%04x", address)
				NSException(name: NSExceptionName(rawValue: "Failed Test"), reason: "Processor jammed unexpectedly at \(hexAddress)", userInfo: nil).raise()
		}
	}

}
