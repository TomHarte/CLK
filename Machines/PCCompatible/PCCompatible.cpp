//
//  PCCompatible.cpp
//  Clock Signal
//
//  Created by Thomas Harte on 15/11/2023.
//  Copyright © 2023 Thomas Harte. All rights reserved.
//

#include "PCCompatible.hpp"

#include "CGA.hpp"
#include "CPUControl.hpp"
#include "DMA.hpp"
#include "FloppyController.hpp"
#include "IDE.hpp"
#include "KeyboardController.hpp"
#include "KeyboardMapper.hpp"
#include "LinearMemory.hpp"
#include "MDA.hpp"
#include "PIC.hpp"
#include "PIT.hpp"
#include "ProcessorByModel.hpp"
#include "RTC.hpp"
#include "SegmentedMemory.hpp"
#include "Speaker.hpp"

#include "Activity/Source.hpp"

#include "InstructionSets/x86/Decoder.hpp"
#include "InstructionSets/x86/Exceptions.hpp"
#include "InstructionSets/x86/Flags.hpp"
#include "InstructionSets/x86/Instruction.hpp"
#include "InstructionSets/x86/Perform.hpp"
#include "InstructionSets/x86/Registers.hpp"

#include "Components/8255/i8255.hpp"

#include "Numeric/RegisterSizes.hpp"

#include "Outputs/CRT/CRT.hpp"
#include "Outputs/Log.hpp"
#include "Outputs/Speaker/Speaker.hpp"

#include "Machines/AudioProducer.hpp"
#include "Machines/KeyboardMachine.hpp"
#include "Machines/MediaTarget.hpp"
#include "Machines/ScanProducer.hpp"
#include "Machines/TimedMachine.hpp"

#include "Analyser/Static/PCCompatible/Target.hpp"

#include <array>
#include <concepts>
#include <iostream>
#include <type_traits>

bool should_log = false;

namespace PCCompatible {
namespace {
Log::Logger<Log::Source::PCCompatible> log;
}

using Target = Analyser::Static::PCCompatible::Target;

// Map from members of the VideoAdaptor enum to concrete class types.
template <Target::VideoAdaptor adaptor> struct Adaptor;
template <> struct Adaptor<Target::VideoAdaptor::MDA> {		using type = MDA;	};
template <> struct Adaptor<Target::VideoAdaptor::CGA> {		using type = CGA;	};

template <Analyser::Static::PCCompatible::Model model>
class PITObserver {
public:
	PITObserver(PICs<model> &pics, Speaker &speaker) : pics_(pics), speaker_(speaker) {}

	template <int channel>
	void update_output(const bool new_level) {
		//	channel 0 is connected to IRQ 0;
		//	channel 1 is used for DRAM refresh (presumably connected to DMA?);
		//	channel 2 is gated by a PPI output and feeds into the speaker.
		switch(channel) {
			default: break;
			case 0: pics_.pic[0].template apply_edge<0>(new_level);	break;
			case 2: speaker_.set_pit(new_level);					break;
		}
	}

private:
	PICs<model> &pics_;
	Speaker &speaker_;
};

template <Analyser::Static::PCCompatible::Model model>
using PIT = i8253<false, PITObserver<model>>;

template <Analyser::Static::PCCompatible::Model model>
class i8255PortHandler : public Intel::i8255::PortHandler {
public:
	i8255PortHandler(
		Speaker &speaker,
		KeyboardController<model> &keyboard,
		const Target::VideoAdaptor adaptor,
		const int drive_count
	) :
		speaker_(speaker), keyboard_(keyboard) {
			// High switches:
			//
			// b3, b2: drive count; 00 = 1, 01 = 2, etc
			// b1, b0: video mode (00 = ROM; 01 = CGA40; 10 = CGA80; 11 = MDA)
			switch(adaptor) {
				default: break;
				case Target::VideoAdaptor::MDA:	high_switches_ |= 0b11;	break;
				case Target::VideoAdaptor::CGA:	high_switches_ |= 0b10;	break;	// Assume 80 columns.
			}
			high_switches_ |= uint8_t(drive_count << 2);

			// Low switches:
			//
			// b3, b2: RAM on motherboard (64 * bit pattern)
			// b1: 1 => FPU present; 0 => absent;
			// b0: 1 => floppy drive present; 0 => absent.
			low_switches_ |= 0b1100;	// Assume maximum RAM.
			if(drive_count) low_switches_ |= 0xb0001;
		}

	/// Supplies a hint about the user's display choice. If the high switches haven't been read yet and this is a CGA device,
	/// this hint will be used to select between 40- and 80-column default display.
	void hint_is_composite(const bool composite) {
		if(high_switches_observed_) {
			return;
		}

		switch(high_switches_ & 3) {
			// Do nothing if a non-CGA card is in use.
			case 0b00:	case 0b11:
				break;

			default:
				high_switches_ &= ~0b11;
				high_switches_ |= composite ? 0b01 : 0b10;
			break;
		}
	}

	void set_value(const int port, const uint8_t value) {
		switch(port) {
			case 1:
				// b7: 0 => enable keyboard read (and IRQ); 1 => don't;
				// b6: 0 => hold keyboard clock low; 1 => don't;
				// b5: 1 => disable IO check; 0 => don't;
				// b4: 1 => disable memory parity check; 0 => don't;
				// b3: [5150] cassette motor control; [5160] high or low switches select;
				// b2: [5150] high or low switches select; [5160] 1 => disable turbo mode;
				// b1, b0: speaker control.
				enable_keyboard_ = !(value & 0x80);
				keyboard_.set_mode(value >> 6);

				use_high_switches_ = value & 0x08;
				speaker_.set_control(value & 0x01, value & 0x02);
			break;
		}
	}

