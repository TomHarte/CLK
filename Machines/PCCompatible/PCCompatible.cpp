//
//  PCCompatible.cpp
//  Clock Signal
//
//  Created by Thomas Harte on 15/11/2023.
//  Copyright © 2023 Thomas Harte. All rights reserved.
//

#include "PCCompatible.hpp"

#include "DMA.hpp"
#include "PIC.hpp"
#include "PIT.hpp"

#include "../../InstructionSets/x86/Decoder.hpp"
#include "../../InstructionSets/x86/Flags.hpp"
#include "../../InstructionSets/x86/Instruction.hpp"
#include "../../InstructionSets/x86/Perform.hpp"

#include "../../Components/AudioToggle/AudioToggle.hpp"
#include "../../Components/8255/i8255.hpp"

#include "../../Numeric/RegisterSizes.hpp"
#include "../../Outputs/Speaker/Implementation/LowpassSpeaker.hpp"

#include "../AudioProducer.hpp"
#include "../ScanProducer.hpp"
#include "../TimedMachine.hpp"

#include <array>
#include <iostream>

namespace PCCompatible {

class PITObserver {
	public:
		PITObserver(PIC &pic) : pic_(pic) {}

		template <int channel>
		void update_output(bool new_level) {
			switch(channel) {
				default: break;
				case 0: pic_.apply_edge<0>(new_level);	break;
			}
		}

	private:
		PIC &pic_;

	// TODO:
	//
	//	channel 0 is connected to IRQ 0;
	//	channel 1 is used for DRAM refresh (presumably connected to DMA?);
	//	channel 2 is gated by a PPI output and feeds into the speaker.
};
using PIT = i8237<false, PITObserver>;

class i8255PortHandler : public Intel::i8255::PortHandler {
	// Likely to be helpful: https://github.com/tmk/tmk_keyboard/wiki/IBM-PC-XT-Keyboard-Protocol
	public:
		void set_value(int port, uint8_t value) {
			switch(port) {
				case 1:
					high_switches_ = value & 0x08;
				break;
			}
			printf("PPI: %02x to %d\n", value, port);
		}

		uint8_t get_value(int port) {
			switch(port) {
				case 2:
					// Common:
					//
					// b7: 1 => memory parity error; 0 => none;
					// b6: 1 => IO channel error; 0 => none;
					// b5: timer 2 output;	[TODO]
					// b4: cassette data input; [TODO]
					return
						high_switches_ ?
							// b3, b2: drive count; 00 = 1, 01 = 2, etc
							// b1, b0: video mode (00 = ROM; 01 = CGA40; 10 = CGA80; 11 = MDA)
							0b0000'0011
						:
							// b3, b2: RAM on motherboard (64 * bit pattern)
							// b1: 1 => FPU present; 0 => absent;
							// b0: 1 => floppy drive present; 0 => absent.
							0b0000'1100;
			}
			printf("PPI: from %d\n", port);
			return 0;
		};

	private:
		bool high_switches_ = false;

