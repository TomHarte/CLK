//
//  PCCompatible.cpp
//  Clock Signal
//
//  Created by Thomas Harte on 15/11/2023.
//  Copyright © 2023 Thomas Harte. All rights reserved.
//

#include "PCCompatible.hpp"

#include "CGA.hpp"
#include "DMA.hpp"
#include "KeyboardMapper.hpp"
#include "MDA.hpp"
#include "Memory.hpp"
#include "PIC.hpp"
#include "PIT.hpp"
#include "RTC.hpp"

#include "../../InstructionSets/x86/Decoder.hpp"
#include "../../InstructionSets/x86/Flags.hpp"
#include "../../InstructionSets/x86/Instruction.hpp"
#include "../../InstructionSets/x86/Perform.hpp"

#include "../../Components/8255/i8255.hpp"
#include "../../Components/8272/CommandDecoder.hpp"
#include "../../Components/8272/Results.hpp"
#include "../../Components/8272/Status.hpp"
#include "../../Components/AudioToggle/AudioToggle.hpp"

#include "../../Numeric/RegisterSizes.hpp"

#include "../../Outputs/CRT/CRT.hpp"
#include "../../Outputs/Speaker/Implementation/LowpassSpeaker.hpp"

#include "../../Storage/Disk/Track/TrackSerialiser.hpp"
#include "../../Storage/Disk/Encodings/MFM/Constants.hpp"
#include "../../Storage/Disk/Encodings/MFM/SegmentParser.hpp"

#include "../AudioProducer.hpp"
#include "../KeyboardMachine.hpp"
#include "../MediaTarget.hpp"
#include "../ScanProducer.hpp"
#include "../TimedMachine.hpp"

#include "../../Analyser/Static/PCCompatible/Target.hpp"

#include <array>
#include <iostream>

namespace PCCompatible {

using VideoAdaptor = Analyser::Static::PCCompatible::Target::VideoAdaptor;

template <VideoAdaptor adaptor> struct Adaptor;
template <> struct Adaptor<VideoAdaptor::MDA> {
	using type = MDA;
};
template <> struct Adaptor<VideoAdaptor::CGA> {
	using type = CGA;
};

class FloppyController {
	public:
		FloppyController(PIC &pic, DMA &dma, int drive_count) : pic_(pic), dma_(dma) {
			// Default: one floppy drive only.
			for(int c = 0; c < 4; c++) {
				drives_[c].exists = drive_count > c;
			}
		}

		void set_digital_output(uint8_t control) {
			// b7, b6, b5, b4: enable motor for drive 4, 3, 2, 1;
			// b3: 1 => enable DMA; 0 => disable;
			// b2: 1 => enable FDC; 0 => hold at reset;
			// b1, b0: drive select (usurps FDC?)

			drives_[0].motor = control & 0x10;
			drives_[1].motor = control & 0x20;
			drives_[2].motor = control & 0x40;
			drives_[3].motor = control & 0x80;

			if(observer_) {
				for(int c = 0; c < 4; c++) {
					if(drives_[c].exists) observer_->set_led_status(drive_name(c), drives_[c].motor);
				}
			}

			enable_dma_ = control & 0x08;

			const bool hold_reset = !(control & 0x04);
			if(!hold_reset && hold_reset_) {
				// TODO: add a delay mechanism.
				reset();
			}
			hold_reset_ = hold_reset;
			if(hold_reset_) {
				pic_.apply_edge<6>(false);
			}
		}

		uint8_t status() const {
			return status_.main();
		}

