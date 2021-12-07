//
//  Drive.cpp
//  Clock Signal
//
//  Created by Thomas Harte on 25/09/2016.
//  Copyright 2016 Thomas Harte. All rights reserved.
//

#include "Drive.hpp"

#include "Track/UnformattedTrack.hpp"

#include <algorithm>
#include <cassert>
#include <cmath>
#include <chrono>
#include <random>

using namespace Storage::Disk;

Drive::Drive(int input_clock_rate, int revolutions_per_minute, int number_of_heads, ReadyType rdy_type):
	Storage::TimedEventLoop(input_clock_rate),
	available_heads_(number_of_heads),
	ready_type_(rdy_type) {
	set_rotation_speed(revolutions_per_minute);

	const auto seed = std::default_random_engine::result_type(std::chrono::system_clock::now().time_since_epoch().count());
	std::default_random_engine randomiser(seed);

	// Get at least 64 bits of random information; rounding is likey to give this a slight bias.
	random_source_ = 0;
	auto half_range = (randomiser.max() - randomiser.min()) / 2;
	for(int bit = 0; bit < 64; ++bit) {
		random_source_ <<= 1;
		random_source_ |= ((randomiser() - randomiser.min()) >= half_range) ? 1 : 0;
	}
}

Drive::Drive(int input_clock_rate, int number_of_heads, ReadyType rdy_type) : Drive(input_clock_rate, 300, number_of_heads, rdy_type) {}

void Drive::set_rotation_speed(float revolutions_per_minute) {
	// Rationalise the supplied speed so that cycles_per_revolution_ is exact.
	cycles_per_revolution_ = int(0.5f + float(get_input_clock_rate()) * 60.0f / revolutions_per_minute);

	// From there derive the appropriate rotational multiplier and possibly update the
	// count of cycles since the index hole proportionally.
	const float new_rotational_multiplier = float(cycles_per_revolution_) / float(get_input_clock_rate());
	cycles_since_index_hole_ = Cycles::IntType(float(cycles_since_index_hole_) * new_rotational_multiplier / rotational_multiplier_);
	rotational_multiplier_ = new_rotational_multiplier;
	cycles_since_index_hole_ %= cycles_per_revolution_;
}

Drive::~Drive() {
	if(disk_) disk_->flush_tracks();
}

void Drive::set_disk(const std::shared_ptr<Disk> &disk) {
	if(ready_type_ == ReadyType::ShugartModifiedRDY || ready_type_ == ReadyType::IBMRDY) {
		is_ready_ = false;
	}
	const bool had_disk = bool(disk_);
	if(disk_) disk_->flush_tracks();
	disk_ = disk;
	has_disk_ = !!disk_;

	invalidate_track();
	did_set_disk(had_disk);
	update_clocking_observer();
}

bool Drive::has_disk() const {
	return has_disk_;
}

ClockingHint::Preference Drive::preferred_clocking() const {
	return (!has_disk_ || (time_until_motor_transition == Cycles(0) && !disk_is_rotating_)) ? ClockingHint::Preference::None : ClockingHint::Preference::JustInTime;
}

bool Drive::get_is_track_zero() const {
	return head_position_ == HeadPosition(0);
}

void Drive::step(HeadPosition offset) {
	if(offset == HeadPosition(0)) {
		return;
	}

	if(disk_ && ready_type_ == ReadyType::IBMRDY) {
		is_ready_ = true;
	}

	HeadPosition old_head_position = head_position_;
	head_position_ += offset;
	if(head_position_ < HeadPosition(0)) {
		head_position_ = HeadPosition(0);
		if(observer_) observer_->announce_drive_event(drive_name_, Activity::Observer::DriveEvent::StepBelowZero);
	} else {
		if(observer_) observer_->announce_drive_event(drive_name_, Activity::Observer::DriveEvent::StepNormal);
	}

	// If the head moved, flush the old track.
	if(disk_ && disk_->tracks_differ(Track::Address(head_, head_position_), Track::Address(head_, old_head_position))) {
		track_ = nullptr;
	}

	// Allow a subclass to react, if desired.
	did_step(head_position_);
}

