//
//  Archimedes.cpp
//  Clock Signal
//
//  Created by Thomas Harte on 04/03/2024.
//  Copyright © 2024 Thomas Harte. All rights reserved.
//

#include "Archimedes.hpp"

#include "../../AudioProducer.hpp"
#include "../../KeyboardMachine.hpp"
#include "../../MediaTarget.hpp"
#include "../../ScanProducer.hpp"
#include "../../TimedMachine.hpp"

#include "../../../InstructionSets/ARM/Executor.hpp"
#include "../../../Outputs/Log.hpp"

#include <algorithm>
#include <array>
#include <set>
#include <vector>

namespace {

Log::Logger<Log::Source::Archimedes> logger;

enum class Zone {
	LogicallyMappedRAM,
	PhysicallyMappedRAM,
	IOControllers,
	LowROM,
	HighROM,
	VideoController,
	DMAAndMEMC,
	AddressTranslator,
};
constexpr std::array<Zone, 0x20> zones(bool is_read) {
	std::array<Zone, 0x20> zones{};
	for(size_t c = 0; c < zones.size(); c++) {
		const auto address = c << 21;
		if(address < 0x200'0000) {
			zones[c] = Zone::LogicallyMappedRAM;
		} else if(address < 0x300'0000) {
			zones[c] = Zone::PhysicallyMappedRAM;
		} else if(address < 0x340'0000) {
			zones[c] = Zone::IOControllers;
		} else if(address < 0x360'0000) {
			zones[c] = is_read ? Zone::LowROM : Zone::VideoController;
		} else if(address < 0x380'0000) {
			zones[c] = is_read ? Zone::LowROM : Zone::DMAAndMEMC;
		} else {
			zones[c] = is_read ? Zone::HighROM : Zone::AddressTranslator;
		}
	}
	return zones;
}

template <int start, int end> struct BitMask {
	static_assert(start >= end);
	static constexpr uint32_t value = ((1 << (start + 1)) - 1) - ((1 << end) - 1);
};
static_assert(BitMask<0, 0>::value == 1);
static_assert(BitMask<1, 1>::value == 2);
static_assert(BitMask<15, 15>::value == 32768);
static_assert(BitMask<15, 0>::value == 0xffff);
static_assert(BitMask<15, 14>::value == 49152);

}

