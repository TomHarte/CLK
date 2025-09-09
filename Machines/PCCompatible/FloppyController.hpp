//
//  FloppyController.hpp
//  Clock Signal
//
//  Created by Thomas Harte on 07/03/2025.
//  Copyright © 2025 Thomas Harte. All rights reserved.
//

#pragma once

#include "DMA.hpp"
#include "PIC.hpp"
#include "PIT.hpp"

#include "Analyser/Static/PCCompatible/Target.hpp"
#include "Components/8272/CommandDecoder.hpp"
#include "Components/8272/Results.hpp"
#include "Components/8272/Status.hpp"
#include "Outputs/Log.hpp"
#include "Storage/Disk/Track/TrackSerialiser.hpp"
#include "Storage/Disk/Encodings/MFM/Parser.hpp"

#include <algorithm>
#include <numeric>

namespace PCCompatible {

template <Analyser::Static::PCCompatible::Model model>
class FloppyController {
public:
	FloppyController(
		PICs<model> &pics,
		DMA<model> &dma,
		const int drive_count
	) : pics_(pics), dma_(dma) {
		// Default: one floppy drive only.
		for(int c = 0; c < 4; c++) {
			drives_[c].exists = drive_count > c;
		}
	}

	void set_digital_output(const uint8_t control) {
		log_.info().append("Digital output: %02x", control);

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

		enable_dma_ = control & 0x08;	// Possibly also enables interrupts?

		const bool hold_reset = !(control & 0x04);
		if(!hold_reset && hold_reset_) {
			// TODO: add a delay mechanism.
			reset();
		}
		hold_reset_ = hold_reset;
		if(hold_reset_) {
			pics_.pic[0].template apply_edge<6>(false);
		}
	}

	void set_data_rate(const uint8_t control) {
		log_.info().append("Data rate: %02x", control);
	}

	uint8_t status() const {
		const auto result = status_.main();
		log_.info().append("Status: %02x", result);
		return result;
	}