	uint8_t get_value(const int port) {
		switch(port) {
			case 0:
				high_switches_observed_ = true;
				return enable_keyboard_ ? keyboard_.read() : uint8_t((high_switches_ << 4) | low_switches_);
					// Guesses that switches is high and low combined as below.

			case 2:
				// b7: 1 => memory parity error; 0 => none;
				// b6: 1 => IO channel error; 0 => none;
				// b5: timer 2 output;	[TODO]
				// b4: cassette data input; [TODO]
				// b3...b0: whichever of the high and low switches is selected.
				high_switches_observed_ |= use_high_switches_;
				return
					use_high_switches_ ? high_switches_ : low_switches_;
		}
		return 0;
	};

private:
	bool high_switches_observed_ = false;
	uint8_t high_switches_ = 0;
	uint8_t low_switches_ = 0;

	bool use_high_switches_ = false;
	Speaker &speaker_;
	KeyboardController<model> &keyboard_;

	bool enable_keyboard_ = false;
};
template <Analyser::Static::PCCompatible::Model model>
using PPI = Intel::i8255::i8255<i8255PortHandler<model>>;

template <Analyser::Static::PCCompatible::Model model, Target::VideoAdaptor video>
class IO {
public:
	IO(
		PIT<model> &pit,
		DMA<model> &dma,
		PPI<model> &ppi,
		PICs<model> &pics,
		typename Adaptor<video>::type &card,
		FloppyController<model> &fdc,
		KeyboardController<model> &keyboard,
		RTC &rtc
	) :
		pit_(pit), dma_(dma), ppi_(ppi), pics_(pics), video_(card), fdc_(fdc), keyboard_(keyboard), rtc_(rtc) {}

private:
	static constexpr bool wilfully_unimplemented(const uint16_t port) {
		switch(port) {
			default:	return false;

			case 0x0278:	case 0x0279:	case 0x027a:
			case 0x0378:	case 0x0379:	case 0x037a:
			case 0x03bc:	case 0x03bd:	case 0x03be:
				// Ignore parallel port accesses.
			return true;

			case 0x02e8:	case 0x02e9:	case 0x02ea:	case 0x02eb:
			case 0x02ec:	case 0x02ed:	case 0x02ee:	case 0x02ef:
			case 0x02f8:	case 0x02f9:	case 0x02fa:	case 0x02fb:
			case 0x02fc:	case 0x02fd:	case 0x02fe:	case 0x02ff:
			case 0x03e8:	case 0x03e9:	case 0x03ea:	case 0x03eb:
			case 0x03ec:	case 0x03ed:	case 0x03ee:	case 0x03ef:
			case 0x03f8:	case 0x03f9:	case 0x03fa:	case 0x03fb:
			case 0x03fc:	case 0x03fd:	case 0x03fe:	case 0x03ff:
				// Ignore serial port accesses.
			return true;

			case 0x0201:
				// Ignore game port.
			return true;
		}
	}

private:
	static constexpr uint16_t crtc_base =
		video == Target::VideoAdaptor::MDA ? 0x03b0 : 0x03d0;

	template <typename IntT>
	void log_unhandled(const uint16_t port, const IntT value) {
		if(wilfully_unimplemented(port)) {
			return;
		}
		log.error().append("Unhandled out: %02x to %04x", value, port);
	};

	template <typename IntT>
	bool require_at(const uint16_t port, const IntT value) {
		if constexpr (is_at(model)) {
			return true;
		}
		log_unhandled(port, value);
		return false;
	};

	template <typename IntT>
	bool require_ide(const uint16_t port, const IntT value) {
		if constexpr (has_ide(model)) {
			return true;
		}
		log_unhandled(port, value);
		return false;
	};

	template <bool is_retry>
	void out16(const uint16_t port, const uint16_t value) {
		switch(port) {
			default:
				if(is_retry) {
					log_unhandled(port, value);
				} else {
					out8<true>(port, uint8_t(value));
					out8<true>(port + 1, uint8_t(value >> 8));
				}
			break;

			// IDE.
			case 0x01f0:	if(require_ide(port, value)) ide_.set_data(value);					break;
		}
	}