namespace Archimedes {

struct Video {
	void write(uint32_t value) {
		const auto target = (value >> 24) & 0xfc;

		switch(target) {
			case 0x00:	case 0x04:	case 0x08:	case 0x0c:
			case 0x10:	case 0x14:	case 0x18:	case 0x1c:
			case 0x20:	case 0x24:	case 0x28:	case 0x2c:
			case 0x30:	case 0x34:	case 0x38:	case 0x3c:
				logger.error().append("TODO: Video palette logical colour %d to %03x", (target >> 2), value & 0x1fff);
			break;

			case 0x40:
				logger.error().append("TODO: Video border colour to %03x", value & 0x1fff);
			break;

			case 0x44:	case 0x48:	case 0x4c:
				logger.error().append("TODO: Cursor colour %d to %03x", (target - 0x44) >> 2, value & 0x1fff);
			break;

			case 0x60:	case 0x64:	case 0x68:	case 0x6c:
			case 0x70:	case 0x74:	case 0x78:	case 0x7c:
				logger.error().append("TODO: Stereo image register %d to %03x", (target - 0x60) >> 2, value & 0x7);
			break;

			case 0x80:
				logger.error().append("TODO: Video horizontal period: %d", (value >> 14) & 0x3ff);
			break;
			case 0x84:
				logger.error().append("TODO: Video horizontal sync width: %d", (value >> 14) & 0x3ff);
			break;
			case 0x88:
				logger.error().append("TODO: Video horizontal border start: %d", (value >> 14) & 0x3ff);
			break;
			case 0x8c:
				logger.error().append("TODO: Video horizontal display start: %d", (value >> 14) & 0x3ff);
			break;
			case 0x90:
				logger.error().append("TODO: Video horizontal display end: %d", (value >> 14) & 0x3ff);
			break;
			case 0x94:
				logger.error().append("TODO: Video horizontal border end: %d", (value >> 14) & 0x3ff);
			break;
			case 0x98:
				logger.error().append("TODO: Video horizontal cursor end: %d", (value >> 14) & 0x3ff);
			break;
			case 0x9c:
				logger.error().append("TODO: Video horizontal interlace: %d", (value >> 14) & 0x3ff);
			break;

			case 0xa0:
				logger.error().append("TODO: Video vertical period: %d", (value >> 14) & 0x3ff);
			break;
			case 0xa4:
				logger.error().append("TODO: Video vertical sync width: %d", (value >> 14) & 0x3ff);
			break;
			case 0xa8:
				logger.error().append("TODO: Video vertical border start: %d", (value >> 14) & 0x3ff);
			break;
			case 0xac:
				logger.error().append("TODO: Video vertical display start: %d", (value >> 14) & 0x3ff);
			break;
			case 0xb0:
				logger.error().append("TODO: Video vertical display end: %d", (value >> 14) & 0x3ff);
			break;
			case 0xb4:
				logger.error().append("TODO: Video vertical border end: %d", (value >> 14) & 0x3ff);
			break;
			case 0xb8:
				logger.error().append("TODO: Video vertical cursor start: %d", (value >> 14) & 0x3ff);
			break;
			case 0xbc:
				logger.error().append("TODO: Video vertical cursor end: %d", (value >> 14) & 0x3ff);
			break;

			case 0xc0:
				logger.error().append("TODO: Sound frequency: %d", value & 0x7f);
			break;

			case 0xe0:
				logger.error().append("TODO: video control: %08x", value);
			break;

			default:
				logger.error().append("TODO: unrecognised VIDC write of %08x", value);
			break;
		}
	}
};

// IRQ A flags
namespace IRQA {
	// The first four of these are taken from the A500 documentation and may be inaccurate.
	static constexpr uint8_t PrinterBusy		= 0x01;
	static constexpr uint8_t SerialRinging		= 0x02;
	static constexpr uint8_t PrinterAcknowledge	= 0x04;
	static constexpr uint8_t VerticalFlyback	= 0x08;
	static constexpr uint8_t PowerOnReset		= 0x10;
	static constexpr uint8_t Timer0				= 0x20;
	static constexpr uint8_t Timer1				= 0x40;
	static constexpr uint8_t SetAlways			= 0x80;
}

// IRQ B flags
namespace IRQB {
	// These are taken from the A3010 documentation.
	static constexpr uint8_t PoduleFIQRequest		= 0x01;
	static constexpr uint8_t SoundBufferPointerUsed	= 0x02;
	static constexpr uint8_t SerialLine				= 0x04;
	static constexpr uint8_t IDE					= 0x08;
	static constexpr uint8_t FloppyDiscInterrupt	= 0x10;
	static constexpr uint8_t PoduleIRQRequest		= 0x20;
	static constexpr uint8_t KeyboardTransmitEmpty	= 0x40;
	static constexpr uint8_t KeyboardReceiveFull	= 0x80;
}

// FIQ flags
namespace FIQ {
	// These are taken from the A3010 documentation.
	static constexpr uint8_t FloppyDiscData			= 0x01;
	static constexpr uint8_t SerialLine				= 0x10;
	static constexpr uint8_t PoduleFIQRequest		= 0x40;
	static constexpr uint8_t SetAlways				= 0x80;
}

namespace InterruptRequests {
	static constexpr int IRQ = 0x01;
	static constexpr int FIQ = 0x02;
};

struct Interrupts {
	int interrupt_mask() const {
		return
			((irq_a_.request() | irq_b_.request()) ? InterruptRequests::IRQ : 0) |
			(fiq_.request() ? InterruptRequests::FIQ : 0);
	}

	template <int c>
	bool tick_timer() {
		if(!counters_[c].value && !counters_[c].reload) {
			return false;
		}

		--counters_[c].value;
		if(!counters_[c].value) {
			counters_[c].value = counters_[c].reload;

			switch(c) {
				case 0:	return irq_a_.apply(IRQA::Timer0);
				case 1:	return irq_a_.apply(IRQA::Timer1);
				default: break;
			}
			// TODO: events for timers 2 and 3.
		}

		return false;
	}