	void write(const uint8_t value) {
//		log_.info().append("03f5 <- %02x", value);
		decoder_.push_back(value);

		if(decoder_.has_command()) {
			using Command = Intel::i8272::Command;
			switch(decoder_.command()) {
				default:
					log_.error().append("TODO: implement FDC command %02x", uint8_t(decoder_.command()));

					// Unimplemented:
					//
					//	ReadTrack
					//	ReadID
					//	FormatTrack
					//
					//	ScanLow
					//	ScanLowOrEqual
					//	ScanHighOrEqual
				break;

				case Command::WriteDeletedData:
				case Command::WriteData: {
					auto &drive = drives_[decoder_.target().drive];
					log_.info().append(
						"Write %sdata to drive %d / head %d / track %d of head %d / track %d / sector %d",
						decoder_.command() == Command::WriteDeletedData ? "deleted " : "",
						decoder_.target().drive,
						decoder_.target().head,
						drive.track,
						decoder_.geometry().head,
						decoder_.geometry().cylinder,
						decoder_.geometry().sector
					);
					status_.begin(decoder_);

					// Just decline to write, for now.
					// TODO: stop doing this.
					status_.set(Intel::i8272::Status1::NotWriteable);
					status_.set(Intel::i8272::Status0::BecameNotReady);

					results_.serialise(
						status_,
						decoder_.geometry().cylinder,
						decoder_.geometry().head,
						decoder_.geometry().sector,
						decoder_.geometry().size);

					// TODO: what if head has changed?
					drive.status = decoder_.drive_head();
					drive.raised_interrupt = true;
					pics_.pic[0].template apply_edge<6>(true);
				} break;

				case Command::ReadDeletedData:
				case Command::ReadData: {
					auto &drive = drives_[decoder_.target().drive];
					log_.info().append(
						"Read %sdata from drive %d / head %d / track %d of head %d / track %d / sector %d",
						decoder_.command() == Command::ReadDeletedData ? "deleted " : "",
						decoder_.target().drive,
						decoder_.target().head,
						drive.track,
						decoder_.geometry().head,
						decoder_.geometry().cylinder,
						decoder_.geometry().sector
					);

					status_.begin(decoder_);

					// Search for a matching sector.
					auto target = decoder_.geometry();
					bool complete = false;
					while(!complete) {
						const auto sector = drive.sector(target.head, target.sector);

						if(sector) {
							// TODO: I _think_ I'm supposed to validate the rest of the address here?

							for(int c = 0; c < 128 << target.size; c++) {
								const auto access_result = dma_.write(2, sector->samples[0].data()[c]);
								switch(access_result) {
									// Default: keep going.
									default: continue;

									// Anything else: update flags and exit.
									case AccessResult::NotAccepted:
										complete = true;
										status_.set(Intel::i8272::Status1::OverRun);
										status_.set(Intel::i8272::Status0::AbnormalTermination);
									break;
									case AccessResult::AcceptedWithEOP:
										complete = true;
									break;
								}
								break;
							}

							++target.sector;	// TODO: multitrack?
						} else {
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
					drive.status = decoder_.drive_head();
					drive.raised_interrupt = true;
					pics_.pic[0].template apply_edge<6>(true);
				} break;

				case Command::ReadID: {
					auto &drive = drives_[decoder_.target().drive];
					const auto target = decoder_.target();
					log_.info().append(
						"Read ID from drive %d / head %d / track %",
						target.drive,
						target.head,
						drive.track
					);

					// TODO: should really provide a succession of different IDs.
					status_.begin(decoder_);
					const auto sector = drive.any_sector(target.head);

					if(!sector) {
						status_.set(Intel::i8272::Status1::EndOfCylinder);
						status_.set(Intel::i8272::Status0::AbnormalTermination);

						results_.serialise(
							status_,
							0,
							0,
							0,
							0);
					} else {
						results_.serialise(
							status_,
							sector->address.track,
							sector->address.side,
							sector->address.sector,
							sector->size);
					}

					drive.status = decoder_.drive_head();
					drive.raised_interrupt = true;
					pics_.pic[0].template apply_edge<6>(true);
				} break;

				case Command::Recalibrate:
				case Command::Seek: {
					auto &drive = drives_[decoder_.target().drive];
					drive.track = decoder_.command() == Command::Seek ? decoder_.seek_target() : 0;
					log_.info().append(
						"%s to %d",
						decoder_.command() == Command::Seek ? "Seek" : "Recalibrate",
						drive.track
					);

					drive.raised_interrupt = true;
					drive.status = decoder_.target().drive | uint8_t(Intel::i8272::Status0::SeekEnded);
					drive.ready = drive.has_disk();
					pics_.pic[0].template apply_edge<6>(true);
				} break;

				case Command::SenseInterruptStatus: {
					const auto interruptor = std::find_if(
						std::begin(drives_),
						std::end(drives_),
						[] (const auto &drive) {
							return drive.raised_interrupt;
						}
					);
					if(interruptor != std::end(drives_)) {
						last_seeking_drive_ = interruptor - std::begin(drives_);
					}
					auto &drive = drives_[last_seeking_drive_];

					log_.info().append(
						"Sense interrupt status; picked drive %d with interrupt status %d",
						last_seeking_drive_,
						drive.raised_interrupt
					);
					status_.set_status0(drive.status);
					results_.serialise(status_, drive.track);

					// Clear cause-of-interrupt flags on that drive.
					drive.raised_interrupt = false;
					drive.status &= ~0xc0;

					// Possibly lower interrupt flag.
					const bool any_remaining_interrupts = std::accumulate(
						std::begin(drives_),
						std::end(drives_),
						false,
						[] (const bool flag, const auto &drive) {
							return flag | drive.raised_interrupt;
						}
					);
					if(!any_remaining_interrupts) {
						pics_.pic[0].template apply_edge<6>(false);
					}
				} break;
				case Command::Specify:
					log_.info().append("Specify");
					specify_specs_ = decoder_.specify_specs();
				break;
				case Command::SenseDriveStatus: {
					const auto &drive = drives_[decoder_.target().drive];
					log_.info().append(
						"Sense drive status: drive %d / head %d; track 0 is %d, ready is %d",
						decoder_.target().drive,
						decoder_.target().head,
						drive.track == 0,
						drive.ready
					);
					results_.serialise(
						decoder_.drive_head(),
						(drive.track == 0 ? 0x10 : 0x00)	|
						(drive.ready ? 0x20 : 0x00)			|	// Ready [=> has disc and has stepped].
						0x00									// Disk in drive is not read-only. [0x40]
					);
				} break;

				case Command::Invalid:
					log_.info().append("Invalid command");
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
			log_.info().append("Result read: %02x", result);
			return result;
		}

		log_.info().append("Result read: 80 [default]");
		return 0x80;
	}

	void set_activity_observer(Activity::Observer *const observer) {
		observer_ = observer;
		for(int c = 0; c < 4; c++) {
			if(drives_[c].exists) {
				observer_->register_led(drive_name(c), 0);
			}
		}
	}

	void set_disk(std::shared_ptr<Storage::Disk::Disk> disk, const int drive) {
//		if(drives_[drive].has_disk()) {
//			// TODO: drive should only transition to unready if it was ready in the first place.
//			drives_[drive].status = uint8_t(Intel::i8272::Status0::BecameNotReady);
//			drives_[drive].raised_interrupt = true;
//			pics_.pic[0].apply_edge<6>(true);
//		}
		drives_[drive].set_disk(disk);
	}

private:
	mutable Log::Logger<Log::Source::Floppy> log_;

	void reset() {
		log_.info().append("{Reset}");
		decoder_.clear();
		status_.reset();

		// Necessary to pass GlaBIOS' POST test, but: why?
		//
		// Cf. INT_13_0_2 and the CMP	AL, 11000000B following a CALL	FDC_WAIT_SENSE.
		for(int c = 0; c < 4; c++) {
			drives_[c].raised_interrupt = true;
			drives_[c].status = uint8_t(Intel::i8272::Status0::BecameNotReady) | uint8_t(c);
		}
		pics_.pic[0].template apply_edge<6>(true);

		using MainStatus = Intel::i8272::MainStatus;
		status_.set(MainStatus::DataReady, true);
		status_.set(MainStatus::DataIsToProcessor, false);
	}

	PICs<model> &pics_;
	DMA<model> &dma_;

	bool hold_reset_ = false;
	bool enable_dma_ = false;

	Intel::i8272::CommandDecoder decoder_;
	Intel::i8272::Status status_;
	Intel::i8272::Results results_;

	Intel::i8272::CommandDecoder::SpecifySpecs specify_specs_;
	struct DriveStatus {
	public:
		bool raised_interrupt = false;
		uint8_t status = 0;	// ST0 if this drive is selected.
		uint8_t track = 0;
		bool motor = false;
		bool exists = true;
		bool ready = false;

		bool has_disk() const {
			return static_cast<bool>(parser_);
		}

		void set_disk(std::shared_ptr<Storage::Disk::Disk> image) {
			parser_ = std::make_unique<Storage::Encodings::MFM::Parser>(image);
			ready = false;
		}

		const Storage::Encodings::MFM::Sector *sector(const int head, const uint8_t sector) {
			return parser_ ? parser_->sector(head, track, sector) : nullptr;
		}

		const Storage::Encodings::MFM::Sector *any_sector(const int head) {
			return parser_ ? parser_->any_sector(head, track) : nullptr;
		}

	private:
		std::unique_ptr<Storage::Encodings::MFM::Parser> parser_;
	} drives_[4];
	ssize_t last_seeking_drive_ = 0;

	static std::string drive_name(const int c) {
		char name[3] = "A";
		name[0] += c;
		return std::string("Drive ") + name;
	}

	Activity::Observer *observer_ = nullptr;
};

}