		void write(uint8_t value) {
			decoder_.push_back(value);

			if(decoder_.has_command()) {
				using Command = Intel::i8272::Command;
				switch(decoder_.command()) {
					default:
						printf("TODO: implement FDC command %d\n", uint8_t(decoder_.command()));
					break;

					case Command::ReadData: {
						printf("FDC: Read from drive %d / head %d / track %d of head %d / track %d / sector %d\n",
							decoder_.target().drive,
							decoder_.target().head,
							drives_[decoder_.target().drive].track,
							decoder_.geometry().head,
							decoder_.geometry().cylinder,
							decoder_.geometry().sector);

						status_.begin(decoder_);

						// Search for a matching sector.
						auto target = decoder_.geometry();
						bool complete = false;
						while(!complete) {
							bool found_sector = false;

							for(auto &pair: drives_[decoder_.target().drive].sectors(decoder_.target().head)) {
								if(
									(pair.second.address.track == target.cylinder) &&
									(pair.second.address.sector == target.sector) &&
									(pair.second.address.side == target.head) &&
									(pair.second.size == target.size)
								) {
									found_sector = true;
									bool wrote_in_full = true;

									for(int c = 0; c < 128 << target.size; c++) {
										const auto access_result = dma_.write(2, pair.second.samples[0].data()[c]);
										switch(access_result) {
											default: break;
											case AccessResult::NotAccepted:
												complete = true;
												wrote_in_full = false;
											break;
											case AccessResult::AcceptedWithEOP:
												complete = true;
											break;
										}
										if(access_result != AccessResult::Accepted) {
											break;
										}
									}

									if(!wrote_in_full) {
										status_.set(Intel::i8272::Status1::OverRun);
										status_.set(Intel::i8272::Status0::AbnormalTermination);
										break;
									}

									++target.sector;	// TODO: multitrack?

									break;
								}
							}

							if(!found_sector) {
								status_.set(Intel::i8272::Status1::EndOfCylinder);
								status_.set(Intel::i8272::Status0::AbnormalTermination);
								break;
							}
						}

						results_.serialise(
							status_,
							decoder_.geometry().cylinder,
							decoder_.geometry().head,
							decoder_.geometry().sector,
							decoder_.geometry().size);

						// TODO: what if head has changed?
						drives_[decoder_.target().drive].status = decoder_.drive_head();
						drives_[decoder_.target().drive].raised_interrupt = true;
						pic_.apply_edge<6>(true);
					} break;

					case Command::Recalibrate:
						drives_[decoder_.target().drive].track = 0;

						drives_[decoder_.target().drive].raised_interrupt = true;
						drives_[decoder_.target().drive].status = decoder_.target().drive | uint8_t(Intel::i8272::Status0::SeekEnded);
						pic_.apply_edge<6>(true);
					break;
					case Command::Seek:
						drives_[decoder_.target().drive].track = decoder_.seek_target();

						drives_[decoder_.target().drive].raised_interrupt = true;
						drives_[decoder_.target().drive].status = decoder_.drive_head() | uint8_t(Intel::i8272::Status0::SeekEnded);
						pic_.apply_edge<6>(true);
					break;

					case Command::SenseInterruptStatus: {
						int c = 0;
						for(; c < 4; c++) {
							if(drives_[c].raised_interrupt) {
								drives_[c].raised_interrupt = false;
								status_.set_status0(drives_[c].status);
								results_.serialise(status_, drives_[0].track);
							}
						}

						bool any_remaining_interrupts = false;
						for(; c < 4; c++) {
							any_remaining_interrupts |= drives_[c].raised_interrupt;
						}
						if(!any_remaining_interrupts) {
							pic_.apply_edge<6>(false);
						}
					} break;
					case Command::Specify:
						specify_specs_ = decoder_.specify_specs();
					break;
//					case Command::SenseDriveStatus: {
//					} break;

					case Command::Invalid:
						results_.serialise_none();
					break;
				}

				decoder_.clear();

				// If there are any results to provide, set data direction and data ready.
				if(!results_.empty()) {
					using MainStatus = Intel::i8272::MainStatus;
					status_.set(MainStatus::DataIsToProcessor, true);
					status_.set(MainStatus::DataReady, true);
					status_.set(MainStatus::CommandInProgress, true);
				}
			}
		}

		uint8_t read() {
			using MainStatus = Intel::i8272::MainStatus;
			if(status_.get(MainStatus::DataIsToProcessor)) {
				const uint8_t result = results_.next();
				if(results_.empty()) {
					status_.set(MainStatus::DataIsToProcessor, false);
					status_.set(MainStatus::CommandInProgress, false);
				}
				return result;
			}

			return 0x80;
		}

		void set_activity_observer(Activity::Observer *observer) {
			observer_ = observer;
			for(int c = 0; c < 4; c++) {
				if(drives_[c].exists) {
					observer_->register_led(drive_name(c), 0);
				}
			}
		}

		void set_disk(std::shared_ptr<Storage::Disk::Disk> disk, int drive) {
//			if(drives_[drive].has_disk()) {
//				// TODO: drive should only transition to unready if it was ready in the first place.
//				drives_[drive].status = uint8_t(Intel::i8272::Status0::BecameNotReady);
//				drives_[drive].raised_interrupt = true;
//				pic_.apply_edge<6>(true);
//			}
			drives_[drive].set_disk(disk);
		}