	template <bool is_retry>
	void out8(const uint16_t port, const uint8_t value) {
		switch(port) {
			default:
				if(is_retry) {
					log_unhandled(port, value);
				} else {
					out16<true>(port, uint16_t(value | (value << 8)));
				}
			break;

			case 0x0070:	rtc_.write<0>(value);	break;
			case 0x0071:	rtc_.write<1>(value);	break;

			case 0x00f1:
				log.error().append("TODO: coprocessor reset");
			break;

			case 0x0000:	dma_.controllers[0].template write<0x0>(value);	break;
			case 0x0001:	dma_.controllers[0].template write<0x1>(value);	break;
			case 0x0002:	dma_.controllers[0].template write<0x2>(value);	break;
			case 0x0003:	dma_.controllers[0].template write<0x3>(value);	break;
			case 0x0004:	dma_.controllers[0].template write<0x4>(value);	break;
			case 0x0005:	dma_.controllers[0].template write<0x5>(value);	break;
			case 0x0006:	dma_.controllers[0].template write<0x6>(value);	break;
			case 0x0007:	dma_.controllers[0].template write<0x7>(value);	break;
			case 0x0008:	dma_.controllers[0].template write<0x8>(value);	break;
			case 0x0009:	dma_.controllers[0].template write<0x9>(value);	break;
			case 0x000a:	dma_.controllers[0].template write<0xa>(value);	break;
			case 0x000b:	dma_.controllers[0].template write<0xb>(value);	break;
			case 0x000c:	dma_.controllers[0].template write<0xc>(value);	break;
			case 0x000d:	dma_.controllers[0].template write<0xd>(value);	break;
			case 0x000e:	dma_.controllers[0].template write<0xe>(value);	break;
			case 0x000f:	dma_.controllers[0].template write<0xf>(value);	break;

			case 0x00c0:	if(require_at(port, value)) dma_.controllers[1].template write<0x0>(value);	break;
			case 0x00c2:	if(require_at(port, value)) dma_.controllers[1].template write<0x1>(value);	break;
			case 0x00c4:	if(require_at(port, value)) dma_.controllers[1].template write<0x2>(value);	break;
			case 0x00c6:	if(require_at(port, value)) dma_.controllers[1].template write<0x3>(value);	break;
			case 0x00c8:	if(require_at(port, value)) dma_.controllers[1].template write<0x4>(value);	break;
			case 0x00ca:	if(require_at(port, value)) dma_.controllers[1].template write<0x5>(value);	break;
			case 0x00cc:	if(require_at(port, value)) dma_.controllers[1].template write<0x6>(value);	break;
			case 0x00ce:	if(require_at(port, value)) dma_.controllers[1].template write<0x7>(value);	break;
			case 0x00d0:	if(require_at(port, value)) dma_.controllers[1].template write<0x8>(value);	break;
			case 0x00d2:	if(require_at(port, value)) dma_.controllers[1].template write<0x9>(value);	break;
			case 0x00d4:	if(require_at(port, value)) dma_.controllers[1].template write<0xa>(value);	break;
			case 0x00d6:	if(require_at(port, value)) dma_.controllers[1].template write<0xb>(value);	break;
			case 0x00d8:	if(require_at(port, value)) dma_.controllers[1].template write<0xc>(value);	break;
			case 0x00da:	if(require_at(port, value)) dma_.controllers[1].template write<0xd>(value);	break;
			case 0x00dc:	if(require_at(port, value)) dma_.controllers[1].template write<0xe>(value);	break;
			case 0x00de:	if(require_at(port, value)) dma_.controllers[1].template write<0xf>(value);	break;

			case 0x0020:	pics_.pic[0].template write<0>(value);	break;
			case 0x0021:	pics_.pic[0].template write<1>(value);	break;

			case 0x00a0:
				if constexpr (is_xt(model)) {
					// On the XT the NMI can be masked by setting bit 7 on I/O port 0xA0.
					log.error().append("TODO: NMIs %s", (value & 0x80) ? "masked" : "unmasked");
				} else {
					pics_.pic[1].template write<0>(value);
				}
			break;
			case 0x00a1:
				if(require_at(port, value)) pics_.pic[1].template write<1>(uint8_t(value));
			break;

			case 0x0040:	pit_.template write<0>(value);	break;
			case 0x0041:	pit_.template write<1>(value);	break;
			case 0x0042:	pit_.template write<2>(value);	break;
			case 0x0043:	pit_.set_mode(value);			break;

			case 0x0060:	case 0x0061:	case 0x0062:	case 0x0063:
			case 0x0064:	case 0x0065:	case 0x0066:	case 0x0067:
			case 0x0068:	case 0x0069:	case 0x006a:	case 0x006b:
			case 0x006c:	case 0x006d:	case 0x006e:	case 0x006f:
				if constexpr (is_xt(model)) {
					ppi_.write(port, value);
				} else {
					keyboard_.write(port, value);
				}
			break;

			case 0x0080:	dma_.pages.template set_page<0>(value);	break;
			case 0x0081:	dma_.pages.template set_page<1>(value);	break;
			case 0x0082:	dma_.pages.template set_page<2>(value);	break;
			case 0x0083:	dma_.pages.template set_page<3>(value);	break;
			case 0x0084:	dma_.pages.template set_page<4>(value);	break;
			case 0x0085:	dma_.pages.template set_page<5>(value);	break;
			case 0x0086:	dma_.pages.template set_page<6>(value);	break;
			case 0x0087:	dma_.pages.template set_page<7>(value);	break;

			case 0x0088:	if(require_at(port, value))	dma_.pages.template set_page<0x8>(value);	break;
			case 0x0089:	if(require_at(port, value))	dma_.pages.template set_page<0x9>(value);	break;
			case 0x008a:	if(require_at(port, value))	dma_.pages.template set_page<0xa>(value);	break;
			case 0x008b:	if(require_at(port, value))	dma_.pages.template set_page<0xb>(value);	break;
			case 0x008c:	if(require_at(port, value))	dma_.pages.template set_page<0xc>(value);	break;
			case 0x008d:	if(require_at(port, value))	dma_.pages.template set_page<0xd>(value);	break;
			case 0x008e:	if(require_at(port, value))	dma_.pages.template set_page<0xe>(value);	break;
			case 0x008f:	if(require_at(port, value))	dma_.pages.template set_page<0xf>(value);	break;

			//
			// CRTC access block, with slightly laboured 16-bit to 8-bit mapping.
			//
			case crtc_base + 0:		case crtc_base + 2:
			case crtc_base + 4:		case crtc_base + 6:
				video_.template write<0>(value);
			break;
			case crtc_base + 1:		case crtc_base + 3:
			case crtc_base + 5:		case crtc_base + 7:
				video_.template write<1>(value);
			break;

			case crtc_base + 0x8:	video_.template write<0x8>(value);	break;
			case crtc_base + 0x9:	video_.template write<0x9>(value);	break;

			case 0x03f2:
				fdc_.set_digital_output(value);
			break;
			case 0x03f4:
			case 0x03f7:
				fdc_.set_data_rate(value);
				log.error().append("TODO: FDC (or IDE?) write of %02x at %04x", value, port);
			break;
			case 0x03f5:
				fdc_.write(value);
			break;

			// IDE.
			case 0x01f1:	if(require_ide(port, value)) ide_.set_write_precompensation(value);	break;
			case 0x01f2:	if(require_ide(port, value)) ide_.set_sector_count(value);			break;
			case 0x01f3:	if(require_ide(port, value)) ide_.set_sector_number(value);			break;
			case 0x01f4:	if(require_ide(port, value)) ide_.set_cylinder_low(value);			break;
			case 0x01f5:	if(require_ide(port, value)) ide_.set_cylinder_high(value);			break;
			case 0x01f6:	if(require_ide(port, value)) ide_.set_drive_head(value);			break;
			case 0x01f7:	if(require_ide(port, value)) ide_.set_command(value);				break;

			case 0x03f6:	if(require_ide(port, value)) ide_.set_controller_data(value);		break;
//			case 0x03f7:	if(require_ide(port, value)) ide_.set_controller_status(value);		break;
		}
	}

public:
	template <typename IntT>
	requires std::same_as<IntT, uint16_t> || std::same_as<IntT, uint8_t>
	void out(const uint16_t port, const IntT value) {
		if constexpr (std::is_same_v<IntT, uint16_t>) {
			out16<false>(port, value);
		} else {
			out8<false>(port, value);
		}
	}

private:
	void log_unhandled(const uint16_t port) {
		if(wilfully_unimplemented(port)) {
			return;
		}
		log.error().append("Unhandled in: %04x", port);
	};

