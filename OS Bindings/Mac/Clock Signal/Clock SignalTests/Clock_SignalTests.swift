//
//  Clock_SignalTests.swift
//  Clock SignalTests
//
//  Created by Thomas Harte on 16/07/2015.
//  Copyright © 2015 Thomas Harte. All rights reserved.
//

import XCTest
@testable import Clock_Signal

class Clock_SignalTests: XCTestCase, CSTestMachineJamHandler {
    
    func testAllSuiteA() {
        if let filename = NSBundle(forClass: self.dynamicType).pathForResource("AllSuiteA", ofType: "bin") {
            if let allSuiteA = NSData(contentsOfFile: filename) {
                let machine = CSTestMachine()
                machine.jamHandler = self

                machine.setData(allSuiteA, atAddress: 0x4000)
                machine.setValue(CSTestMachineJamOpcode, forAddress:0x45c0);  // end

                machine.setValue(0x4000, forRegister: CSTestMachineRegister.ProgramCounter)
                while !machine.isJammed {
                    machine.runForNumberOfCycles(1000)
                }

                if machine.valueForAddress(0x0210) != 0xff {
					NSException(name: "Failed AllSuiteA", reason: "Failed test \(machine.valueForAddress(0x0210))", userInfo: nil).raise()
				}
            }
        }
    }

    func testKlausDormann() {

        func errorForTrapAddress(address: UInt16) -> String? {
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

                default: return "Unknown error at \(hexAddress)"
            }
        }

        if let filename = NSBundle(forClass: self.dynamicType).pathForResource("6502_functional_test", ofType: "bin") {
            if let functionalTest = NSData(contentsOfFile: filename) {
                let machine = CSTestMachine()

                machine.setData(functionalTest, atAddress: 0)
                machine.setValue(0x400, forRegister: CSTestMachineRegister.ProgramCounter)

                while true {
                    let oldPC = machine.valueForRegister(CSTestMachineRegister.LastOperationAddress)
                    machine.runForNumberOfCycles(1000)
                    let newPC = machine.valueForRegister(CSTestMachineRegister.LastOperationAddress)

                    if newPC == oldPC {
                        let error = errorForTrapAddress(oldPC)

                        if let error = error {
                            NSException(name: "Failed test", reason: error, userInfo: nil).raise()
                        } else {
                            return
                        }
                    }
                }
            }
        }

    }

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

	private func runWolfgangLorenzTest(name: String, suffixes: [String]) {
		for suffix in suffixes {
			let testName = name + suffix
			self.runWolfgangLorenzTest(testName)
		}
	}

    private var output: String = ""
    private func runWolfgangLorenzTest(name: String) {

		var machine: CSTestMachine!

        if let filename = NSBundle(forClass: self.dynamicType).pathForResource(name, ofType: nil) {
            if let testData = NSData(contentsOfFile: filename) {

				machine = CSTestMachine()
				machine.jamHandler = self
//				machine.logActivity = true
				output = ""

                let dataPointer = UnsafePointer<UInt8>(testData.bytes)
                let loadAddress = UInt16(dataPointer[0]) | (UInt16(dataPointer[1]) << 8)
                let contents = testData.subdataWithRange(NSMakeRange(2, testData.length - 2))

                machine.setData(contents, atAddress: loadAddress)

                machine.setValue(0x00, forAddress: 0x0002)
                machine.setValue(0x00, forAddress: 0xa002)
                machine.setValue(0x80, forAddress: 0xa003)
                machine.setValue(0xff, forAddress: 0x01fe)
                machine.setValue(0x7f, forAddress: 0x01ff)
                machine.setValue(0x48, forAddress: 0xfffe)
                machine.setValue(0xff, forAddress: 0xffff)

                let irqHandler = NSData(bytes: [
                    0x48, 0x8a, 0x48, 0x98, 0x48, 0xba, 0xbd, 0x04, 0x01,
                    0x29, 0x10, 0xf0, 0x03, 0x6c, 0x16, 0x03, 0x6c, 0x14, 0x03
                ] as [UInt8], length: 19)
                machine.setData( irqHandler, atAddress: 0xff48)

                machine.setValue(CSTestMachineJamOpcode, forAddress:0xffd2);  // print character
                machine.setValue(CSTestMachineJamOpcode, forAddress:0xe16f);  // load
                machine.setValue(CSTestMachineJamOpcode, forAddress:0xffe4);  // scan keyboard
                machine.setValue(CSTestMachineJamOpcode, forAddress:0x8000);  // exit
                machine.setValue(CSTestMachineJamOpcode, forAddress:0xa474);  // exit

                machine.setValue(0x0801, forRegister: CSTestMachineRegister.ProgramCounter)
                machine.setValue(0xfd, forRegister: CSTestMachineRegister.StackPointer)
                machine.setValue(0x04, forRegister: CSTestMachineRegister.Flags)
            }
        }

		if machine == nil {
			NSException(name: "Failed Test", reason: "Couldn't load file \(name)", userInfo: nil).raise()
		}

        while !machine.isJammed {
            machine.runForNumberOfCycles(1000)
        }

		let jammedPC = machine.valueForRegister(CSTestMachineRegister.LastOperationAddress)
		if jammedPC != 0xe16f {
			let hexAddress = String(format:"%04x", jammedPC)
			NSException(name: "Failed Test", reason: "Processor jammed unexpectedly at \(hexAddress)", userInfo: nil).raise()
		}
    }

// MARK: MachineJamHandler

    func petsciiToString(string: String) -> String {
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

    func testMachine(machine: CSTestMachine!, didJamAtAddress address: UInt16) {

        switch address {
            case 0xffd2:
                machine.setValue(0x00, forAddress: 0x030c)

                let character = machine.valueForRegister(CSTestMachineRegister.A)
                output.append(Character(UnicodeScalar(character)))

                machine.returnFromSubroutine()

            case 0xffe4:
                machine.setValue(0x3, forRegister:CSTestMachineRegister.A)
                machine.returnFromSubroutine()

            case 0x8000, 0xa474:
                NSException(name: "Failed test", reason: self.petsciiToString(output), userInfo: nil).raise()

			case 0x0000:
                NSException(name: "Failed test", reason: "Execution hit 0000", userInfo: nil).raise()

            case 0xe16f, 0x45c0:    // Wolfgang Lorenz load next (which we consider to be success)
			break;

            default:
				let hexAddress = String(format:"%04x", address)
				NSException(name: "Failed Test", reason: "Processor jammed unexpectedly at \(hexAddress)", userInfo: nil).raise()
        }
    }

}