	// Provisionally, possibly:
	//
	//	port 0 = keyboard data output buffer;
	//
};
using PPI = Intel::i8255::i8255<i8255PortHandler>;

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
//		int mda_delay = -1;	// HACK.
		template <typename IntT, AccessType type>
		typename InstructionSet::x86::Accessor<IntT, type>::type access(uint32_t address) {

			// TEMPORARY HACK.
//			if(mda_delay > 0) {
//				--mda_delay;
//				if(!mda_delay) {
//					print_mda();
//				}
//			}
//			if(address >= 0xb'0000 && is_writeable(type)) {
//				mda_delay = 100;
//			}

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


		// TEMPORARY HACK.
//		void print_mda() {
//			uint32_t pointer = 0xb'0000;
//			for(int y = 0; y < 25; y++) {
//				for(int x = 0; x < 80; x++) {
//					printf("%c", memory[pointer]);
//					pointer += 2;	// MDA goes [character, attributes]...; skip the attributes.
//				}
//				printf("\n");
//			}
//		}

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
		IO(PIT &pit, DMA &dma, PPI &ppi, PIC &pic) : pit_(pit), dma_(dma), ppi_(ppi), pic_(pic) {}

		template <typename IntT> void out(uint16_t port, IntT value) {
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

				case 0x0000:	dma_.write<0>(value);	break;
				case 0x0001:	dma_.write<1>(value);	break;
				case 0x0002:	dma_.write<2>(value);	break;
				case 0x0003:	dma_.write<3>(value);	break;
				case 0x0004:	dma_.write<4>(value);	break;
				case 0x0005:	dma_.write<5>(value);	break;
				case 0x0006:	dma_.write<6>(value);	break;
				case 0x0007:	dma_.write<7>(value);	break;

				case 0x0008:	case 0x0009:	case 0x000a:	case 0x000b:
				case 0x000c:	case 0x000f:
					printf("TODO: DMA write of %02x at %04x\n", value, port);
				break;

				case 0x000d:	dma_.master_reset();	break;
				case 0x000e:	dma_.mask_reset();		break;

				case 0x0020:	pic_.write<0>(value);	break;
				case 0x0021:	pic_.write<1>(value);	break;

				case 0x0060:	case 0x0061:	case 0x0062:	case 0x0063:
				case 0x0064:	case 0x0065:	case 0x0066:	case 0x0067:
				case 0x0068:	case 0x0069:	case 0x006a:	case 0x006b:
				case 0x006c:	case 0x006d:	case 0x006e:	case 0x006f:
					ppi_.write(port, value);
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

				case 0x0000:	return dma_.read<0>();
				case 0x0001:	return dma_.read<1>();
				case 0x0002:	return dma_.read<2>();
				case 0x0003:	return dma_.read<3>();
				case 0x0004:	return dma_.read<4>();
				case 0x0005:	return dma_.read<5>();
				case 0x0006:	return dma_.read<6>();
				case 0x0007:	return dma_.read<7>();

				case 0x0020:	return pic_.read<0>();
				case 0x0021:	return pic_.read<1>();

				case 0x0040:	return pit_.read<0>();
				case 0x0041:	return pit_.read<1>();
				case 0x0042:	return pit_.read<2>();

				case 0x0060:	case 0x0061:	case 0x0062:	case 0x0063:
				case 0x0064:	case 0x0065:	case 0x0066:	case 0x0067:
				case 0x0068:	case 0x0069:	case 0x006a:	case 0x006b:
				case 0x006c:	case 0x006d:	case 0x006e:	case 0x006f:
				return ppi_.read(port);
			}
			return IntT(~0);
		}

