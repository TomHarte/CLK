//
//  PCCompatible.cpp
//  Clock Signal
//
//  Created by Thomas Harte on 15/11/2023.
//  Copyright © 2023 Thomas Harte. All rights reserved.
//

#include "PCCompatible.hpp"

#include "../../InstructionSets/x86/Decoder.hpp"
#include "../../InstructionSets/x86/Flags.hpp"
#include "../../InstructionSets/x86/Instruction.hpp"
#include "../../InstructionSets/x86/Perform.hpp"

#include "../ScanProducer.hpp"
#include "../TimedMachine.hpp"

#include <array>

namespace PCCompatible {

template <bool is_8254>
class PIT {
	public:
		template <int channel> uint8_t read() {
			const auto result = channels_[channel].read();
			printf("Read from %d; %02x\n", channel, result);
			return result;
		}

		template <int channel> void write(uint8_t value) {
			printf("Write to %d\n", channel);
			channels_[channel].write(value);
		}

		void set_mode(uint8_t value) {
			const int channel_id = (value >> 6) & 3;
			if(channel_id == 3) {
				read_back_ = is_8254;

				// TODO: decode rest of read-back command.
				return;
			}

			printf("Set mode on %d\n", channel_id);

			Channel &channel = channels_[channel_id];
			switch((value >> 1) & 3) {
				default:
					channel.latch_value();
				return;

				case 1:		channel.latch_mode = LatchMode::LowOnly;	break;
				case 2:		channel.latch_mode = LatchMode::HighOnly;	break;
				case 3:		channel.latch_mode = LatchMode::LowHigh;	break;
			}
			channel.is_bcd = value & 1;
			channel.next_write_high = false;

			const auto operating_mode = (value >> 3) & 7;
			switch(operating_mode) {
				default:	channel.mode = OperatingMode(operating_mode);		break;
				case 6:		channel.mode = OperatingMode::RateGenerator;		break;
				case 7:		channel.mode = OperatingMode::SquareWaveGenerator;	break;
			}

			// Set up operating mode.
			switch(channel.mode) {
				default:
					printf("%d switches to unimplemented mode %d\n", channel_id, int(channel.mode));
				break;

				case OperatingMode::InterruptOnTerminalCount:
					channel.output = false;
					channel.awaiting_reload = true;
				break;

				case OperatingMode::RateGenerator:
					channel.output = true;
					channel.awaiting_reload = true;
				break;
			}
		}

		void run_for(Cycles cycles) {
			// TODO: be intelligent enough to take ticks outside the loop when appropriate.
			auto ticks = cycles.as<int>();
			while(ticks--) {
				bool output_changed;
				output_changed = channels_[0].advance(1);
				output_changed |= channels_[1].advance(1);
				output_changed |= channels_[2].advance(1);
			}
		}

	private:
		// Supported only on 8254s.
		bool read_back_ = false;

		enum class LatchMode {
			LowOnly,
			HighOnly,
			LowHigh,
		};

		enum class OperatingMode {
			InterruptOnTerminalCount		= 0,
			HardwareRetriggerableOneShot	= 1,
			RateGenerator					= 2,
			SquareWaveGenerator				= 3,
			SoftwareTriggeredStrobe			= 4,
			HardwareTriggeredStrobe			= 5,
		};

		struct Channel {
			LatchMode latch_mode = LatchMode::LowHigh;
			OperatingMode mode = OperatingMode::InterruptOnTerminalCount;
			bool is_bcd = false;

			bool gated = false;
			bool awaiting_reload = true;

			uint16_t counter = 0;
			uint16_t reload = 0;
			uint16_t latch = 0;
			bool output = false;

			bool next_write_high = false;

			void latch_value() {
				latch = counter;
			}