std::shared_ptr<Track> Drive::step_to(HeadPosition offset) {
	HeadPosition old_head_position = head_position_;
	head_position_ = std::max(offset, HeadPosition(0));

	if(disk_ && head_position_ != old_head_position) {
		track_ = nullptr;
		setup_track();
	}

	return track_;
}

void Drive::set_head(int head) {
	head = std::min(head, available_heads_ - 1);
	if(head != head_) {
		head_ = head;
		track_ = nullptr;
	}
}

int Drive::get_head_count() const {
	return available_heads_;
}

bool Drive::get_tachometer() const {
	// I have made a guess here that the tachometer is a symmetric square wave;
	// if that is correct then around 60 beats per rotation appears to be correct
	// to proceed beyond the speed checks I've so far uncovered.
	constexpr float ticks_per_rotation = 60.0f; // 56 was too low; 64 too high.
	return int(get_rotation() * 2.0f * ticks_per_rotation) & 1;
}

float Drive::get_rotation() const {
	return get_time_into_track();
}

float Drive::get_time_into_track() const {
	// i.e. amount of time since the index hole was seen, as a proportion of a second,
	// converted to a proportion of a rotation.
	return float(cycles_since_index_hole_) / (float(get_input_clock_rate()) * rotational_multiplier_);
}

bool Drive::get_is_read_only() const {
	if(disk_) return disk_->get_is_read_only();
	return true;
}

bool Drive::get_is_ready() const {
	return is_ready_;
}

void Drive::set_motor_on(bool motor_is_on) {
	// Do nothing if the input hasn't changed.
	if(motor_input_is_on_ == motor_is_on) return;
	motor_input_is_on_ = motor_is_on;

	// If this now means that the input and the actual state are in harmony,
	// cancel any planned change and stop.
	if(disk_is_rotating_ == motor_is_on) {
		time_until_motor_transition = Cycles(0);
		return;
	}

	// If this is a transition to on, start immediately.
	// TODO: spin-up?
	// TODO: momentum.
	if(motor_is_on) {
		set_disk_is_rotating(true);
		time_until_motor_transition = Cycles(0);
		return;
	}

	// This is a transition from on to off. Simulate momentum (ha!)
	// by delaying the time until complete standstill.
	if(time_until_motor_transition == Cycles(0))
		time_until_motor_transition = get_input_clock_rate();
}

bool Drive::get_motor_on() const {
	return motor_input_is_on_;
}

bool Drive::get_index_pulse() const {
	return index_pulse_remaining_ > Cycles(0);
}

void Drive::set_event_delegate(Storage::Disk::Drive::EventDelegate *delegate) {
	event_delegate_ = delegate;
}

void Drive::advance(const Cycles cycles) {
	cycles_since_index_hole_ += cycles.as_integral();
	if(event_delegate_) event_delegate_->advance(cycles);
}

void Drive::run_for(const Cycles cycles) {
	// Assumed: the index pulse pulses even if the drive has stopped spinning.
	index_pulse_remaining_ = std::max(index_pulse_remaining_ - cycles, Cycles(0));

	if(time_until_motor_transition > Cycles(0)) {
		if(time_until_motor_transition > cycles) {
			time_until_motor_transition -= cycles;
		} else {
			time_until_motor_transition = Cycles(0);
			set_disk_is_rotating(!disk_is_rotating_);
		}
	}

	if(disk_is_rotating_) {
		if(has_disk_) {
			Time zero(0);

			auto number_of_cycles = cycles.as_integral();
			while(number_of_cycles) {
				auto cycles_until_next_event = get_cycles_until_next_event();
				auto cycles_to_run_for = std::min(cycles_until_next_event, number_of_cycles);
				if(!is_reading_ && cycles_until_bits_written_ > zero) {
					auto write_cycles_target = cycles_until_bits_written_.get<Cycles::IntType>();
					if(cycles_until_bits_written_.length % cycles_until_bits_written_.clock_rate) ++write_cycles_target;
					cycles_to_run_for = std::min(cycles_to_run_for, write_cycles_target);
				}

				number_of_cycles -= cycles_to_run_for;
				if(!is_reading_) {
					if(cycles_until_bits_written_ > zero) {
						Storage::Time cycles_to_run_for_time(static_cast<int>(cycles_to_run_for));
						if(cycles_until_bits_written_ <= cycles_to_run_for_time) {
							if(event_delegate_) event_delegate_->process_write_completed();
							if(cycles_until_bits_written_ <= cycles_to_run_for_time)
								cycles_until_bits_written_.set_zero();
							else
								cycles_until_bits_written_ -= cycles_to_run_for_time;
						} else {
							cycles_until_bits_written_ -= cycles_to_run_for_time;
						}
					}
				}
				TimedEventLoop::run_for(Cycles(cycles_to_run_for));
			}
		} else {
			TimedEventLoop::run_for(cycles);
		}
	}
}

