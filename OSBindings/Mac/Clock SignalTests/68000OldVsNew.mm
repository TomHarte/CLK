//
//  68000ArithmeticTests.m
//  Clock SignalTests
//
//  Created by Thomas Harte on 28/06/2019.
//
//  Largely ported from the tests of the Portable 68k Emulator.
//

#import <XCTest/XCTest.h>

#include "68000.hpp"
#include "68000Mk2.hpp"

#include <array>
#include <unordered_map>
#include <set>

namespace {

struct RandomStore {
	using CollectionT = std::unordered_map<uint32_t, std::pair<uint8_t, uint8_t>>;
	CollectionT values;

	void flag(uint32_t address, uint8_t participant) {
		values[address].first |= participant;
	}

	bool has(uint32_t address, uint8_t participant) {
		auto entry = values.find(address);
		if(entry == values.end()) return false;
		return entry->second.first & participant;
	}

	uint8_t value(uint32_t address, uint8_t participant) {
		auto entry = values.find(address);
		if(entry != values.end()) {
			entry->second.first |= participant;
			return entry->second.second;
		}

		const uint8_t value = uint8_t(rand() >> 8);
		values[address] = std::make_pair(participant, value);
		return value;
	}

	void clear() {
		values.clear();
	}

};

struct Transaction {
	HalfCycles timestamp;
	uint8_t function_code = 0;
	uint32_t address = 0;
	uint16_t value = 0;
	bool address_strobe = false;
	bool read = false;
	int data_strobes = 0;

	bool operator !=(const Transaction &rhs) const {
		if(timestamp != rhs.timestamp) return true;
//		if(function_code != rhs.function_code) return true;
		if(address != rhs.address) return true;
		if(value != rhs.value) return true;
		if(address_strobe != rhs.address_strobe) return true;
		if(data_strobes != rhs.data_strobes) return true;
		return false;
	}

	void print() const {
		printf("%d: %d%d%d %c %c%c @ %06x %s %04x\n",
			timestamp.as<int>(),
			(function_code >> 2) & 1,
			(function_code >> 1) & 1,
			(function_code >> 0) & 1,
			address_strobe ? 'a' : '-',
			(address_strobe & 1) ? 'b' : '-',
			(address_strobe & 2) ? 'w' : '-',
			address,
			read ? "->" : "<-",
			value);
	}
};

struct HarmlessStopException {};

struct BusHandler {
	BusHandler(RandomStore &_store, uint8_t _participant) : store(_store), participant(_participant) {}

	void will_perform(uint32_t, uint16_t) {
		--instructions;
		if(instructions < 0) {
			throw HarmlessStopException{};
		}
	}