	bool tick_timers() {
		bool did_change_interrupts = false;
		did_change_interrupts |= tick_timer<0>();
		did_change_interrupts |= tick_timer<1>();
		did_change_interrupts |= tick_timer<2>();
		did_change_interrupts |= tick_timer<3>();
		return did_change_interrupts;
	}

	static constexpr uint32_t AddressMask = 0x1f'ffff;

	bool read(uint32_t address, uint8_t &value) const {
		const auto target = address & AddressMask;
		value = 0xff;
		switch(target) {
			default:
				logger.error().append("Unrecognised IOC read from %08x", address);
			break;

			case 0x3200000 & AddressMask:
				logger.error().append("TODO: IOC control read");
				value = 0;
			return true;

			case 0x3200004 & AddressMask:
				logger.error().append("TODO: IOC serial receive");
				value = 0;
			return true;

			// IRQ A.
			case 0x3200010 & AddressMask:
				value = irq_a_.status;
//				logger.error().append("IRQ A status is %02x", value);
			return true;
			case 0x3200014 & AddressMask:
				value = irq_a_.request();
				logger.error().append("IRQ A request is %02x", value);
			return true;
			case 0x3200018 & AddressMask:
				value = irq_a_.mask;
				logger.error().append("IRQ A mask is %02x", value);
			return true;

			// IRQ B.
			case 0x3200020 & AddressMask:
				value = irq_b_.status;
//				logger.error().append("IRQ B status is %02x", value);
			return true;
			case 0x3200024 & AddressMask:
				value = irq_b_.request();
				logger.error().append("IRQ B request is %02x", value);
			return true;
			case 0x3200028 & AddressMask:
				value = irq_b_.mask;
				logger.error().append("IRQ B mask is %02x", value);
			return true;

			// FIQ.
			case 0x3200030 & AddressMask:
				value = fiq_.status;
				logger.error().append("FIQ status is %02x", value);
			return true;
			case 0x3200034 & AddressMask:
				value = fiq_.request();
				logger.error().append("FIQ request is %02x", value);
			return true;
			case 0x3200038 & AddressMask:
				value = fiq_.mask;
				logger.error().append("FIQ mask is %02x", value);
			return true;

			// Counters.
			case 0x3200040 & AddressMask:
			case 0x3200050 & AddressMask:
			case 0x3200060 & AddressMask:
			case 0x3200070 & AddressMask:
				value = counters_[(target >> 4) - 0x4].output & 0xff;
				logger.error().append("%02x: Counter %d low is %02x", target, (target >> 4) - 0x4, value);
			return true;

			case 0x3200044 & AddressMask:
			case 0x3200054 & AddressMask:
			case 0x3200064 & AddressMask:
			case 0x3200074 & AddressMask:
				value = counters_[(target >> 4) - 0x4].output >> 8;
				logger.error().append("%02x: Counter %d high is %02x", target, (target >> 4) - 0x4, value);
			return true;
		}

		return true;
	}