// MARK: - Track timed event loop

void Drive::get_next_event(float duration_already_passed) {
	/*
		Quick word on random-bit generation logic below; it seeks to obey the following logic:
		if there is a gap of 15µs between recorded bits, start generating flux transitions
		at random intervals thereafter, unless and until one is within 5µs of the next real transition.

		This behaviour is based on John Morris' observations of an MC3470, as described in his WOZ
		file format documentation — https://applesaucefdc.com/woz/reference2/
	*/

	if(!disk_) {
		current_event_.type = Track::Event::IndexHole;
		current_event_.length = 1.0f;
		set_next_event_time_interval((current_event_.length - duration_already_passed) * rotational_multiplier_);
		return;
	}

	// Grab a new track if not already in possession of one. This will recursively call get_next_event,
	// supplying a proper duration_already_passed.
	if(!track_) {
		random_interval_ = 0.0f;
		setup_track();
		return;
	}

	// If gain has now been turned up so as to generate noise, generate some noise.
	if(random_interval_ > 0.0f) {
		current_event_.type = Track::Event::FluxTransition;
		current_event_.length = float(2 + (random_source_&1)) / 1'000'000.0f;
		random_source_ = (random_source_ >> 1) | (random_source_ << 63);

		// If this random transition is closer than 5µs to the next real bit,
		// discard it.
		if(random_interval_ - 5.0f / 1'000'000.f < current_event_.length) {
			random_interval_ = 0.0f;
		} else {
			random_interval_ -= current_event_.length;
			set_next_event_time_interval(current_event_.length);
			return;
		}
	}

	if(track_) {
		const auto track_event = track_->get_next_event();
		current_event_.type = track_event.type;
		current_event_.length = track_event.length.get<float>();
	} else {
		current_event_.length = 1.0f;
		current_event_.type = Track::Event::IndexHole;
	}

	// divide interval, which is in terms of a single rotation of the disk, by rotation speed to
	// convert it into revolutions per second; this is achieved by multiplying by rotational_multiplier_
	float interval = std::max((current_event_.length - duration_already_passed) * rotational_multiplier_, 0.0f);

	// An interval greater than 15µs => adjust gain up the point where noise starts happening.
	// Seed that up and leave a 15µs gap until it starts.
	constexpr float safe_gain_period = 15.0f / 1'000'000.0f;
	if(interval >= safe_gain_period) {
		random_interval_ = interval - safe_gain_period;
		interval = safe_gain_period;
	}

	set_next_event_time_interval(interval);
}

void Drive::process_next_event() {
	if(current_event_.type == Track::Event::IndexHole) {
		++ready_index_count_;
		if(ready_index_count_ == 2 && (ready_type_ == ReadyType::ShugartRDY || ready_type_ == ReadyType::ShugartModifiedRDY)) {
			is_ready_ = true;
		}
		cycles_since_index_hole_ = 0;

		// Begin a 2ms period of holding the index line pulse active.
		index_pulse_remaining_ = Cycles((get_input_clock_rate() * 2) / 1000);
	}
	if(
		event_delegate_ &&
		(current_event_.type == Track::Event::IndexHole || is_reading_)
	){
		event_delegate_->process_event(current_event_);
	}
	get_next_event(0.0f);
}

// MARK: - Track management

std::shared_ptr<Track> Drive::get_track() {
	if(disk_) return disk_->get_track_at_position(Track::Address(head_, head_position_));
	return nullptr;
}