	bool require_at(const uint16_t port) {
		if constexpr (is_at(model)) {
			return true;
		}
		log_unhandled(port);
		return false;
	};

	bool require_ide(const uint16_t port) {
		if constexpr (has_ide(model)) {
			return true;
		}
		log_unhandled(port);
		return false;
	};

	template <bool is_retry>
	uint16_t in16(const uint16_t port) {
		switch(port) {
			default:		break;
			case 0x01f0:	if(require_ide(port)) return ide_.data();				break;
		}

		if constexpr (is_retry) {
			log_unhandled(port);
			return 0xffff;
		} else {
			return uint16_t(in8<true>(port) | (in8<true>(port + 1) << 8));
		}
	}

	template <bool is_retry>
	uint8_t in8(const uint16_t port) {
		switch(port) {
			default:		break;

			case 0x0000:	return dma_.controllers[0].template read<0x0>();
			case 0x0001:	return dma_.controllers[0].template read<0x1>();
			case 0x0002:	return dma_.controllers[0].template read<0x2>();
			case 0x0003:	return dma_.controllers[0].template read<0x3>();
			case 0x0004:	return dma_.controllers[0].template read<0x4>();
			case 0x0005:	return dma_.controllers[0].template read<0x5>();
			case 0x0006:	return dma_.controllers[0].template read<0x6>();
			case 0x0007:	return dma_.controllers[0].template read<0x7>();
			case 0x0008:	return dma_.controllers[0].template read<0x8>();
			case 0x000d:	return dma_.controllers[0].template read<0xd>();

			case 0x00c0:	if(require_at(port)) return dma_.controllers[1].template read<0x0>();	break;
			case 0x00c2:	if(require_at(port)) return dma_.controllers[1].template read<0x1>();	break;
			case 0x00c4:	if(require_at(port)) return dma_.controllers[1].template read<0x2>();	break;
			case 0x00c6:	if(require_at(port)) return dma_.controllers[1].template read<0x3>();	break;
			case 0x00c8:	if(require_at(port)) return dma_.controllers[1].template read<0x4>();	break;
			case 0x00ca:	if(require_at(port)) return dma_.controllers[1].template read<0x5>();	break;
			case 0x00cc:	if(require_at(port)) return dma_.controllers[1].template read<0x6>();	break;
			case 0x00ce:	if(require_at(port)) return dma_.controllers[1].template read<0x7>();	break;
			case 0x00d0:	if(require_at(port)) return dma_.controllers[1].template read<0x8>();	break;
			case 0x00d8:	if(require_at(port)) return dma_.controllers[1].template read<0xd>();	break;

			case 0x0009:	case 0x000b:
			case 0x000c:	case 0x000f:
				// DMA area, but it doesn't respond.
			break;

			case 0x0020:	return pics_.pic[0].template read<0>();
			case 0x0021:	return pics_.pic[0].template read<1>();
			case 0x00a0:	if(require_at(port)) return pics_.pic[1].template read<0>();	break;
			case 0x00a1:	if(require_at(port)) return pics_.pic[1].template read<1>();	break;

			case 0x0040:	return pit_.template read<0>();
			case 0x0041:	return pit_.template read<1>();
			case 0x0042:	return pit_.template read<2>();

			case 0x0060:	case 0x0061:	case 0x0062:	case 0x0063:
			case 0x0064:	case 0x0065:	case 0x0066:	case 0x0067:
			case 0x0068:	case 0x0069:	case 0x006a:	case 0x006b:
			case 0x006c:	case 0x006d:	case 0x006e:	case 0x006f:
				if constexpr (is_xt(model)) {
					return ppi_.read(port);
				} else {
					return keyboard_.read(port);
				}
			break;

			case 0x0071:	return rtc_.read();

			case 0x0080:	return dma_.pages.template page<0>();
			case 0x0081:	return dma_.pages.template page<1>();
			case 0x0082:	return dma_.pages.template page<2>();
			case 0x0083:	return dma_.pages.template page<3>();
			case 0x0084:	return dma_.pages.template page<4>();
			case 0x0085:	return dma_.pages.template page<5>();
			case 0x0086:	return dma_.pages.template page<6>();
			case 0x0087:	return dma_.pages.template page<7>();

			case 0x0088:	if(require_at(port)) return dma_.pages.template page<0x8>(); 	break;
			case 0x0089:	if(require_at(port)) return dma_.pages.template page<0x9>(); 	break;
			case 0x008a:	if(require_at(port)) return dma_.pages.template page<0xa>(); 	break;
			case 0x008b:	if(require_at(port)) return dma_.pages.template page<0xb>(); 	break;
			case 0x008c:	if(require_at(port)) return dma_.pages.template page<0xc>(); 	break;
			case 0x008d:	if(require_at(port)) return dma_.pages.template page<0xd>(); 	break;
			case 0x008e:	if(require_at(port)) return dma_.pages.template page<0xe>(); 	break;
			case 0x008f:	if(require_at(port)) return dma_.pages.template page<0xf>(); 	break;

			case 0x03f4:	return fdc_.status();
			case 0x03f5:	return fdc_.read();

			case 0x03b8:
				if constexpr (video == Target::VideoAdaptor::MDA) {
					return video_.template read<0x8>();
				}
			break;
			case 0x03ba:
				if constexpr (video == Target::VideoAdaptor::MDA) {
					return video_.template read<0xa>();
				}
			break;

			case 0x3da:
				if constexpr (video == Target::VideoAdaptor::CGA) {
					return video_.template read<0xa>();
				}
			break;

			// IDE.
			case 0x01f1:	if(require_ide(port)) return ide_.error();				break;
			case 0x01f2:	if(require_ide(port)) return ide_.sector_count();		break;
			case 0x01f3:	if(require_ide(port)) return ide_.sector_number();		break;
			case 0x01f4:	if(require_ide(port)) return ide_.cylinder_low();		break;
			case 0x01f5:	if(require_ide(port)) return ide_.cylinder_high();		break;
			case 0x01f6:	if(require_ide(port)) return ide_.drive_head();			break;
			case 0x01f7:	if(require_ide(port)) return ide_.status();				break;

			case 0x03f1:	return 0b01010000;	// "D/S/P DIAGNOSTIC REGISTER"?
												// This value is seemingly required by the PC AT BIOS, but is not
												// further documented by IBM or elsewhere.

			case 0x03f6:	if(require_ide(port)) return ide_.controller_data();	break;
			case 0x03f7:	if(require_ide(port)) return ide_.controller_status();	break;
		}

		if constexpr (is_retry) {
			log_unhandled(port);
			return 0xff;
		} else {
			return uint8_t(in16<true>(port));
		}
	}

public:
	template <typename IntT>
	requires std::same_as<IntT, uint16_t> || std::same_as<IntT, uint8_t>
	IntT in(const uint16_t port) {
		if constexpr (std::is_same_v<IntT, uint16_t>) {
			return in16<false>(port);
		} else {
			return in8<false>(port);
		}
	}

private:
	PIT<model> &pit_;
	DMA<model> &dma_;
	PPI<model> &ppi_;
	PICs<model> &pics_;
	typename Adaptor<video>::type &video_;
	FloppyController<model> &fdc_;
	KeyboardController<model> &keyboard_;
	RTC &rtc_;
	IDE ide_;
};

template <Analyser::Static::PCCompatible::Model model>
class FlowController {
	static constexpr auto x86_model = processor_model(model);

public:
	FlowController(InstructionSet::x86::Registers<x86_model> &registers, Segments<x86_model, LinearMemory<x86_model>> &segments) :
		registers_(registers), segments_(segments) {}