	bool write(uint32_t address, uint8_t value) {
		const auto target = address & AddressMask;
		switch(target) {
			default:
				logger.error().append("Unrecognised IOC write of %02x at %08x", value, address);
			break;

			case 0x320'0000 & AddressMask:
				logger.error().append("TODO: IOC control write %02x", value);
			return true;

			case 0x320'0004 & AddressMask:
				logger.error().append("TODO: IOC serial transmit %02x", value);
			return true;

			case 0x320'0014 & AddressMask:
				// b2: clear IF.
				// b3: clear IR.
				// b4: clear POR.
				// b5: clear TM[0].
				// b6: clear TM[1].
				irq_a_.clear(value & 0x7c);
			return true;

			// Interrupts.
			case 0x320'0018 & AddressMask:	irq_a_.mask = value;	return true;
			case 0x320'0028 & AddressMask:	irq_b_.mask = value;	return true;
			case 0x320'0038 & AddressMask:	fiq_.mask = value;		return true;

			// Counters.
			case 0x320'0040 & AddressMask:
			case 0x320'0050 & AddressMask:
			case 0x320'0060 & AddressMask:
			case 0x320'0070 & AddressMask:
				counters_[(target >> 4) - 0x4].reload = uint16_t(
					(counters_[(target >> 4) - 0x4].reload & 0xff00) | value
				);
			return true;

			case 0x320'0044 & AddressMask:
			case 0x320'0054 & AddressMask:
			case 0x320'0064 & AddressMask:
			case 0x320'0074 & AddressMask:
				counters_[(target >> 4) - 0x4].reload = uint16_t(
					(counters_[(target >> 4) - 0x4].reload & 0x00ff) | (value << 8)
				);
			return true;

			case 0x320'0048 & AddressMask:
			case 0x320'0058 & AddressMask:
			case 0x320'0068 & AddressMask:
			case 0x320'0078 & AddressMask:
				counters_[(target >> 4) - 0x4].value = counters_[(target >> 4) - 0x4].reload;
			return true;

			case 0x320'004c & AddressMask:
			case 0x320'005c & AddressMask:
			case 0x320'006c & AddressMask:
			case 0x320'007c & AddressMask:
				counters_[(target >> 4) - 0x4].output = counters_[(target >> 4) - 0x4].value;
			return true;

			case 0x327'0000 & AddressMask:
				logger.error().append("TODO: exteded external podule space");
			return true;

			case 0x331'0000 & AddressMask:
				logger.error().append("TODO: 1772 / disk write");
			return true;

			case 0x335'0000 & AddressMask:
				logger.error().append("TODO: LS374 / printer data write");
			return true;

			case 0x335'0018 & AddressMask:
				logger.error().append("TODO: latch B write");
			return true;

			case 0x335'0040 & AddressMask:
				logger.error().append("TODO: latch A write");
			return true;

			case 0x335'0048 & AddressMask:
				logger.error().append("TODO: latch C write");
			return true;

			case 0x336'0000 & AddressMask:
				logger.error().append("TODO: podule interrupt request");
			return true;

			case 0x336'0004 & AddressMask:
				logger.error().append("TODO: podule interrupt mask");
			return true;

			case 0x33a'0000 & AddressMask:
				logger.error().append("TODO: 6854 / econet write");
			return true;

			case 0x33b'0000 & AddressMask:
				logger.error().append("TODO: 6551 / serial line write");
			return true;
		}

		return true;
	}

	Interrupts() {
		irq_a_.status = IRQA::SetAlways | IRQA::PowerOnReset;
		irq_b_.status = 0x00;
		fiq_.status = 0x80;				// 'set always'.
	}

private:
	struct Interrupt {
		uint8_t status, mask;
		uint8_t request() const {
			return status & mask;
		}
		bool apply(uint8_t value) {
			status |= value;
			return status & mask;
		}
		void clear(uint8_t bits) {
			status &= ~bits;
		}
	};
	Interrupt irq_a_, irq_b_, fiq_;

	struct Counter {
		uint16_t value;
		uint16_t reload;
		uint16_t output;
	};
	Counter counters_[4];
};

/// Primarily models the MEMC.
template <typename IOCWriteDelegateT>
struct Memory {
	Memory(IOCWriteDelegateT &ioc_write_delegate) : ioc_write_delegate_(ioc_write_delegate) {}

	int interrupt_mask() const {
		return ioc_.interrupt_mask();
	}

	void set_rom(const std::vector<uint8_t> &rom) {
		std::copy(
			rom.begin(),
			rom.begin() + static_cast<ptrdiff_t>(std::min(rom.size(), rom_.size())),
			rom_.begin());
	}

	template <typename IntT>
	uint32_t aligned(uint32_t address) {
		if constexpr (std::is_same_v<IntT, uint32_t>) {
			return address & static_cast<uint32_t>(~3);
		}
		return address;
	}