			bool advance(int ticks) {
				if(gated || awaiting_reload) return false;

				// TODO: BCD mode is completely ignored below. Possibly not too important.
				const bool initial_output = output;
				switch(mode) {
					case OperatingMode::InterruptOnTerminalCount:
						// Output goes permanently high upon a tick from 1 to 0; reload value is not used on wraparound.
						output |= counter <= ticks;
						counter -= ticks;
					break;

					case OperatingMode::RateGenerator:
						// Output goes low upon a tick from 2 to 1. It goes high again on 1 to 0, and the reload value is used.
						if(counter <= ticks) {
							counter = reload - ticks + counter;
						} else {
							counter -= ticks;
						}
						output = counter != 1;
					break;

					default:
						// TODO.
						break;
				}

				return output != initial_output;
			}

			void write(uint8_t value) {
				switch(latch_mode) {
					case LatchMode::LowOnly:
						reload = (reload & 0xff00) | value;
					break;
					case LatchMode::HighOnly:
						reload = (reload & 0x00ff) | (value << 8);
					break;
					case LatchMode::LowHigh:
						if(!next_write_high) {
							reload = (reload & 0xff00) | value;
							next_write_high = true;
							return;
						}

						reload = (reload & 0x00ff) | (value << 8);
						next_write_high = false;
					break;
				}

				awaiting_reload = false;

				switch(mode) {
					case OperatingMode::InterruptOnTerminalCount:
					case OperatingMode::RateGenerator:
						counter = reload;
					break;
				}
			}

			uint8_t read() {
				switch(latch_mode) {
					case LatchMode::LowOnly:	return uint8_t(latch);
					case LatchMode::HighOnly:	return uint8_t(latch >> 8);
					default:
					case LatchMode::LowHigh:
						next_write_high ^= true;
						return next_write_high ? uint8_t(latch) : uint8_t(latch >> 8);
					break;
				}
			}
		} channels_[3];

		// TODO:
		//
		//	channel 0 is connected to IRQ 0;
		//	channel 1 is used for DRAM refresh;
		//	channel 2 is gated by a PPI output and feeds into the speaker.
		//
		//	RateGenerator: output goes high if gated.
};

struct Registers {
	public:
		static constexpr bool is_32bit = false;

		uint8_t &al()	{	return ax_.halves.low;	}
		uint8_t &ah()	{	return ax_.halves.high;	}
		uint16_t &ax()	{	return ax_.full;		}

		CPU::RegisterPair16 &axp()	{	return ax_;	}

		uint8_t &cl()	{	return cx_.halves.low;	}
		uint8_t &ch()	{	return cx_.halves.high;	}
		uint16_t &cx()	{	return cx_.full;		}

		uint8_t &dl()	{	return dx_.halves.low;	}
		uint8_t &dh()	{	return dx_.halves.high;	}
		uint16_t &dx()	{	return dx_.full;		}

		uint8_t &bl()	{	return bx_.halves.low;	}
		uint8_t &bh()	{	return bx_.halves.high;	}
		uint16_t &bx()	{	return bx_.full;		}

		uint16_t &sp()	{	return sp_;				}
		uint16_t &bp()	{	return bp_;				}
		uint16_t &si()	{	return si_;				}
		uint16_t &di()	{	return di_;				}

		uint16_t &ip()	{	return ip_;				}

		uint16_t &es()		{	return es_;			}
		uint16_t &cs()		{	return cs_;			}
		uint16_t &ds()		{	return ds_;			}
		uint16_t &ss()		{	return ss_;			}
		uint16_t es() const	{	return es_;			}
		uint16_t cs() const	{	return cs_;			}
		uint16_t ds() const	{	return ds_;			}
		uint16_t ss() const	{	return ss_;			}

		void reset() {
			cs_ = 0xffff;
			ip_ = 0;
		}

	private:
		CPU::RegisterPair16 ax_;
		CPU::RegisterPair16 cx_;
		CPU::RegisterPair16 dx_;
		CPU::RegisterPair16 bx_;