	// Requirements for perform.
	template <typename AddressT>
	requires std::same_as<AddressT, uint16_t>
	void jump(const AddressT address) {
		registers_.ip() = address;
	}

	template <typename AddressT>
	requires std::same_as<AddressT, uint16_t>
	void jump(const uint16_t segment, const AddressT address) {
		static constexpr auto cs = InstructionSet::x86::Source::CS;
		segments_.preauthorise(cs, segment);
		registers_.cs() = segment;
		segments_.did_update(cs);
		registers_.ip() = address;
	}

	void halt() {
		halted_ = true;
	}
	void wait() {
		log.error().append("WAIT ????");
	}

	void repeat_last() {
		should_repeat_ = true;
	}
	void cancel_repetition() {
		should_repeat_ = false;
	}

	// Other actions.
	void begin_instruction() {
		should_repeat_ = false;
	}
	bool should_repeat() const {
		return should_repeat_;
	}

	void unhalt() {
		halted_ = false;
	}
	bool halted() const {
		return halted_;
	}

private:
	InstructionSet::x86::Registers<x86_model> &registers_;
	Segments<x86_model, LinearMemory<x86_model>> &segments_;
	bool should_repeat_ = false;
	bool halted_ = false;
};

template <Analyser::Static::PCCompatible::Model pc_model, Target::VideoAdaptor video>
class ConcreteMachine:
	public Machine,
	public MachineTypes::TimedMachine,
	public MachineTypes::AudioProducer,
	public MachineTypes::MappedKeyboardMachine,
	public MachineTypes::MediaTarget,
	public MachineTypes::ScanProducer,
	public Activity::Source,
	public Configurable::Device
{
	static constexpr int DriveCount = 1;
	using Video = typename Adaptor<video>::type;

public:
	ConcreteMachine(
		const Analyser::Static::PCCompatible::Target &target,
		const ROMMachine::ROMFetcher &rom_fetcher
	) :
		keyboard_(pics_, speaker_, video),
		fdc_(pics_, dma_, DriveCount),
		pit_observer_(pics_, speaker_),
		ppi_handler_(speaker_, keyboard_, video, DriveCount),
		pit_(pit_observer_),
		ppi_(ppi_handler_),
		context_(pit_, dma_, ppi_, pics_, video_, fdc_, keyboard_, rtc_)
	{
		// Set up DMA source/target.
		dma_.set_memory(&context_.linear_memory);

		// Use clock rate as a MIPS count; keeping it as a multiple or divisor of the PIT frequency is easy.
		static constexpr int pit_frequency = 1'193'182;
		set_clock_rate(double(pit_frequency));
		speaker_.speaker.set_input_rate(double(pit_frequency));

		// Fetch the BIOS.
		const auto font = Video::FontROM;

		constexpr auto bios_XT = ROM::Name::PCCompatibleGLaBIOS;
		constexpr auto tick_XT = ROM::Name::PCCompatibleGLaTICK;

		constexpr auto bios_AT = ROM::Name::PCCompatibleIBMATBIOS;
		constexpr auto bios_AT_even = ROM::Name::PCCompatibleIBMATBIOSNov85U27;
		constexpr auto bios_AT_odd = ROM::Name::PCCompatibleIBMATBIOSNov85U47;
		constexpr auto bios_AT_Phoenix = ROM::Name::PCCompatiblePhoenix80286BIOS;

		constexpr auto rom_BASIC = ROM::Name::IBMBASIC110;

		ROM::Request request = ROM::Request(font);
		switch(pc_model) {
			default:
				request = request && ROM::Request(bios_XT) && ROM::Request(tick_XT);
			break;
			case Analyser::Static::PCCompatible::Model::AT: {
				auto at_even_odd = ROM::Request(bios_AT_odd) && ROM::Request(bios_AT_even);
				auto at_full = ROM::Request(bios_AT);
				auto phoenix_full = ROM::Request(bios_AT_Phoenix);
				const auto at_any = ((at_even_odd || at_full) && rom_BASIC) || phoenix_full;
				request = request && at_any;
			} break;
		}

		auto roms = rom_fetcher(request);
		if(!request.validate(roms)) {
			throw ROMMachine::Error::MissingROMs;
		}

		const auto install_bios = [&](const std::vector<uint8_t> &source) {
			context_.linear_memory.install(
				uint32_t(0x10'0000 - source.size()),
				source.data(),
				uint32_t(source.size())
			);
		};

		switch(pc_model) {
			default: {
				install_bios(roms.find(bios_XT)->second);

				// If found, install GlaTICK at 0xd'0000.
				const auto tick_contents = roms.find(tick_XT);
				if(tick_contents != roms.end()) {
					context_.linear_memory.install(
						0xd'0000,
						tick_contents->second.data(),
						uint32_t(tick_contents->second.size())
					);
				}
			} break;

			case Analyser::Static::PCCompatible::Model::AT: {
				const auto install_at = [&] {
					const auto bios_even = roms.find(bios_AT_even);
					const auto bios_odd = roms.find(bios_AT_odd);
					bool is_ibm = false;

					if(bios_even != roms.end() && bios_odd != roms.end()) {
						std::vector<uint8_t> bios(65536);

						for(size_t c = 0; c < bios.size(); c++) {
							bios[c] = (c & 1) ? bios_odd->second[c >> 1] : bios_even->second[c >> 1];
						}

						install_bios(bios);
						is_ibm = true;
					} else {
						for(const auto name: {bios_AT, bios_AT_Phoenix}) {
							const auto bios_contents = roms.find(name);
							if(bios_contents != roms.end()) {
								install_bios(bios_contents->second);
								is_ibm = name == bios_AT;
								break;
							}
						}
					}

					if(is_ibm) {
						const auto basic = roms.find(rom_BASIC);
						context_.linear_memory.install(
							0xf'6000,
							basic->second.data(),
							uint32_t(basic->second.size())
						);
					}
				};

				install_at();
			} break;
		}

		// Give the video card something to read from.
		const auto &font_contents = roms.find(font)->second;
		video_.set_source(context_.linear_memory.at(Video::BaseAddress), font_contents);

		// ... and insert media.
		insert_media(target.media);
	}

	~ConcreteMachine() {
		speaker_.queue.flush();
	}

	// MARK: - TimedMachine.
	using Exception = InstructionSet::x86::Exception;

	void run_for(const Cycles duration) final {
#ifndef NDEBUG
		static constexpr int SpeedMultiplier = 3;
#else
		static constexpr int SpeedMultiplier = 2;
#endif

		const auto pit_ticks = (duration.as<int>() * SpeedMultiplier) / 2;
		static constexpr int pit_multiplier = [] {
			switch(pc_model) {
				// This is implicitly treated as running at 1/3 the PIT clock = around 0.4 MIPS.
				// i.e. a shade more than 8086 speed, if MIPS were meaningful.
				case Analyser::Static::PCCompatible::Model::XT: return 1;

				// Other multipliers are CPU instructions per PIT clock.
				//
				// 2*PIT = around 2.4 MIPS, broadly 80286 speed, if MIPS were a valid measure.
				case Analyser::Static::PCCompatible::Model::TurboXT: return 2;

				case Analyser::Static::PCCompatible::Model::AT: return 2;	// TODO: increase if/when no longer timing
																			// dependent in BIOS boot (cf. test 11h,
																			// RAM refresh rate versus execution rate)
			}
			// Other inevitably broad and fuzzy and inconsistent MIPS counts for my own potential future play:
			//
			// 80386 @ 20Mhz: 4–5 MIPS.
			// 80486 @ 66Mhz: 25 MIPS.
			// Pentium @ 100Mhz: 188 MIPS.
		} ();

		int ticks;
		if constexpr (pit_multiplier > 1) {
			ticks = pit_ticks;
		} else {
			cpu_divisor_ += pit_ticks;
			ticks = cpu_divisor_ / 3;
			cpu_divisor_ %= 3;
		}

		while(ticks--) {
			//
			// First draft: all hardware runs in lockstep, as a multiple or divisor of the PIT frequency.
			//

			//
			// Advance the PIT and audio.
			//
			pit_.run_for(1);
			++speaker_.cycles_since_update;

			// For the slow speed, run the PIT multiple times per CPU tick.
			if constexpr (pit_multiplier == 1) {
				pit_.run_for(1);
				++speaker_.cycles_since_update;
				pit_.run_for(1);
				++speaker_.cycles_since_update;
			}

			//
			// Advance CRTC.
			//
			video_.run_for((pit_multiplier > 1) ? Cycles(1) : Cycles(3));

			//
			// Give the keyboard a notification of passing time; it's very approximately clocked,
			// really just including 'some' delays to avoid being instant.
			//
			keyboard_.run_for(Cycles(1));

			//
			// Perform one CPU instruction every three PIT cycles.
			// i.e. CPU instruction rate is 1/3 * ~1.19Mhz ~= 0.4 MIPS.
			//

			// Query for interrupts and apply if pending.
			// TODO: include the other PIC.
			if(pics_.pic[0].pending() && context_.flags.template flag<InstructionSet::x86::Flag::Interrupt>()) {
				// Regress the IP if a REP is in-progress so as to resume it later.
				if(context_.flow_controller.should_repeat()) {
					context_.registers.ip() = decoded_ip_;
					context_.flow_controller.begin_instruction();
				}

				// Signal interrupt.
				context_.flow_controller.unhalt();
				const auto interrupt_id = pics_.pic[0].acknowledge();
				InstructionSet::x86::fault(Exception::interrupt(interrupt_id), context_);

				if(should_log) {
					log.info().append("Taking interrupt vector %d", interrupt_id);
				}
			}

			// Do nothing if currently halted.
			if(context_.flow_controller.halted()) {
				continue;
			}

			for(int c = 0; c < pit_multiplier; c++) {
				perform_instruction();
			}
		}
	}

	void perform_instruction() {
		// Get the next thing to execute.
		if(!context_.flow_controller.should_repeat()) {
			// Decode from the current IP.
			decoded_ip_ = context_.registers.ip();
			const auto remainder = context_.memory.next_code();
			decoded_ = decoder_.decode(remainder.first, remainder.second);

			// If that didn't yield a whole instruction then the end of memory must have been hit;
			// continue from the beginning.
			if(decoded_.first <= 0) {
				const auto start = context_.memory.start_code();
				decoded_ = decoder_.decode(start.first, start.second);
			}

			context_.registers.ip() += decoded_.first;
		} else {
			context_.flow_controller.begin_instruction();
		}

//		if(decoded_ip_ >= 0x7c00 && decoded_ip_ < 0x7c00 + 1024) {
//		if(decoded_ip_ == 0x21d0) {
//			should_log = true;
//		}

		// Covers DISK_RESET.
//		should_log = (decoded_ip_ >= 0x21d0 && decoded_ip_ < 0x221c);

		if(should_log) {
			log.info().append(
				"%04x %s \t\t[ds:6Bh]:%02x",
					decoded_ip_,
					to_string(decoded_, InstructionSet::x86::Model::i80286).c_str(),
//					context_.registers.bl()
					context_.memory.template access<uint8_t, InstructionSet::x86::AccessType::PreauthorisedRead>(
						InstructionSet::x86::Source::DS,
						uint16_t(0x6b)
					)
			).append_if(decoded_.second.operation() == InstructionSet::x86::Operation::INT,
				" dl:%02x ah:%02x ch:%02x cl:%02x dh:%02x es:%04x bx:%04x",
					context_.registers.dl(),
					context_.registers.ah(),
					context_.registers.ch(),
					context_.registers.cl(),
					context_.registers.dh(),
					context_.registers.es(),
					context_.registers.bx()
			);
		}

		if(decoded_.second.operation() == InstructionSet::x86::Operation::Invalid) {
			log.error().append("Invalid operation");
		}

		// Execute it.
		InstructionSet::x86::perform(
			decoded_.second,
			context_,
			decoded_ip_
		);
	}

	// MARK: - ScanProducer.
	void set_scan_target(Outputs::Display::ScanTarget *const scan_target) final {
		video_.set_scan_target(scan_target);
	}
	Outputs::Display::ScanStatus get_scaled_scan_status() const final {
		return video_.get_scaled_scan_status();
	}

	// MARK: - AudioProducer.
	Outputs::Speaker::Speaker *get_speaker() final {
		return &speaker_.speaker;
	}

	void flush_output(const int outputs) final {
		if(outputs & Output::Audio) {
			speaker_.update();
			speaker_.queue.perform();
		}
	}

	// MARK: - MediaTarget
	bool insert_media(const Analyser::Static::Media &media) final {
		int c = 0;
		for(auto &disk : media.disks) {
			fdc_.set_disk(disk, c);
			c++;
			if(c == 4) break;
		}
		return true;
	}

	// MARK: - MappedKeyboardMachine.
	MappedKeyboardMachine::KeyboardMapper *get_keyboard_mapper() final {
		return &keyboard_mapper_;
	}

	void set_key_state(const uint16_t key, const bool is_pressed) final {
		keyboard_.post_keyboard(uint8_t(key | (is_pressed ? 0x00 : 0x80)));
	}

	// MARK: - Activity::Source.
	void set_activity_observer(Activity::Observer *const observer) final {
		fdc_.set_activity_observer(observer);
	}

	// MARK: - Configuration options.
	std::unique_ptr<Reflection::Struct> get_options() const final {
		auto options = std::make_unique<Options>(Configurable::OptionsType::UserFriendly);
		options->output = get_video_signal_configurable();
		return options;
	}

	void set_options(const std::unique_ptr<Reflection::Struct> &str) final {
		const auto options = dynamic_cast<Options *>(str.get());
		set_video_signal_configurable(options->output);
	}

	void set_display_type(const Outputs::Display::DisplayType display_type) final {
		video_.set_display_type(display_type);

		// Give the PPI a shout-out in case it isn't too late to switch to CGA40.
		ppi_handler_.hint_is_composite(Outputs::Display::is_composite(display_type));
	}

	Outputs::Display::DisplayType get_display_type() const final {
		return video_.get_display_type();
	}

private:
	static constexpr auto x86_model = processor_model(pc_model);

	Speaker speaker_;
	PICs<pc_model> pics_;
	DMA<pc_model> dma_;
	Video video_;

	KeyboardController<pc_model> keyboard_;
	FloppyController<pc_model> fdc_;
	PITObserver<pc_model> pit_observer_;
	i8255PortHandler<pc_model> ppi_handler_;

	PIT<pc_model> pit_;
	PPI<pc_model> ppi_;
	RTC rtc_;

	PCCompatible::KeyboardMapper keyboard_mapper_;

	struct Context {
		Context(
			PIT<pc_model> &pit,
			DMA<pc_model> &dma,
			PPI<pc_model> &ppi,
			PICs<pc_model> &pics,
			typename Adaptor<video>::type &card,
			FloppyController<pc_model> &fdc,
			KeyboardController<pc_model> &keyboard,
			RTC &rtc
		) :
			flags(x86_model),
			segments(registers, linear_memory),
			memory(registers, segments, linear_memory),
			flow_controller(registers, segments),
			cpu_control(registers, segments, memory, linear_memory),
			io(pit, dma, ppi, pics, card, fdc, keyboard, rtc)
		{
			keyboard.set_cpu_control(&cpu_control);
			reset();
		}

		void reset() {
			cpu_control.reset();
		}

		InstructionSet::x86::Flags flags;
		InstructionSet::x86::Registers<x86_model> registers;

		LinearMemory<x86_model> linear_memory;
		Segments<x86_model, LinearMemory<x86_model>> segments;
		SegmentedMemory<x86_model, LinearMemory<x86_model>> memory;

		FlowController<pc_model> flow_controller;
		CPUControl<pc_model> cpu_control;
		IO<pc_model, video> io;
		static constexpr auto model = processor_model(pc_model);
	} context_;

	using Decoder = InstructionSet::x86::Decoder<Context::model>;
	Decoder decoder_;

	uint16_t decoded_ip_ = 0;
	std::pair<int, typename Decoder::InstructionT> decoded_;

	int cpu_divisor_ = 0;
};

}

using namespace PCCompatible;

namespace {
#ifndef NDEBUG
static constexpr bool ForceAT = true;
#else
static constexpr bool ForceAT = false;
#endif

template <Target::VideoAdaptor video>
std::unique_ptr<Machine> machine(const Target &target, const ROMMachine::ROMFetcher &rom_fetcher) {
	using Model = Analyser::Static::PCCompatible::Model;
	switch(ForceAT ? Model::AT : target.model) {
		case Model::XT:
			return std::make_unique<PCCompatible::ConcreteMachine<Model::XT, video>>(target, rom_fetcher);

		case Model::TurboXT:
			return std::make_unique<PCCompatible::ConcreteMachine<Model::TurboXT, video>>(target, rom_fetcher);

		case Model::AT:
			return std::make_unique<PCCompatible::ConcreteMachine<Model::AT, video>>(target, rom_fetcher);
	}

	return nullptr;
}
}

std::unique_ptr<Machine> Machine::PCCompatible(
	const Analyser::Static::Target *target,
	const ROMMachine::ROMFetcher &rom_fetcher
) {
	const Target *const pc_target = dynamic_cast<const Target *>(target);
	using VideoAdaptor = Target::VideoAdaptor;
	switch(pc_target->adaptor) {
		case VideoAdaptor::MDA:	return machine<VideoAdaptor::MDA>(*pc_target, rom_fetcher);
		case VideoAdaptor::CGA:	return machine<VideoAdaptor::CGA>(*pc_target, rom_fetcher);
		default: return nullptr;
	}
}