	template <typename IntT>
	bool write(uint32_t address, IntT source, InstructionSet::ARM::Mode mode, bool) {
		if(mode == InstructionSet::ARM::Mode::User && address >= 0x2000000) {
			return false;
		}

		switch (write_zones_[(address >> 21) & 31]) {
			case Zone::DMAAndMEMC:
				if((address & 0b1110'0000'0000'0000'0000) == 0b1110'0000'0000'0000'0000) {
					// "The parameters are encoded into the processor address lines".
					os_mode_ = address & (1 << 12);
					sound_dma_enable_ = address & (1 << 11);
					video_dma_enable_ = address & (1 << 10);
					switch((address >> 8) & 3) {
						default:
							dynamic_ram_refresh_ = DynamicRAMRefresh::None;
						break;
						case 0b01:
						case 0b11:
							dynamic_ram_refresh_ = DynamicRAMRefresh((address >> 8) & 3);
						break;
					}
					high_rom_access_time_ = ROMAccessTime((address >> 6) & 3);
					low_rom_access_time_ = ROMAccessTime((address >> 4) & 3);
					page_size_ = PageSize((address >> 2) & 3);

					logger.info().append("MEMC Control: %08x -> OS:%d sound:%d video:%d refresh:%d high:%d low:%d size:%d", address, os_mode_, sound_dma_enable_, video_dma_enable_, dynamic_ram_refresh_, high_rom_access_time_, low_rom_access_time_, page_size_);
					map_dirty_ = true;

					return true;
				} else {
					logger.error().append("TODO: DMA/MEMC %08x to %08x", source, address);
				}
			break;

			case Zone::LogicallyMappedRAM: {
				const auto item = logical_ram<IntT, false>(address, mode);
				if(!item) {
					return false;
				}
				*item = source;
				return true;
			} break;

			case Zone::IOControllers:
				// TODO: have I overrestricted the value type for the IOC area?
				ioc_.write(address, uint8_t(source));
				ioc_write_delegate_.did_write_ioc();
			return true;

			case Zone::VideoController:
				// TODO: handle byte writes correctly.
				vidc_.write(source);
			break;

			case Zone::PhysicallyMappedRAM:
				physical_ram<IntT>(address) = source;
			return true;

			case Zone::AddressTranslator:
//				printf("Translator write at %08x; replaces %08x\n", address, pages_[address & 0x7f]);
				pages_[address & 0x7f] = address;
				map_dirty_ = true;
			break;

			default:
//				printf("TODO: write of %08x to %08x [%lu]\n", source, address, sizeof(IntT));
			break;
		}

		return true;
	}

	template <typename IntT>
	bool read(uint32_t address, IntT &source, InstructionSet::ARM::Mode mode, bool) {
		if(mode == InstructionSet::ARM::Mode::User && address >= 0x2000000) {
			return false;
		}

		switch (read_zones_[(address >> 21) & 31]) {
			case Zone::PhysicallyMappedRAM:
				source = physical_ram<IntT>(address);
			return true;

			case Zone::LogicallyMappedRAM: {
				if(!has_moved_rom_) {	// TODO: maintain this state in the zones table.
					source = high_rom<IntT>(address);
					return true;
				}

				const auto item = logical_ram<IntT, true>(address, mode);
				if(!item) {
					return false;
				}
				source = *item;
				return true;
			} break;

			case Zone::LowROM:
				logger.error().append("TODO: Low ROM read from %08x", address);
			break;

			case Zone::HighROM:
				// Real test is: require A24=A25=0, then A25=1.
				// TODO: as above, move this test into the zones tables.
				has_moved_rom_ = true;
				source = high_rom<IntT>(address);
			return true;

			case Zone::IOControllers:	{
				if constexpr (std::is_same_v<IntT, uint8_t>) {
					ioc_.read(address, source);
					return true;
				} else {
					// TODO: generalise this adaptation of an 8-bit device to the 32-bit bus, which probably isn't right anyway.
					uint8_t value;
					ioc_.read(address, value);
					source = value;
					return true;
				}
			}

			default:
				logger.error().append("TODO: read from %08x", address);
			break;
		}

		source = 0;
		return true;
	}

	bool tick_timers() {
		return ioc_.tick_timers();
	}

	private:
		bool has_moved_rom_ = false;
		std::array<uint8_t, 4*1024*1024> ram_{};
		std::array<uint8_t, 2*1024*1024> rom_;
		Interrupts ioc_;
		Video vidc_;
		IOCWriteDelegateT &ioc_write_delegate_;

		template <typename IntT>
		IntT &physical_ram(uint32_t address) {
			address = aligned<IntT>(address);
			address &= (ram_.size() - 1);
			return *reinterpret_cast<IntT *>(&ram_[address]);
		}

		template <typename IntT>
		IntT &high_rom(uint32_t address) {
			address = aligned<IntT>(address);
			return *reinterpret_cast<IntT *>(&rom_[address & (rom_.size() - 1)]);
		}