	private:
		void reset() {
			printf("FDC reset\n");
			decoder_.clear();
			status_.reset();

			// Necessary to pass GlaBIOS' POST test, but: why?
			//
			// Cf. INT_13_0_2 and the CMP	AL, 11000000B following a CALL	FDC_WAIT_SENSE.
			for(int c = 0; c < 4; c++) {
				drives_[c].raised_interrupt = true;
				drives_[c].status = uint8_t(Intel::i8272::Status0::BecameNotReady);
			}
			pic_.apply_edge<6>(true);

			using MainStatus = Intel::i8272::MainStatus;
			status_.set(MainStatus::DataReady, true);
			status_.set(MainStatus::DataIsToProcessor, false);
		}

		PIC &pic_;
		DMA &dma_;

		bool hold_reset_ = false;
		bool enable_dma_ = false;

		Intel::i8272::CommandDecoder decoder_;
		Intel::i8272::Status status_;
		Intel::i8272::Results results_;

		Intel::i8272::CommandDecoder::SpecifySpecs specify_specs_;
		struct DriveStatus {
			public:
				bool raised_interrupt = false;
				uint8_t status = 0;
				uint8_t track = 0;
				bool motor = false;
				bool exists = true;

				bool has_disk() const {
					return bool(disk);
				}

				void set_disk(std::shared_ptr<Storage::Disk::Disk> image) {
					disk = image;
					cached.clear();
				}

				Storage::Encodings::MFM::SectorMap &sectors(bool side) {
					if(cached.track == track && cached.side == side) {
						return cached.sectors;
					}

					cached.track = track;
					cached.side = side;
					cached.sectors.clear();

					if(!disk) {
						return cached.sectors;
					}

					auto raw_track = disk->get_track_at_position(
						Storage::Disk::Track::Address(
							side,
							Storage::Disk::HeadPosition(track)
						)
					);
					if(!raw_track) {
						return cached.sectors;
					}

					const bool is_double_density = true;	// TODO: use MFM flag here.
					auto serialisation = Storage::Disk::track_serialisation(
						*raw_track,
						is_double_density ? Storage::Encodings::MFM::MFMBitLength : Storage::Encodings::MFM::FMBitLength
					);
					cached.sectors = Storage::Encodings::MFM::sectors_from_segment(std::move(serialisation), is_double_density);
					return cached.sectors;
				}

			private:
				struct {
					uint8_t track = 0xff;
					bool side;
					Storage::Encodings::MFM::SectorMap sectors;

					void clear() {
						track = 0xff;
						sectors.clear();
					}
				} cached;
				std::shared_ptr<Storage::Disk::Disk> disk;

		} drives_[4];

		static std::string drive_name(int c) {
			char name[3] = "A";
			name[0] += c;
			return std::string("Drive ") + name;
		}

		Activity::Observer *observer_ = nullptr;
};

class KeyboardController {
	public:
		KeyboardController(PIC &pic) : pic_(pic) {}

		// KB Status Port 61h high bits:
		//; 01 - normal operation. wait for keypress, when one comes in,
		//;		force data line low (forcing keyboard to buffer additional
		//;		keypresses) and raise IRQ1 high
		//; 11 - stop forcing data line low. lower IRQ1 and don't raise it again.
		//;		drop all incoming keypresses on the floor.
		//; 10 - lower IRQ1 and force clock line low, resetting keyboard
		//; 00 - force clock line low, resetting keyboard, but on a 01->00 transition,
		//;		IRQ1 would remain high
		void set_mode(uint8_t mode) {
			mode_ = Mode(mode);
			switch(mode_) {
				case Mode::NormalOperation: 	break;
				case Mode::NoIRQsIgnoreInput:
					pic_.apply_edge<1>(false);
				break;
				case Mode::ClearIRQReset:
					pic_.apply_edge<1>(false);
					[[fallthrough]];
				case Mode::Reset:
					reset_delay_ = 5;	// Arbitrarily.
				break;
			}
		}

		void run_for(Cycles cycles) {
			if(reset_delay_ <= 0) {
				return;
			}
			reset_delay_ -= cycles.as<int>();
			if(reset_delay_ <= 0) {
				input_.clear();
				post(0xaa);
			}
		}

		uint8_t read() {
			pic_.apply_edge<1>(false);
			if(input_.empty()) {
				return 0;
			}

			const uint8_t key = input_.front();
			input_.erase(input_.begin());
			if(!input_.empty()) {
				pic_.apply_edge<1>(true);
			}
			return key;
		}