	template <typename Microcycle> HalfCycles perform_bus_operation(const Microcycle &cycle, bool is_supervisor) {
		Transaction transaction;

		// Fill all of the transaction record except the data field; will do that after
		// any potential read.
		if(cycle.operation & Microcycle::InterruptAcknowledge) {
			transaction.function_code = 0b111;
		} else {
			transaction.function_code = is_supervisor ? 0x4 : 0x0;
			transaction.function_code |= (cycle.operation & Microcycle::IsData) ? 0x1 : 0x2;
		}
		transaction.address_strobe = cycle.operation & (Microcycle::NewAddress | Microcycle::SameAddress);
		transaction.data_strobes = cycle.operation & (Microcycle::SelectByte | Microcycle::SelectWord);
		if(cycle.address) transaction.address = *cycle.address & 0xffff'ff;
		transaction.timestamp = time;
		transaction.read = cycle.operation & Microcycle::Read;

		time += cycle.length;

		// Do the operation...
		const uint32_t address = cycle.address ? (*cycle.address & 0xffff'ff) : 0;
		switch(cycle.operation & (Microcycle::SelectWord | Microcycle::SelectByte | Microcycle::Read)) {
			default: break;

			case Microcycle::SelectWord | Microcycle::Read:
				if(!store.has(address, participant)) {
					ram[address] = store.value(address, participant);
				}
				if(!store.has(address+1, participant)) {
					ram[address+1] = store.value(address+1, participant);
				}

				cycle.set_value16((ram[address] << 8) | ram[address + 1]);
			break;
			case Microcycle::SelectByte | Microcycle::Read:
				if(!store.has(address, participant)) {
					ram[address] = store.value(address, participant);
				}

				if(address & 1) {
					cycle.set_value8_low(ram[address]);
				} else {
					cycle.set_value8_high(ram[address]);
				}
			break;
			case Microcycle::SelectWord:
				ram[address] = cycle.value8_high();
				ram[address+1] = cycle.value8_low();
				store.flag(address, participant);
				store.flag(address+1, participant);
			break;
			case Microcycle::SelectByte:
				ram[address] = (address & 1) ? cycle.value8_low() : cycle.value8_high();
				store.flag(address, participant);
			break;
		}


		// Add the data value if relevant.
		if(transaction.data_strobes) {
			transaction.value = cycle.value16();
		}

		// Push back only if interesting.
		if(transaction.address_strobe || transaction.data_strobes || transaction.function_code == 7) {
			if(transaction_delay) {
				--transaction_delay;

				// Start counting time only from the first recorded transaction.
				if(!transaction_delay) {
					time = HalfCycles(0);
				}
			} else {
				transactions.push_back(transaction);
			}
		}

		return HalfCycles(0);
	}

	void flush() {}

	int transaction_delay;
	int instructions;

	HalfCycles time;
	std::vector<Transaction> transactions;
	std::array<uint8_t, 16*1024*1024> ram;

	void set_default_vectors() {
		// Establish that all exception vectors point to 1024-byte blocks of memory.
		for(int c = 0; c < 256; c++) {
			const uint32_t target = (c + 2) << 10;
			const uint32_t address = c << 2;
			ram[address + 0] = uint8_t(target >> 24);
			ram[address + 1] = uint8_t(target >> 16);
			ram[address + 2] = uint8_t(target >> 8);
			ram[address + 3] = uint8_t(target >> 0);

			store.flag(address+0, participant);
			store.flag(address+1, participant);
			store.flag(address+2, participant);
			store.flag(address+3, participant);
		}
	}

	RandomStore &store;
	const uint8_t participant;
};

using OldProcessor = CPU::MC68000::Processor<BusHandler, true, true>;
using NewProcessor = CPU::MC68000Mk2::Processor<BusHandler, true, true, true>;

template <typename M68000> struct Tester {
	Tester(RandomStore &store, uint8_t participant) : bus_handler(store, participant), processor(bus_handler) {}

	void run_instructions(int instructions) {
		bus_handler.instructions = instructions;

		try {
			processor.run_for(HalfCycles(5000));	// Arbitrary, but will definitely exceed any one instruction (by quite a distance).
		} catch (const HarmlessStopException &) {}
	}

	void reset_with_opcode(uint16_t opcode) {
		bus_handler.transactions.clear();
		bus_handler.set_default_vectors();

		const uint32_t address = 3 << 10;
		bus_handler.ram[address + 0] = uint8_t(opcode >> 8);
		bus_handler.ram[address + 1] = uint8_t(opcode >> 0);
		bus_handler.store.flag(address, bus_handler.participant);
		bus_handler.store.flag(address+1, bus_handler.participant);

		bus_handler.transaction_delay = 12;	// i.e. ignore everything from the RESET sequence.
		bus_handler.time = HalfCycles(0);

		processor.reset();
	}

	BusHandler bus_handler;
	M68000 processor;
};

}

@interface M68000OldVsNewTests : XCTestCase
@end

@implementation M68000OldVsNewTests

- (void)testOldVsNew {
	RandomStore random_store;
	auto oldTester = std::make_unique<Tester<OldProcessor>>(random_store, 0x01);
	auto newTester = std::make_unique<Tester<NewProcessor>>(random_store, 0x02);
	InstructionSet::M68k::Predecoder<InstructionSet::M68k::Model::M68000> decoder;

	// Use a fixed seed to guarantee continuity across repeated runs.
	srand(68000);

	std::set<InstructionSet::M68k::Operation> test_set = {
//		InstructionSet::M68k::Operation::ABCD,			// Old implementation doesn't match flamewing tests, sometimes produces incorrect results.
//		InstructionSet::M68k::Operation::SBCD,			// Old implementation doesn't match flamewing tests, sometimes produces incorrect results.
//		InstructionSet::M68k::Operation::MOVEb,
//		InstructionSet::M68k::Operation::MOVEw,
//		InstructionSet::M68k::Operation::MOVEl,
//		InstructionSet::M68k::Operation::PEA,
//		InstructionSet::M68k::Operation::MOVEtoSR,		// Old implementation doesn't repeat a PC fetch.
//		InstructionSet::M68k::Operation::MOVEtoCCR,		// Old implementation doesn't repeat a PC fetch.
//		InstructionSet::M68k::Operation::CMPAl,			// Old implementation omits an idle cycle before -(An)
//		InstructionSet::M68k::Operation::JSR,			// Old implementation ends up skipping stack space if the destination throws an address error.
//		InstructionSet::M68k::Operation::CLRb,			// Old implementation omits an idle cycle before -(An)
//		InstructionSet::M68k::Operation::CLRw,			// Old implementation omits an idle cycle before -(An)
//		InstructionSet::M68k::Operation::NEGXb,			// Old implementation omits an idle cycle before -(An)
//		InstructionSet::M68k::Operation::NEGXw,			// Old implementation omits an idle cycle before -(An)
//		InstructionSet::M68k::Operation::NEGb,			// Old implementation omits an idle cycle before -(An)
//		InstructionSet::M68k::Operation::NEGw,			// Old implementation omits an idle cycle before -(An)
//		InstructionSet::M68k::Operation::NOTb,			// Old implementation omits an idle cycle before -(An)
//		InstructionSet::M68k::Operation::NOTw,			// Old implementation omits an idle cycle before -(An)
//		InstructionSet::M68k::Operation::MULU,
//		InstructionSet::M68k::Operation::MULS,
//		InstructionSet::M68k::Operation::DIVU,
//		InstructionSet::M68k::Operation::DIVS,
//		InstructionSet::M68k::Operation::TRAP,			// Old implementation relocates the idle state near the end to the beginning.
//		InstructionSet::M68k::Operation::TRAPV,			// Old implementation relocates the idle state near the end to the beginning.
//		InstructionSet::M68k::Operation::CHK,			// Old implementation pauses four cycles too long.
//		InstructionSet::M68k::Operation::TAS,			// Old implementation just doesn't match published cycle counts.
	};

	std::set<InstructionSet::M68k::Operation> failing_operations;
	for(int c = 0; c < 65536; c++) {
//		printf("%04x\n", c);

		// Test only defined opcodes that aren't STOP (which will never teminate).
		const auto instruction = decoder.decode(uint16_t(c));
		if(
			instruction.operation == InstructionSet::M68k::Operation::Undefined ||
			instruction.operation == InstructionSet::M68k::Operation::STOP
		) {
			continue;
		}

		// If a whitelist is in place, adhere to it.
		if(!test_set.empty() && test_set.find(instruction.operation) == test_set.end()) {
			continue;
		}

		// Test each 1000 times.
		for(int test = 0; test < 100; test++) {
			// Establish with certainty the initial memory state.
			random_store.clear();
			newTester->reset_with_opcode(c);
			oldTester->reset_with_opcode(c);

			// Generate a random initial register state.
			auto oldState = oldTester->processor.get_state();
			auto newState = newTester->processor.get_state();

			for(int c = 0; c < 8; c++) {
				oldState.data[c] = newState.registers.data[c] = rand() ^ (rand() << 1);
				if(c != 7) oldState.address[c] = newState.registers.address[c] = rand() << 1;
			}
			// Fully to paper over the two 68000s' different ways of doing a faked
			// reset, pick a random status such that:
			//
			//	(i) supervisor mode is active;
			//	(ii) trace is inactive; and
			//	(iii) interrupt level is 7.
			oldState.status = newState.registers.status = (rand() | (1 << 13) | (7 << 8)) & ~(1 << 15);
			oldState.user_stack_pointer = newState.registers.user_stack_pointer = rand() << 1;
			oldState.supervisor_stack_pointer = newState.registers.supervisor_stack_pointer = 0x800;

			newTester->processor.set_state(newState);
			oldTester->processor.set_state(oldState);

			// Run a single instruction.
			newTester->run_instructions(1);
			oldTester->run_instructions(1);

			// Grab final states.
			oldState = oldTester->processor.get_state();
			newState = newTester->processor.get_state();

			// Compare bus activity only if it doesn't look like an address
			// error occurred. Don't check those as the old 68000 appears to be wrong
			// most of the time about function codes, and that bleeds into the stacked data.
			//
			// Net effect will be 50% fewer transaction comparisons for instructions that
			// can trigger an address error.
			const auto &oldTransactions = oldTester->bus_handler.transactions;
			const auto &newTransactions = newTester->bus_handler.transactions;
			if(oldState.program_counter != 0x1404 || newState.registers.program_counter != 0x1404) {
				auto newIt = newTransactions.begin();
				auto oldIt = oldTransactions.begin();
				while(newIt != newTransactions.end() && oldIt != oldTransactions.end()) {
					if(*newIt != *oldIt) {
						printf("Mismatch in %s, test %d:\n", instruction.to_string().c_str(), test);

						auto repeatIt = newTransactions.begin();
						while(repeatIt != newIt) {
							repeatIt->print();
							++repeatIt;
						}
						printf("---\n");
						while(newIt != newTransactions.end()) {
							printf("n: "); newIt->print();
							++newIt;
						}
						printf("\n");
						while(oldIt != oldTransactions.end()) {
							printf("o: "); oldIt->print();
							++oldIt;
						}
						printf("\n");

						failing_operations.insert(instruction.operation);
						break;
					}

					++newIt;
					++oldIt;
				}
			}

			// Compare registers.
			bool mismatch = false;
			for(int c = 0; c < 8; c++) {
				mismatch |= oldState.data[c] != newState.registers.data[c];
				if(c != 7) mismatch |= oldState.address[c] != newState.registers.address[c];
			}
			mismatch |= oldState.status != newState.registers.status;
			mismatch |= oldState.program_counter != newState.registers.program_counter;
			mismatch |= oldState.user_stack_pointer != newState.registers.user_stack_pointer;
			mismatch |= oldState.supervisor_stack_pointer != newState.registers.supervisor_stack_pointer;

			if(mismatch) {
				failing_operations.insert(instruction.operation);
				printf("Registers don't match after %s, test %d\n", instruction.to_string().c_str(), test);
				for(const auto &transaction: newTransactions) {
					printf("n: "); transaction.print();
				}
				printf("\n");
				for(const auto &transaction: oldTransactions) {
					printf("o: "); transaction.print();
				}
				printf("\n");

				// TODO: more detail here!
			}
		}
	}

	printf("\nAll failing operations:\n");
	for(const auto operation: failing_operations) {
		printf("%d,\n", int(operation));
	}
}

@end