		static constexpr std::array<Zone, 0x20> read_zones_ = zones(true);
		static constexpr std::array<Zone, 0x20> write_zones_ = zones(false);

		// Control register values.
		bool os_mode_ = false;
		bool sound_dma_enable_ = false;
		bool video_dma_enable_ = false;	// "Unaffected" by reset, so here picked arbitrarily.

		enum class DynamicRAMRefresh {
			None = 0b00,
			DuringFlyback = 0b01,
			Continuous = 0b11,
		} dynamic_ram_refresh_ = DynamicRAMRefresh::None;	// State at reset is undefined; constrain to a valid enum value.

		enum class ROMAccessTime {
			ns450 = 0b00,
			ns325 = 0b01,
			ns200 = 0b10,
			ns200with60nsNibble = 0b11,
		} high_rom_access_time_ = ROMAccessTime::ns450, low_rom_access_time_ = ROMAccessTime::ns450;

		enum class PageSize {
			kb4 = 0b00,
			kb8 = 0b01,
			kb16 = 0b10,
			kb32 = 0b11,
		} page_size_ = PageSize::kb4;

		// Address translator.
		//
		// MEMC contains one entry per a physical page number, indicating where it goes logically.
		// Any logical access is tested against all 128 mappings. So that's backwards compared to
		// the ideal for an emulator, which would map from logical to physical, even if a lot more
		// compact — there are always 128 physical pages; there are up to 8192 logical pages.
		//
		// So captured here are both the physical -> logical map as representative of the real
		// hardware, and the reverse logical -> physical map, which is built (and rebuilt, and rebuilt)
		// from the other.

		// Physical to logical mapping.
		std::array<uint32_t, 128> pages_{};

		// Logical to physical mapping.
		struct MappedPage {
			uint8_t *target = nullptr;
			uint8_t protection_level = 0;
		};
		std::array<MappedPage, 8192> mapping_;
		bool map_dirty_ = true;

		template <typename IntT, bool is_read>
		IntT *logical_ram(uint32_t address, InstructionSet::ARM::Mode mode) {
			// Possibly TODO: this recompute-if-dirty flag is supposed to ameliorate for an expensive
			// mapping process. It can be eliminated when the process is improved.
			if(map_dirty_) {
				update_mapping();
				map_dirty_ = false;
			}

			address = aligned<IntT>(address);
			address &= 0x1ff'ffff;
			size_t page;

			// TODO: eliminate switch here.
			switch(page_size_) {
				default:
				case PageSize::kb4:
					page = address >> 12;
					address &= 0x0fff;
				break;
				case PageSize::kb8:
					page = address >> 13;
					address &= 0x1fff;
				break;
				case PageSize::kb16:
					page = address >> 14;
					address &= 0x3fff;
				break;
				case PageSize::kb32:
					page = address >> 15;
					address &= 0x7fff;
				break;
			}

			if(!mapping_[page].target) {
				return nullptr;
			}

			// TODO: eliminate switch here.
			// Top of my head idea: is_read, is_user and is_os_mode make three bits, so
			// keep a one-byte bitmap of permitted accesses rather than the raw protection
			// level?
			switch(mapping_[page].protection_level) {
				case 0b00:	break;
				case 0b01:
					if(!is_read && mode == InstructionSet::ARM::Mode::User) {
						return nullptr;
					}
				break;
				default:
					if(mode == InstructionSet::ARM::Mode::User) {
						return nullptr;
					}
					if(!is_read && !os_mode_) {
						return nullptr;
					}
				break;
			}

			return reinterpret_cast<IntT *>(mapping_[page].target + address);
		}

		void update_mapping() {
			// For each physical page, project it into logical space.
			switch(page_size_) {
				default:
				case PageSize::kb4:		update_mapping<PageSize::kb4>();	break;
				case PageSize::kb8:		update_mapping<PageSize::kb8>();	break;
				case PageSize::kb16:	update_mapping<PageSize::kb16>();	break;
				case PageSize::kb32:	update_mapping<PageSize::kb32>();	break;
			}
		}