		void post(uint8_t value) {
			if(mode_ == Mode::NoIRQsIgnoreInput) {
				return;
			}
			input_.push_back(value);
			pic_.apply_edge<1>(true);
		}

	private:
		enum class Mode {
			NormalOperation = 0b01,
			NoIRQsIgnoreInput = 0b11,
			ClearIRQReset = 0b10,
			Reset = 0b00,
		} mode_;

		std::vector<uint8_t> input_;
		PIC &pic_;

		int reset_delay_ = 0;
};

struct PCSpeaker {
	PCSpeaker() :
		toggle(queue),
		speaker(toggle) {}

	void update() {
		speaker.run_for(queue, cycles_since_update);
		cycles_since_update = 0;
	}

	void set_pit(bool pit_input) {
		pit_input_ = pit_input;
		set_level();
	}

	void set_control(bool pit_mask, bool level) {
		pit_mask_ = pit_mask;
		level_ = level;
		set_level();
	}

	void set_level() {
		// TODO: I think pit_mask_ actually acts as the gate input to the PIT.
		const bool new_output = (!pit_mask_ | pit_input_) & level_;

		if(new_output != output_) {
			update();
			toggle.set_output(new_output);
			output_ = new_output;
		}
	}

	Concurrency::AsyncTaskQueue<false> queue;
	Audio::Toggle toggle;
	Outputs::Speaker::PullLowpass<Audio::Toggle> speaker;
	Cycles cycles_since_update = 0;

	bool pit_input_ = false;
	bool pit_mask_ = false;
	bool level_ = false;
	bool output_ = false;
};

class PITObserver {
	public:
		PITObserver(PIC &pic, PCSpeaker &speaker) : pic_(pic), speaker_(speaker) {}

		template <int channel>
		void update_output(bool new_level) {
			switch(channel) {
				default: break;
				case 0: pic_.apply_edge<0>(new_level);	break;
				case 2: speaker_.set_pit(new_level);	break;
			}
		}

	private:
		PIC &pic_;
		PCSpeaker &speaker_;