		uint16_t sp_;
		uint16_t bp_;
		uint16_t si_;
		uint16_t di_;
		uint16_t es_, cs_, ds_, ss_;
		uint16_t ip_;
};

class Segments {
	public:
		Segments(const Registers &registers) : registers_(registers) {}

		using Source = InstructionSet::x86::Source;

		/// Posted by @c perform after any operation which *might* have affected a segment register.
		void did_update(Source segment) {
			switch(segment) {
				default: break;
				case Source::ES:	es_base_ = uint32_t(registers_.es()) << 4;	break;
				case Source::CS:	cs_base_ = uint32_t(registers_.cs()) << 4;	break;
				case Source::DS:	ds_base_ = uint32_t(registers_.ds()) << 4;	break;
				case Source::SS:	ss_base_ = uint32_t(registers_.ss()) << 4;	break;
			}
		}

		void reset() {
			did_update(Source::ES);
			did_update(Source::CS);
			did_update(Source::DS);
			did_update(Source::SS);
		}

		uint32_t es_base_, cs_base_, ds_base_, ss_base_;

		bool operator ==(const Segments &rhs) const {
			return
				es_base_ == rhs.es_base_ &&
				cs_base_ == rhs.cs_base_ &&
				ds_base_ == rhs.ds_base_ &&
				ss_base_ == rhs.ss_base_;
		}

	private:
		const Registers &registers_;
};

// TODO: send writes to the ROM area off to nowhere.
struct Memory {
	public:
		using AccessType = InstructionSet::x86::AccessType;

		// Constructor.
		Memory(Registers &registers, const Segments &segments) : registers_(registers), segments_(segments) {}

		//
		// Preauthorisation call-ins. Since only an 8088 is currently modelled, all accesses are implicitly authorised.
		//
		void preauthorise_stack_write([[maybe_unused]] uint32_t length) {}
		void preauthorise_stack_read([[maybe_unused]] uint32_t length) {}
		void preauthorise_read([[maybe_unused]] InstructionSet::x86::Source segment, [[maybe_unused]] uint16_t start, [[maybe_unused]] uint32_t length) {}
		void preauthorise_read([[maybe_unused]] uint32_t start, [[maybe_unused]] uint32_t length) {}

		//
		// Access call-ins.
		//

		// Accesses an address based on segment:offset.
		template <typename IntT, AccessType type>
		typename InstructionSet::x86::Accessor<IntT, type>::type access(InstructionSet::x86::Source segment, uint16_t offset) {
			const uint32_t physical_address = address(segment, offset);

			if constexpr (std::is_same_v<IntT, uint16_t>) {
				// If this is a 16-bit access that runs past the end of the segment, it'll wrap back
				// to the start. So the 16-bit value will need to be a local cache.
				if(offset == 0xffff) {
					return split_word<type>(physical_address, address(segment, 0));
				}
			}

			return access<IntT, type>(physical_address);
		}

		// Accesses an address based on physical location.
		template <typename IntT, AccessType type>
		typename InstructionSet::x86::Accessor<IntT, type>::type access(uint32_t address) {
			// Dispense with the single-byte case trivially.
			if constexpr (std::is_same_v<IntT, uint8_t>) {
				return memory[address];
			} else if(address != 0xf'ffff) {
				return *reinterpret_cast<IntT *>(&memory[address]);
			} else {
				return split_word<type>(address, 0);
			}
		}

		template <typename IntT>
		void write_back() {
			if constexpr (std::is_same_v<IntT, uint16_t>) {
				if(write_back_address_[0] != NoWriteBack) {
					memory[write_back_address_[0]] = write_back_value_ & 0xff;
					memory[write_back_address_[1]] = write_back_value_ >> 8;
					write_back_address_[0]  = 0;
				}
			}
		}