	private:
		PIT &pit_;
		DMA &dma_;
		PPI &ppi_;
		PIC &pic_;
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
	public MachineTypes::AudioProducer,
	public MachineTypes::ScanProducer
{
	public:
		// i.e. CPU clock rate is 1/3 * ~1.19Mhz ~= 0.4 MIPS.
		static constexpr int CPUMultiplier = 1;
		static constexpr int CPUDivisor = 3;

		ConcreteMachine(
			[[maybe_unused]] const Analyser::Static::Target &target,
			const ROMMachine::ROMFetcher &rom_fetcher
		) :
			pit_observer_(pic_),
			pit_(pit_observer_),
			ppi_(ppi_handler_),
			context(pit_, dma_, ppi_, pic_),
			audio_toggle_(audio_queue_),
			speaker_(audio_toggle_)
		{
			// Use clock rate as a MIPS count; keeping it as a multiple or divisor of the PIT frequency is easy.
			static constexpr int pit_frequency = 1'193'182;
			set_clock_rate(double(pit_frequency));
			speaker_.set_input_rate(double(pit_frequency));

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

		~ConcreteMachine() {
			audio_queue_.flush();
		}

		// MARK: - TimedMachine.
//		bool log = false;
//		std::string previous;
		void run_for(const Cycles duration) override {
			auto pit_ticks = duration.as_integral();
			while(pit_ticks--) {
				//
				// First draft: all hardware runs in lockstep, as a multiple or divisor of the PIT frequency.
				//

				// Advance the PIT.
				pit_.run_for(1);

				// Advance audio.
				++cycles_since_audio_update_;

				// Advance the CPU.
				cpu_divisor_ += CPUMultiplier;
				int cycles = cpu_divisor_ / CPUDivisor;
				cycles %= CPUDivisor;

				// To consider: a Duff-esque switch table calling into a function templated on clock phase
				// might alleviate a large part of the conditionality here?

				while(cycles--) {
					// Query for interrupts and apply if pending.
					if(pic_.pending() && context.flags.flag<InstructionSet::x86::Flag::Interrupt>()) {
						// Regress the IP if a REP is in-progress so as to resume it later.
						if(context.flow_controller.should_repeat()) {
							context.registers.ip() = decoded_ip_;
							context.flow_controller.begin_instruction();
						}

						// Signal interrupt.
						InstructionSet::x86::interrupt(
							pic_.acknowledge(),
							context
						);
					}

					// Get the next thing to execute.
					if(!context.flow_controller.should_repeat()) {
						// Decode from the current IP.
						decoded_ip_ = context.registers.ip();
						const auto remainder = context.memory.next_code();
						decoded = decoder.decode(remainder.first, remainder.second);

						// If that didn't yield a whole instruction then the end of memory must have been hit;
						// continue from the beginning.
						if(decoded.first <= 0) {
							const auto all = context.memory.all();
							decoded = decoder.decode(all.first, all.second);
						}

						context.registers.ip() += decoded.first;

//						log |= decoded.second.operation() == InstructionSet::x86::Operation::STI;
					} else {
						context.flow_controller.begin_instruction();
					}

//					if(log) {
//						const auto next = to_string(decoded, InstructionSet::x86::Model::i8086);
//						if(next != previous) {
//							std::cout << next << std::endl;
//							previous = next;
//						}
//					}

					// Execute it.
					InstructionSet::x86::perform(
						decoded.second,
						context
					);
				}
			}
		}

		// MARK: - ScanProducer.
		void set_scan_target([[maybe_unused]] Outputs::Display::ScanTarget *scan_target) override {}
		Outputs::Display::ScanStatus get_scaled_scan_status() const override {
			return Outputs::Display::ScanStatus();
		}

		// MARK: - AudioProducer.
		Outputs::Speaker::Speaker *get_speaker() override {
			return &speaker_;
		}

		void flush_output(int outputs) final {
			if(outputs & Output::Audio) {
				update_audio();
				audio_queue_.perform();
			}
		}

		void update_audio() {
			speaker_.run_for(audio_queue_, cycles_since_audio_update_);
		}

	private:
		PIC pic_;
		DMA dma_;

		PITObserver pit_observer_;
		i8255PortHandler ppi_handler_;

		PIT pit_;
		PPI ppi_;

		struct Context {
			Context(PIT &pit, DMA &dma, PPI &ppi, PIC &pic) :
				segments(registers),
				memory(registers, segments),
				flow_controller(registers, segments),
				io(pit, dma, ppi, pic)
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

		uint16_t decoded_ip_ = 0;
		std::pair<int, InstructionSet::x86::Instruction<false>> decoded;

		int cpu_divisor_ = 0;

		// PC speaker.
		Concurrency::AsyncTaskQueue<false> audio_queue_;
		Audio::Toggle audio_toggle_;
		Outputs::Speaker::PullLowpass<Audio::Toggle> speaker_;
		Cycles cycles_since_audio_update_ = 0;
};


}

using namespace PCCompatible;

// See header; constructs and returns an instance of the Amstrad CPC.
Machine *Machine::PCCompatible(const Analyser::Static::Target *target, const ROMMachine::ROMFetcher &rom_fetcher) {
	return new PCCompatible::ConcreteMachine(*target, rom_fetcher);
}

Machine::~Machine() {}