	// TODO:
	//
	//	channel 0 is connected to IRQ 0;
	//	channel 1 is used for DRAM refresh (presumably connected to DMA?);
	//	channel 2 is gated by a PPI output and feeds into the speaker.
};
using PIT = i8253<false, PITObserver>;

class i8255PortHandler : public Intel::i8255::PortHandler {
	public:
		i8255PortHandler(PCSpeaker &speaker, KeyboardController &keyboard, VideoAdaptor adaptor, int drive_count) :
			speaker_(speaker), keyboard_(keyboard) {
				// High switches:
				//
				// b3, b2: drive count; 00 = 1, 01 = 2, etc
				// b1, b0: video mode (00 = ROM; 01 = CGA40; 10 = CGA80; 11 = MDA)
				switch(adaptor) {
					default: break;
					case VideoAdaptor::MDA:	high_switches_ |= 0b11;	break;
					case VideoAdaptor::CGA:	high_switches_ |= 0b10;	break;	// Assume 80 columns.
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
		void hint_is_composite(bool composite) {
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

		void set_value(int port, uint8_t value) {
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

		uint8_t get_value(int port) {
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
		PCSpeaker &speaker_;
		KeyboardController &keyboard_;

		bool enable_keyboard_ = false;
};
using PPI = Intel::i8255::i8255<i8255PortHandler>;

template <VideoAdaptor video>
class IO {
	public:
		IO(PIT &pit, DMA &dma, PPI &ppi, PIC &pic, typename Adaptor<video>::type &card, FloppyController &fdc, RTC &rtc) :
			pit_(pit), dma_(dma), ppi_(ppi), pic_(pic), video_(card), fdc_(fdc), rtc_(rtc) {}

		template <typename IntT> void out(uint16_t port, IntT value) {
			static constexpr uint16_t crtc_base =
				video == VideoAdaptor::MDA ? 0x03b0 : 0x03d0;

			switch(port) {
				default:
					if constexpr (std::is_same_v<IntT, uint8_t>) {
						printf("Unhandled out: %02x to %04x\n", value, port);
					} else {
						printf("Unhandled out: %04x to %04x\n", value, port);
					}
				break;

				case 0x0070:	rtc_.write<0>(uint8_t(value));	break;
				case 0x0071:	rtc_.write<1>(uint8_t(value));	break;

				// On the XT the NMI can be masked by setting bit 7 on I/O port 0xA0.
				case 0x00a0:
					printf("TODO: NMIs %s\n", (value & 0x80) ? "masked" : "unmasked");
				break;

				case 0x0000:	dma_.controller.write<0x0>(uint8_t(value));		break;
				case 0x0001:	dma_.controller.write<0x1>(uint8_t(value));		break;
				case 0x0002:	dma_.controller.write<0x2>(uint8_t(value));		break;
				case 0x0003:	dma_.controller.write<0x3>(uint8_t(value));		break;
				case 0x0004:	dma_.controller.write<0x4>(uint8_t(value));		break;
				case 0x0005:	dma_.controller.write<0x5>(uint8_t(value));		break;
				case 0x0006:	dma_.controller.write<0x6>(uint8_t(value));		break;
				case 0x0007:	dma_.controller.write<0x7>(uint8_t(value));		break;
				case 0x0008:	dma_.controller.write<0x8>(uint8_t(value));		break;
				case 0x0009:	dma_.controller.write<0x9>(uint8_t(value));		break;
				case 0x000a:	dma_.controller.write<0xa>(uint8_t(value));		break;
				case 0x000b:	dma_.controller.write<0xb>(uint8_t(value));		break;
				case 0x000c:	dma_.controller.write<0xc>(uint8_t(value));		break;
				case 0x000d:	dma_.controller.write<0xd>(uint8_t(value));		break;
				case 0x000e:	dma_.controller.write<0xe>(uint8_t(value));		break;
				case 0x000f:	dma_.controller.write<0xf>(uint8_t(value));		break;

				case 0x0020:	pic_.write<0>(uint8_t(value));	break;
				case 0x0021:	pic_.write<1>(uint8_t(value));	break;

				case 0x0040:	pit_.write<0>(uint8_t(value));	break;
				case 0x0041:	pit_.write<1>(uint8_t(value));	break;
				case 0x0042:	pit_.write<2>(uint8_t(value));	break;
				case 0x0043:	pit_.set_mode(uint8_t(value));	break;

				case 0x0060:	case 0x0061:	case 0x0062:	case 0x0063:
				case 0x0064:	case 0x0065:	case 0x0066:	case 0x0067:
				case 0x0068:	case 0x0069:	case 0x006a:	case 0x006b:
				case 0x006c:	case 0x006d:	case 0x006e:	case 0x006f:
					ppi_.write(port, uint8_t(value));
				break;

				case 0x0080:	dma_.pages.set_page<0>(uint8_t(value));	break;
				case 0x0081:	dma_.pages.set_page<1>(uint8_t(value));	break;
				case 0x0082:	dma_.pages.set_page<2>(uint8_t(value));	break;
				case 0x0083:	dma_.pages.set_page<3>(uint8_t(value));	break;
				case 0x0084:	dma_.pages.set_page<4>(uint8_t(value));	break;
				case 0x0085:	dma_.pages.set_page<5>(uint8_t(value));	break;
				case 0x0086:	dma_.pages.set_page<6>(uint8_t(value));	break;
				case 0x0087:	dma_.pages.set_page<7>(uint8_t(value));	break;

				//
				// CRTC access block, with slightly laboured 16-bit to 8-bit mapping.
				//
				case crtc_base + 0:		case crtc_base + 2:
				case crtc_base + 4:		case crtc_base + 6:
					if constexpr (std::is_same_v<IntT, uint16_t>) {
						video_.template write<0>(uint8_t(value));
						video_.template write<1>(uint8_t(value >> 8));
					} else {
						video_.template write<0>(value);
					}
				break;
				case crtc_base + 1:		case crtc_base + 3:
				case crtc_base + 5:		case crtc_base + 7:
					if constexpr (std::is_same_v<IntT, uint16_t>) {
						video_.template write<1>(uint8_t(value));
						video_.template write<0>(uint8_t(value >> 8));
					} else {
						video_.template write<1>(value);
					}
				break;

				case crtc_base + 0x8:	video_.template write<0x8>(uint8_t(value));	break;
				case crtc_base + 0x9:	video_.template write<0x9>(uint8_t(value));	break;

				case 0x03f2:
					fdc_.set_digital_output(uint8_t(value));
				break;
				case 0x03f4:
					printf("TODO: FDC write of %02x at %04x\n", value, port);
				break;
				case 0x03f5:
					fdc_.write(uint8_t(value));
				break;

				case 0x0278:	case 0x0279:	case 0x027a:
				case 0x0378:	case 0x0379:	case 0x037a:
				case 0x03bc:	case 0x03bd:	case 0x03be:
					// Ignore parallel port accesses.
				break;

				case 0x02e8:	case 0x02e9:	case 0x02ea:	case 0x02eb:
				case 0x02ec:	case 0x02ed:	case 0x02ee:	case 0x02ef:
				case 0x02f8:	case 0x02f9:	case 0x02fa:	case 0x02fb:
				case 0x02fc:	case 0x02fd:	case 0x02fe:	case 0x02ff:
				case 0x03e8:	case 0x03e9:	case 0x03ea:	case 0x03eb:
				case 0x03ec:	case 0x03ed:	case 0x03ee:	case 0x03ef:
				case 0x03f8:	case 0x03f9:	case 0x03fa:	case 0x03fb:
				case 0x03fc:	case 0x03fd:	case 0x03fe:	case 0x03ff:
					// Ignore serial port accesses.
				break;
			}
		}
		template <typename IntT> IntT in([[maybe_unused]] uint16_t port) {
			switch(port) {
				default:
					printf("Unhandled in: %04x\n", port);
				break;

				case 0x0000:	return dma_.controller.read<0x0>();
				case 0x0001:	return dma_.controller.read<0x1>();
				case 0x0002:	return dma_.controller.read<0x2>();
				case 0x0003:	return dma_.controller.read<0x3>();
				case 0x0004:	return dma_.controller.read<0x4>();
				case 0x0005:	return dma_.controller.read<0x5>();
				case 0x0006:	return dma_.controller.read<0x6>();
				case 0x0007:	return dma_.controller.read<0x7>();
				case 0x0008:	return dma_.controller.read<0x8>();
				case 0x000d:	return dma_.controller.read<0xd>();

				case 0x0009:	case 0x000b:
				case 0x000c:	case 0x000f:
					// DMA area, but it doesn't respond.
				break;

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

				case 0x0071:	return rtc_.read();

				case 0x0080:	return dma_.pages.page<0>();
				case 0x0081:	return dma_.pages.page<1>();
				case 0x0082:	return dma_.pages.page<2>();
				case 0x0083:	return dma_.pages.page<3>();
				case 0x0084:	return dma_.pages.page<4>();
				case 0x0085:	return dma_.pages.page<5>();
				case 0x0086:	return dma_.pages.page<6>();
				case 0x0087:	return dma_.pages.page<7>();

				case 0x0201:	break;		// Ignore game port.

				case 0x0278:	case 0x0279:	case 0x027a:
				case 0x0378:	case 0x0379:	case 0x037a:
				case 0x03bc:	case 0x03bd:	case 0x03be:
					// Ignore parallel port accesses.
				break;

				case 0x03f4:	return fdc_.status();
				case 0x03f5:	return fdc_.read();

				case 0x03b8:
					if constexpr (video == VideoAdaptor::MDA) {
						return video_.template read<0x8>();
					}
				break;

				case 0x3da:
					if constexpr (video == VideoAdaptor::CGA) {
						return video_.template read<0xa>();
					}
				break;

				case 0x02e8:	case 0x02e9:	case 0x02ea:	case 0x02eb:
				case 0x02ec:	case 0x02ed:	case 0x02ee:	case 0x02ef:
				case 0x02f8:	case 0x02f9:	case 0x02fa:	case 0x02fb:
				case 0x02fc:	case 0x02fd:	case 0x02fe:	case 0x02ff:
				case 0x03e8:	case 0x03e9:	case 0x03ea:	case 0x03eb:
				case 0x03ec:	case 0x03ed:	case 0x03ee:	case 0x03ef:
				case 0x03f8:	case 0x03f9:	case 0x03fa:	case 0x03fb:
				case 0x03fc:	case 0x03fd:	case 0x03fe:	case 0x03ff:
					// Ignore serial port accesses.
				break;
			}
			return 0xff;
		}

	private:
		PIT &pit_;
		DMA &dma_;
		PPI &ppi_;
		PIC &pic_;
		typename Adaptor<video>::type &video_;
		FloppyController &fdc_;
		RTC &rtc_;
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

		void halt() {
			halted_ = true;
		}
		void wait() {
			printf("WAIT ????\n");
		}

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

		void unhalt() {
			halted_ = false;
		}
		bool halted() const {
			return halted_;
		}

	private:
		Registers &registers_;
		Segments &segments_;
		bool should_repeat_ = false;
		bool halted_ = false;
};

template <VideoAdaptor video>
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
			keyboard_(pic_),
			fdc_(pic_, dma_, DriveCount),
			pit_observer_(pic_, speaker_),
			ppi_handler_(speaker_, keyboard_, video, DriveCount),
			pit_(pit_observer_),
			ppi_(ppi_handler_),
			context(pit_, dma_, ppi_, pic_, video_, fdc_, rtc_)
		{
			// Set up DMA source/target.
			dma_.set_memory(&context.memory);

			// Use clock rate as a MIPS count; keeping it as a multiple or divisor of the PIT frequency is easy.
			static constexpr int pit_frequency = 1'193'182;
			set_clock_rate(double(pit_frequency));
			speaker_.speaker.set_input_rate(double(pit_frequency));

			// Fetch the BIOS. [8088 only, for now]
			const auto bios = ROM::Name::PCCompatibleGLaBIOS;
			const auto tick = ROM::Name::PCCompatibleGLaTICK;
			const auto font = Video::FontROM;

			ROM::Request request = ROM::Request(bios) && ROM::Request(tick) && ROM::Request(font);
			auto roms = rom_fetcher(request);
			if(!request.validate(roms)) {
				throw ROMMachine::Error::MissingROMs;
			}

			const auto &bios_contents = roms.find(bios)->second;
			context.memory.install(0x10'0000 - bios_contents.size(), bios_contents.data(), bios_contents.size());

			const auto &tick_contents = roms.find(tick)->second;
			context.memory.install(0xd'0000, tick_contents.data(), tick_contents.size());

			// Give the video card something to read from.
			const auto &font_contents = roms.find(font)->second;
			video_.set_source(context.memory.at(Video::BaseAddress), font_contents);

			// ... and insert media.
			insert_media(target.media);
		}

		~ConcreteMachine() {
			speaker_.queue.flush();
		}

		// MARK: - TimedMachine.
		void run_for(const Cycles duration) override {
			const auto pit_ticks = duration.as_integral();
			cpu_divisor_ += pit_ticks;
			int ticks = cpu_divisor_ / 3;
			cpu_divisor_ %= 3;

			while(ticks--) {
				//
				// First draft: all hardware runs in lockstep, as a multiple or divisor of the PIT frequency.
				//

				//
				// Advance the PIT and audio.
				//
				pit_.run_for(1);
				++speaker_.cycles_since_update;
				pit_.run_for(1);
				++speaker_.cycles_since_update;
				pit_.run_for(1);
				++speaker_.cycles_since_update;

				//
				// Advance CRTC at a more approximate rate.
				//
				video_.run_for(Cycles(3));

				//
				// Perform one CPU instruction every three PIT cycles.
				// i.e. CPU instruction rate is 1/3 * ~1.19Mhz ~= 0.4 MIPS.
				//

				keyboard_.run_for(Cycles(1));

				// Query for interrupts and apply if pending.
				if(pic_.pending() && context.flags.template flag<InstructionSet::x86::Flag::Interrupt>()) {
					// Regress the IP if a REP is in-progress so as to resume it later.
					if(context.flow_controller.should_repeat()) {
						context.registers.ip() = decoded_ip_;
						context.flow_controller.begin_instruction();
					}

					// Signal interrupt.
					context.flow_controller.unhalt();
					InstructionSet::x86::interrupt(
						pic_.acknowledge(),
						context
					);
				}

				// Do nothing if halted.
				if(context.flow_controller.halted()) {
					continue;
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
				} else {
					context.flow_controller.begin_instruction();
				}

/*				if(decoded_ip_ >= 0x7c00 && decoded_ip_ < 0x7c00 + 1024) {
					const auto next = to_string(decoded, InstructionSet::x86::Model::i8086);
//					if(next != previous) {
						std::cout << std::hex << decoded_ip_ << " " << next;

						if(decoded.second.operation() == InstructionSet::x86::Operation::INT) {
							std::cout << " dl:" << std::hex << +context.registers.dl() << "; ";
							std::cout << "ah:" << std::hex << +context.registers.ah() << "; ";
							std::cout << "ch:" << std::hex << +context.registers.ch() << "; ";
							std::cout << "cl:" << std::hex << +context.registers.cl() << "; ";
							std::cout << "dh:" << std::hex << +context.registers.dh() << "; ";
							std::cout << "es:" << std::hex << +context.registers.es() << "; ";
							std::cout << "bx:" << std::hex << +context.registers.bx();
						}

						std::cout << std::endl;
//						previous = next;
//					}
				}*/

				// Execute it.
				InstructionSet::x86::perform(
					decoded.second,
					context
				);
			}
		}

		// MARK: - ScanProducer.
		void set_scan_target(Outputs::Display::ScanTarget *scan_target) override {
			video_.set_scan_target(scan_target);
		}
		Outputs::Display::ScanStatus get_scaled_scan_status() const override {
			return video_.get_scaled_scan_status();
		}

		// MARK: - AudioProducer.
		Outputs::Speaker::Speaker *get_speaker() override {
			return &speaker_.speaker;
		}

		void flush_output(int outputs) final {
			if(outputs & Output::Audio) {
				speaker_.update();
				speaker_.queue.perform();
			}
		}

		// MARK: - MediaTarget
		bool insert_media(const Analyser::Static::Media &media) override {
			int c = 0;
			for(auto &disk : media.disks) {
				fdc_.set_disk(disk, c);
				c++;
				if(c == 4) break;
			}
			return true;
		}

		// MARK: - MappedKeyboardMachine.
		MappedKeyboardMachine::KeyboardMapper *get_keyboard_mapper() override {
			return &keyboard_mapper_;
		}

		void set_key_state(uint16_t key, bool is_pressed) override {
			keyboard_.post(uint8_t(key | (is_pressed ? 0x00 : 0x80)));
		}

		// MARK: - Activity::Source.
		void set_activity_observer(Activity::Observer *observer) final {
			fdc_.set_activity_observer(observer);
		}

		// MARK: - Configuration options.
		std::unique_ptr<Reflection::Struct> get_options() override {
			auto options = std::make_unique<Options>(Configurable::OptionsType::UserFriendly);
			options->output = get_video_signal_configurable();
			return options;
		}

		void set_options(const std::unique_ptr<Reflection::Struct> &str) override {
			const auto options = dynamic_cast<Options *>(str.get());
			set_video_signal_configurable(options->output);
		}

		void set_display_type(Outputs::Display::DisplayType display_type) override {
			video_.set_display_type(display_type);
			ppi_handler_.hint_is_composite(
				(display_type == Outputs::Display::DisplayType::CompositeColour) ||
				(display_type == Outputs::Display::DisplayType::CompositeMonochrome)
			);
		}

		Outputs::Display::DisplayType get_display_type() const override {
			return video_.get_display_type();
		}

	private:
		PIC pic_;
		DMA dma_;
		PCSpeaker speaker_;
		Video video_;

		KeyboardController keyboard_;
		FloppyController fdc_;
		PITObserver pit_observer_;
		i8255PortHandler ppi_handler_;

		PIT pit_;
		PPI ppi_;
		RTC rtc_;

		PCCompatible::KeyboardMapper keyboard_mapper_;

		struct Context {
			Context(PIT &pit, DMA &dma, PPI &ppi, PIC &pic, typename Adaptor<video>::type &card, FloppyController &fdc, RTC &rtc) :
				segments(registers),
				memory(registers, segments),
				flow_controller(registers, segments),
				io(pit, dma, ppi, pic, card, fdc, rtc)
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
			IO<video> io;
			static constexpr auto model = InstructionSet::x86::Model::i8086;
		} context;

		// TODO: eliminate use of Decoder8086 and Decoder8086 in gneral in favour of the templated version, as soon
		// as whatever error is preventing GCC from picking up Decoder's explicit instantiations becomes apparent.
		InstructionSet::x86::Decoder8086 decoder;
//		InstructionSet::x86::Decoder<InstructionSet::x86::Model::i8086> decoder;

		uint16_t decoded_ip_ = 0;
		std::pair<int, InstructionSet::x86::Instruction<false>> decoded;

		int cpu_divisor_ = 0;
};


}

using namespace PCCompatible;

// See header; constructs and returns an instance of the Amstrad CPC.
Machine *Machine::PCCompatible(const Analyser::Static::Target *target, const ROMMachine::ROMFetcher &rom_fetcher) {
	using Target = Analyser::Static::PCCompatible::Target;
	const Target *const pc_target = dynamic_cast<const Target *>(target);

	switch(pc_target->adaptor) {
		case VideoAdaptor::MDA:	return new PCCompatible::ConcreteMachine<VideoAdaptor::MDA>(*pc_target, rom_fetcher);
		case VideoAdaptor::CGA:	return new PCCompatible::ConcreteMachine<VideoAdaptor::CGA>(*pc_target, rom_fetcher);
		default: return nullptr;
	}
}

Machine::~Machine() {}