void Drive::set_track(const std::shared_ptr<Track> &track) {
	if(disk_) disk_->set_track_at_position(Track::Address(head_, head_position_), track);
}

void Drive::setup_track() {
	track_ = get_track();
	if(!track_) {
		track_ = std::make_shared<UnformattedTrack>();
	}

	float offset = 0.0f;
	const float track_time_now = get_time_into_track();
	const float time_found = track_->seek_to(track_time_now);

	// `time_found` can be greater than `track_time_now` if limited precision caused rounding.
	if(time_found <= track_time_now) {
		offset = track_time_now - time_found;
	}

	// Reseed cycles_since_index_hole_; 99.99% of the time it'll still be correct as is,
	// but if the track has rounded one way or the other it may now be very slightly adrift.
	cycles_since_index_hole_ = (int((time_found + offset) * cycles_per_revolution_)) % cycles_per_revolution_;

	get_next_event(offset);
}

void Drive::invalidate_track() {
	random_interval_ = 0.0f;
	track_ = nullptr;
	if(patched_track_) {
		set_track(patched_track_);
		patched_track_ = nullptr;
	}
}

// MARK: - Writing

void Drive::begin_writing(Time bit_length, bool clamp_to_index_hole) {
	// Do nothing if already writing.
	// TODO: cope properly if there's no disk to write to.
	if(!is_reading_ || !disk_) return;

	// Get a copy of the track if that hasn't happened yet.
	if(!track_) {
		setup_track();
	}

	// Store the relevant parameters, and kick off writing.
	is_reading_ = false;
	clamp_writing_to_index_hole_ = clamp_to_index_hole;

	cycles_per_bit_ = Storage::Time(int(get_input_clock_rate())) * bit_length;
	cycles_per_bit_.simplify();

	write_segment_.length_of_a_bit = bit_length / Time(rotational_multiplier_);
	write_segment_.data.clear();

	write_start_time_ = Time(get_time_into_track());
}

void Drive::write_bit(bool value) {
	write_segment_.data.push_back(value);
	cycles_until_bits_written_ += cycles_per_bit_;
}

void Drive::end_writing() {
	// If the user modifies a track, it's scaled up to a "high" resolution and modifications
	// are plotted on top of that.
	//
	// "High" is defined as: two samples per clock relative to an idiomatic
	// 8Mhz disk controller and 300RPM disk speed.
	const size_t high_resolution_track_rate = 3200000;

	if(!is_reading_) {
		is_reading_ = true;

		if(!patched_track_) {
			// Avoid creating a new patched track if this one is already patched
			patched_track_ = std::dynamic_pointer_cast<PCMTrack>(track_);
			if(!patched_track_ || !patched_track_->is_resampled_clone()) {
				Track *const tr = track_.get();
				patched_track_.reset(PCMTrack::resampled_clone(tr, high_resolution_track_rate));
			}
		}
		patched_track_->add_segment(write_start_time_, write_segment_, clamp_writing_to_index_hole_);
		cycles_since_index_hole_ %= cycles_per_revolution_;
		invalidate_track();
	}
}

bool Drive::is_writing() const {
	return !is_reading_;
}

void Drive::set_disk_is_rotating(bool is_rotating) {
	disk_is_rotating_ = is_rotating;

	if(observer_) {
		observer_->set_drive_motor_status(drive_name_, disk_is_rotating_);
		if(announce_motor_led_) {
			observer_->set_led_status(drive_name_, disk_is_rotating_);
		}
	}

	if(!is_rotating) {
		if(ready_type_ == ReadyType::ShugartRDY) {
			is_ready_ = false;
		}
		ready_index_count_ = 0;
		if(disk_) disk_->flush_tracks();
	}
	update_clocking_observer();
}

void Drive::set_activity_observer(Activity::Observer *observer, const std::string &name, bool add_motor_led) {
	observer_ = observer;
	announce_motor_led_ = add_motor_led;
	if(observer) {
		drive_name_ = name;

		observer->register_drive(drive_name_);
		observer->set_drive_motor_status(drive_name_, disk_is_rotating_);

		if(add_motor_led) {
			observer->register_led(drive_name_);
			observer->set_led_status(drive_name_, disk_is_rotating_);
		}
	}
}