		template <PageSize size>
		void update_mapping() {
			// Clear all logical mappings.
			std::fill(mapping_.begin(), mapping_.end(), MappedPage{});

			// For each physical page, project it into logical space
			// and store it.
			for(const auto page: pages_) {
				uint32_t physical, logical;

				switch(size) {
					case PageSize::kb4:
						// 4kb:
						//		A[6:0] -> PPN[6:0]
						//		A[11:10] -> LPN[12:11];	A[22:12] -> LPN[10:0]	i.e. 8192 logical pages
						physical = page & BitMask<6, 0>::value;

						physical <<= 12;

						logical = (page & BitMask<11, 10>::value) << 1;
						logical |= (page & BitMask<22, 12>::value) >> 12;
					break;

					case PageSize::kb8:
						// 8kb:
						//		A[0] -> PPN[6]; A[6:1] -> PPN[5:0]
						//		A[11:10] -> LPN[11:10];	A[22:13] -> LPN[9:0]	i.e. 4096 logical pages
						physical = (page & BitMask<0, 0>::value) << 6;
						physical |= (page & BitMask<6, 1>::value) >> 1;

						physical <<= 13;

						logical = page & BitMask<11, 10>::value;
						logical |= (page & BitMask<22, 13>::value) >> 13;
					break;

					case PageSize::kb16:
						// 16kb:
						//		A[1:0] -> PPN[6:5]; A[6:2] -> PPN[4:0]
						//		A[11:10] -> LPN[10:9];	A[22:14] -> LPN[8:0]	i.e. 2048 logical pages
						physical = (page & BitMask<1, 0>::value) << 5;
						physical |= (page & BitMask<6, 2>::value) >> 2;

						physical <<= 14;

						logical = (page & BitMask<11, 10>::value) >> 1;
						logical |= (page & BitMask<22, 14>::value) >> 14;
					break;

					case PageSize::kb32:
						// 32kb:
						//		A[1] -> PPN[6]; A[2] -> PPN[5]; A[0] -> PPN[4]; A[6:3] -> PPN[3:0]
						//		A[11:10] -> LPN[9:8];	A[22:15] -> LPN[7:0]	i.e. 1024 logical pages
						physical = (page & BitMask<1, 1>::value) << 5;
						physical |= (page & BitMask<2, 2>::value) << 3;
						physical |= (page & BitMask<0, 0>::value) << 4;
						physical |= (page & BitMask<6, 3>::value) >> 3;

						physical <<= 15;

						logical = (page & BitMask<11, 10>::value) >> 2;
						logical |= (page & BitMask<22, 15>::value) >> 15;
					break;
				}

//				printf("%08x => physical %d -> logical %d\n", page, (physical >> 15), logical);

				// TODO: consider clashes.
				// TODO: what if there's less than 4mb present?
				mapping_[logical].target = &ram_[physical];
				mapping_[logical].protection_level = (page >> 8) & 3;
			}
		}
};

class ConcreteMachine:
	public Machine,
	public MachineTypes::MediaTarget,
	public MachineTypes::TimedMachine,
	public MachineTypes::ScanProducer
{
	// TODO: pick a sensible clock rate; this is just code for '24 MIPS, please'.
	static constexpr int ClockRate = 24'000'000;

	// Timers tick at 2Mhz, so figure out the proper divider for that.
	static constexpr int TimerTarget = ClockRate / 2'000'000;
	int timer_divider_ = TimerTarget;

	public:
		ConcreteMachine(
			const Analyser::Static::Target &target,
			const ROMMachine::ROMFetcher &rom_fetcher
		) : executor_(*this) {
			set_clock_rate(ClockRate);

			constexpr ROM::Name risc_os = ROM::Name::AcornRISCOS319;
			ROM::Request request(risc_os);
			auto roms = rom_fetcher(request);
			if(!request.validate(roms)) {
				throw ROMMachine::Error::MissingROMs;
			}

			executor_.bus.set_rom(roms.find(risc_os)->second);
			insert_media(target.media);
		}

		void did_write_ioc() {
			test_interrupts();
		}

	private:
		// MARK: - ScanProducer.
		void set_scan_target(Outputs::Display::ScanTarget *scan_target) override {
			(void)scan_target;
		}
		Outputs::Display::ScanStatus get_scaled_scan_status() const override {
			return Outputs::Display::ScanStatus();
		}

		// MARK: - TimedMachine.
		void run_for(Cycles cycles) override {
			static uint32_t last_pc = 0;

			auto instructions = cycles.as<int>();

			while(instructions) {
				auto run_length = std::min(timer_divider_, instructions);
				instructions -= run_length;
				timer_divider_ -= run_length;

				while(run_length--) {
					uint32_t instruction;
					if(!executor_.bus.read(executor_.pc(), instruction, executor_.registers().mode(), false)) {
						logger.info().append("Prefetch abort at %08x; last good was at %08x", executor_.pc(), last_pc);
						executor_.prefetch_abort();

						// TODO: does a double abort cause a reset?
						executor_.bus.read(executor_.pc(), instruction, executor_.registers().mode(), false);
					} else {
						last_pc = executor_.pc();
					}
					// TODO: pipeline prefetch?

					static bool log = false;

//					if(executor_.pc() == 0x03810398) {
//						printf("");
//					}
//					log |= (executor_.pc() > 0x02000000 && executor_.pc() < 0x02000078);
//					log |= executor_.pc() == 0x03811eb4;
//					log |= (executor_.pc() > 0x03801000);
//					log &= executor_.pc() != 0x03801a0c;

//					if(executor_.pc() == 0x02000078) {
//						if(!all.empty()) {
//							int c = 0;
//							for(auto instr: all) {
//								printf("0x%08x, ", instr);
//								++c;
//								if(!(c&31)) printf("\n");
//							}
//							all.clear();
//						}
//						return;
//					}

					if(log) {
						auto info = logger.info();
						info.append("%08x: %08x prior:[", executor_.pc(), instruction);
						for(uint32_t c = 0; c < 15; c++) {
							info.append("r%d:%08x ", c, executor_.registers()[c]);
						}
						info.append("]");
					}
//					logger.info().append("%08x: %08x", executor_.pc(), instruction);
					InstructionSet::ARM::execute(instruction, executor_);

//					if(
//						executor_.pc() > 0x038021d0 &&
//							last_r1 != executor_.registers()[1]
//							 ||
//						(
//							last_link != executor_.registers()[14] ||
//							last_r0 != executor_.registers()[0] ||
//							last_r10 != executor_.registers()[10] ||
//							last_r1 != executor_.registers()[1]
//						)
//					) {
//						logger.info().append("%08x modified R14 to %08x; R0 to %08x; R10 to %08x; R1 to %08x",
//							last_pc,
//							executor_.registers()[14],
//							executor_.registers()[0],
//							executor_.registers()[10],
//							executor_.registers()[1]
//						);
//						logger.info().append("%08x modified R1 to %08x",
//							last_pc,
//							executor_.registers()[1]
//						);
//						last_link = executor_.registers()[14];
//						last_r0 = executor_.registers()[0];
//						last_r10 = executor_.registers()[10];
//						last_r1 = executor_.registers()[1];
//					}
				}

				if(!timer_divider_) {
					timer_divider_ = TimerTarget;

					if(executor_.bus.tick_timers()) {
						test_interrupts();
					}
				}
			}
		}

		void test_interrupts() {
			using Exception = InstructionSet::ARM::Registers::Exception;

			const int requests = executor_.bus.interrupt_mask();
			if((requests & InterruptRequests::FIQ) && executor_.registers().interrupt<Exception::FIQ>()) {
				return;
			}
			if(requests & InterruptRequests::IRQ) {
				executor_.registers().interrupt<Exception::IRQ>();
			}
		}

		// MARK: - MediaTarget
		bool insert_media(const Analyser::Static::Media &) override {
//			int c = 0;
//			for(auto &disk : media.disks) {
//				fdc_.set_disk(disk, c);
//				c++;
//				if(c == 4) break;
//			}
//			return true;
			return false;
		}

		// MARK: - ARM execution
		static constexpr auto arm_model = InstructionSet::ARM::Model::ARMv2;
		InstructionSet::ARM::Executor<arm_model, Memory<ConcreteMachine>> executor_;
};

}

using namespace Archimedes;

std::unique_ptr<Machine> Machine::Archimedes(const Analyser::Static::Target *target, const ROMMachine::ROMFetcher &rom_fetcher) {
	return std::make_unique<ConcreteMachine>(*target, rom_fetcher);
}