		//
		// Direct write.
		//
		template <typename IntT>
		void preauthorised_write(InstructionSet::x86::Source segment, uint16_t offset, IntT value) {
			// Bytes can be written without further ado.
			if constexpr (std::is_same_v<IntT, uint8_t>) {
				memory[address(segment, offset) & 0xf'ffff] = value;
				return;
			}

			// Words that straddle the segment end must be split in two.
			if(offset == 0xffff) {
				memory[address(segment, offset) & 0xf'ffff] = value & 0xff;
				memory[address(segment, 0x0000) & 0xf'ffff] = value >> 8;
				return;
			}

			const uint32_t target = address(segment, offset) & 0xf'ffff;

			// Words that straddle the end of physical RAM must also be split in two.
			if(target == 0xf'ffff) {
				memory[0xf'ffff] = value & 0xff;
				memory[0x0'0000] = value >> 8;
				return;
			}

			// It's safe just to write then.
			*reinterpret_cast<uint16_t *>(&memory[target]) = value;
		}

		//
		// Helper for instruction fetch.
		//
		std::pair<const uint8_t *, size_t> next_code() {
			const uint32_t start = segments_.cs_base_ + registers_.ip();
			return std::make_pair(&memory[start], 0x10'000 - start);
		}

		std::pair<const uint8_t *, size_t> all() {
			return std::make_pair(memory.data(), 0x10'000);
		}

		//
		// Population.
		//
		void install(size_t address, const uint8_t *data, size_t length) {
			std::copy(data, data + length, memory.begin() + std::vector<uint8_t>::difference_type(address));
		}

	private:
		std::array<uint8_t, 1024*1024> memory{0xff};
		Registers &registers_;
		const Segments &segments_;

		uint32_t segment_base(InstructionSet::x86::Source segment) {
			using Source = InstructionSet::x86::Source;
			switch(segment) {
				default:			return segments_.ds_base_;
				case Source::ES:	return segments_.es_base_;
				case Source::CS:	return segments_.cs_base_;
				case Source::SS:	return segments_.ss_base_;
			}
		}

		uint32_t address(InstructionSet::x86::Source segment, uint16_t offset) {
			return (segment_base(segment) + offset) & 0xf'ffff;
		}

		template <AccessType type>
		typename InstructionSet::x86::Accessor<uint16_t, type>::type
		split_word(uint32_t low_address, uint32_t high_address) {
			if constexpr (is_writeable(type)) {
				write_back_address_[0] = low_address;
				write_back_address_[1] = high_address;

				// Prepopulate only if this is a modify.
				if constexpr (type == AccessType::ReadModifyWrite) {
					write_back_value_ = uint16_t(memory[write_back_address_[0]] | (memory[write_back_address_[1]] << 8));
				}

				return write_back_value_;
			} else {
				return uint16_t(memory[low_address] | (memory[high_address] << 8));
			}
		}

		static constexpr uint32_t NoWriteBack = 0;	// A low byte address of 0 can't require write-back.
		uint32_t write_back_address_[2] = {NoWriteBack, NoWriteBack};
		uint16_t write_back_value_;
};

class IO {
	public:
		IO(PIT<false> &pit) : pit_(pit) {}

		template <typename IntT> void out([[maybe_unused]] uint16_t port, [[maybe_unused]] IntT value) {
			switch(port) {
				default:
					if constexpr (std::is_same_v<IntT, uint8_t>) {
						printf("Unhandled out: %02x to %04x\n", value, port);
					} else {
						printf("Unhandled out: %04x to %04x\n", value, port);
					}
				break;

				// On the XT the NMI can be masked by setting bit 7 on I/O port 0xA0.
				case 0x00a0:
					printf("TODO: NMIs %s\n", (value & 0x80) ? "masked" : "unmasked");
				break;

				case 0x0000:	case 0x0001:	case 0x0002:	case 0x0003:
				case 0x0004:	case 0x0005:	case 0x0006:	case 0x0007:
				case 0x0008:	case 0x0009:	case 0x000a:	case 0x000b:
				case 0x000c:	case 0x000d:	case 0x000e:	case 0x000f:
					printf("TODO: DMA write of %02x at %04x\n", value, port);
				break;

				case 0x0060:	case 0x0061:	case 0x0062:	case 0x0063:
				case 0x0064:	case 0x0065:	case 0x0066:	case 0x0067:
				case 0x0068:	case 0x0069:	case 0x006a:	case 0x006b:
				case 0x006c:	case 0x006d:	case 0x006e:	case 0x006f:
					// Likely to be helpful: https://github.com/tmk/tmk_keyboard/wiki/IBM-PC-XT-Keyboard-Protocol
					printf("TODO: PPI write of %02x at %04x\n", value, port);
				break;

				case 0x0080:	case 0x0081:	case 0x0082:	case 0x0083:
				case 0x0084:	case 0x0085:	case 0x0086:	case 0x0087:
				case 0x0088:	case 0x0089:	case 0x008a:	case 0x008b:
				case 0x008c:	case 0x008d:	case 0x008e:	case 0x008f:
					printf("TODO: DMA page write of %02x at %04x\n", value, port);
				break;

				case 0x03b0:	case 0x03b1:	case 0x03b2:	case 0x03b3:
				case 0x03b4:	case 0x03b5:	case 0x03b6:	case 0x03b7:
				case 0x03b8:	case 0x03b9:	case 0x03ba:	case 0x03bb:
				case 0x03bc:	case 0x03bd:	case 0x03be:	case 0x03bf:
					printf("TODO: MDA write of %02x at %04x\n", value, port);
				break;

				case 0x03d0:	case 0x03d1:	case 0x03d2:	case 0x03d3:
				case 0x03d4:	case 0x03d5:	case 0x03d6:	case 0x03d7:
				case 0x03d8:	case 0x03d9:	case 0x03da:	case 0x03db:
				case 0x03dc:	case 0x03dd:	case 0x03de:	case 0x03df:
					printf("TODO: CGA write of %02x at %04x\n", value, port);
				break;

				case 0x0040:	pit_.write<0>(uint8_t(value));	break;
				case 0x0041:	pit_.write<1>(uint8_t(value));	break;
				case 0x0042:	pit_.write<2>(uint8_t(value));	break;
				case 0x0043:	pit_.set_mode(uint8_t(value));	break;
			}
		}
		template <typename IntT> IntT in([[maybe_unused]] uint16_t port) {
			switch(port) {
				default:
					printf("Unhandled in: %04x\n", port);
				break;

				case 0x0040:	return pit_.read<0>();
				case 0x0041:	return pit_.read<1>();
				case 0x0042:	return pit_.read<2>();

				case 0x0060:	case 0x0061:	case 0x0062:	case 0x0063:
				case 0x0064:	case 0x0065:	case 0x0066:	case 0x0067:
				case 0x0068:	case 0x0069:	case 0x006a:	case 0x006b:
				case 0x006c:	case 0x006d:	case 0x006e:	case 0x006f:
					printf("TODO: PPI read from %04x\n", port);
				break;
			}
			return IntT(~0);
		}

	private:
		PIT<false> &pit_;
};

class FlowController {
	public:
		FlowController(Registers &registers, Segments &segments) :
			registers_(registers), segments_(segments) {}

		// Requirements for perform.
		void jump(uint16_t address) {
			registers_.ip() = address;
		}

		void jump(uint16_t segment, uint16_t address) {
			registers_.cs() = segment;
			segments_.did_update(Segments::Source::CS);
			registers_.ip() = address;
		}

		void halt() {}
		void wait() {}

		void repeat_last() {
			should_repeat_ = true;
		}

		// Other actions.
		void begin_instruction() {
			should_repeat_ = false;
		}
		bool should_repeat() const {
			return should_repeat_;
		}

	private:
		Registers &registers_;
		Segments &segments_;
		bool should_repeat_ = false;
};

class ConcreteMachine:
	public Machine,
	public MachineTypes::TimedMachine,
	public MachineTypes::ScanProducer
{
	public:
		static constexpr int PitMultiplier = 1;
		static constexpr int PitDivisor = 3;

		ConcreteMachine(
			[[maybe_unused]] const Analyser::Static::Target &target,
			const ROMMachine::ROMFetcher &rom_fetcher
		) : context(pit_) {
			// Use clock rate as a MIPS count; keeping it as a multiple or divisor of the PIT frequency is easy.
			static constexpr int pit_frequency = 1'193'182;
			set_clock_rate(double(pit_frequency) * double(PitMultiplier) / double(PitDivisor));	// i.e. almost 0.4 MIPS for an XT.

			// Fetch the BIOS. [8088 only, for now]
			const auto bios = ROM::Name::PCCompatibleGLaBIOS;

			ROM::Request request = ROM::Request(bios);
			auto roms = rom_fetcher(request);
			if(!request.validate(roms)) {
				throw ROMMachine::Error::MissingROMs;
			}

			const auto &bios_contents = roms.find(bios)->second;
			context.memory.install(0x10'0000 - bios_contents.size(), bios_contents.data(), bios_contents.size());
		}

		// MARK: - TimedMachine.
		void run_for(const Cycles cycles) override {
			auto instructions = cycles.as_integral();
			while(instructions--) {
				// First draft: all hardware runs in lockstep.
				pit_.run_for(PitDivisor / PitMultiplier);

				// Get the next thing to execute into decoded.
				if(!context.flow_controller.should_repeat()) {
					// Decode from the current IP.
					const auto remainder = context.memory.next_code();
					decoded = decoder.decode(remainder.first, remainder.second);

					// If that didn't yield a whole instruction then the end of memory must have been hit;
					// continue from the beginning.
					if(decoded.first <= 0) {
						const auto all = context.memory.all();
						decoded = decoder.decode(all.first, all.second);
					}

					context.registers.ip() += decoded.first;
				} else {
					context.flow_controller.begin_instruction();
				}

				// Execute it.
				InstructionSet::x86::perform(
					decoded.second,
					context
				);
			}
		}

		// MARK: - ScanProducer.
		void set_scan_target([[maybe_unused]] Outputs::Display::ScanTarget *scan_target) override {}
		Outputs::Display::ScanStatus get_scaled_scan_status() const override {
			return Outputs::Display::ScanStatus();
		}

	private:
		PIT<false> pit_;

		struct Context {
			Context(PIT<false> &pit) :
				segments(registers),
				memory(registers, segments),
				flow_controller(registers, segments),
				io(pit)
			{
				reset();
			}

			void reset() {
				registers.reset();
				segments.reset();
			}

			InstructionSet::x86::Flags flags;
			Registers registers;
			Segments segments;
			Memory memory;
			FlowController flow_controller;
			IO io;
			static constexpr auto model = InstructionSet::x86::Model::i8086;
		} context;

		// TODO: eliminate use of Decoder8086 and Decoder8086 in gneral in favour of the templated version, as soon
		// as whatever error is preventing GCC from picking up Decoder's explicit instantiations becomes apparent.
		InstructionSet::x86::Decoder8086 decoder;
//		InstructionSet::x86::Decoder<InstructionSet::x86::Model::i8086> decoder;

		std::pair<int, InstructionSet::x86::Instruction<false>> decoded;
};


}

using namespace PCCompatible;

// See header; constructs and returns an instance of the Amstrad CPC.
Machine *Machine::PCCompatible(const Analyser::Static::Target *target, const ROMMachine::ROMFetcher &rom_fetcher) {
	return new PCCompatible::ConcreteMachine(*target, rom_fetcher);
}

Machine::~Machine() {}
