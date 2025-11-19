//
//  TZX.cpp
//  Clock Signal
//
//  Created by Thomas Harte on 16/07/2017.
//  Copyright 2017 Thomas Harte. All rights reserved.
//

#include "TZX.hpp"

#include "CSW.hpp"
#include "Outputs/Log.hpp"

using namespace Storage::Tape;

namespace {
constexpr unsigned int StandardTZXClock = 3500000;
constexpr unsigned int TZXClockMSMultiplier = 3500;
using Logger = Log::Logger<Log::Source::TZX>;
}

TZX::TZX(const std::string &file_name) : file_name_(file_name) {
	Storage::FileHolder file(file_name, FileMode::Read);

	// Check for signature followed by a 0x1a
	if(!file.check_signature<SignatureType::String>("ZXTape!")) throw ErrorNotTZX;
	if(file.get() != 0x1a) throw ErrorNotTZX;

	// Get version number
	const uint8_t major_version = file.get();
	const uint8_t minor_version = file.get();

	// Reject if an incompatible version
	if(major_version != 1 || minor_version > 21) throw ErrorNotTZX;
}

std::unique_ptr<FormatSerialiser> TZX::format_serialiser() const {
	return std::make_unique<Serialiser>(file_name_);
}

TZX::Serialiser::Serialiser(const std::string &file_name) : file_(file_name, FileMode::Read) {
	reset();
}

void TZX::Serialiser::reset() {
	clear();
	set_is_at_end(false);
	file_.seek(0x0a, Whence::SET);

	// This is a workaround for arguably dodgy ZX80/ZX81 TZXs; they launch straight
	// into data but both machines require a gap before data begins. So impose
	// an initial gap, in the form of a very long wave.
	current_level_ = false;
	post_gap(500);
}

void TZX::Serialiser::push_next_pulses() {
	while(empty()) {
		const uint8_t chunk_id = file_.get();
		if(file_.eof()) {
			set_is_at_end(true);
			return;
		}

		switch(chunk_id) {
			case 0x10:	get_standard_speed_data_block();	break;
			case 0x11:	get_turbo_speed_data_block();		break;
			case 0x12:	get_pure_tone_data_block();			break;
			case 0x13:	get_pulse_sequence();				break;
			case 0x14:	get_pure_data_block();				break;
			case 0x15:	get_direct_recording_block();		break;
			case 0x18:	get_csw_recording_block();			break;
			case 0x19:	get_generalised_data_block();		break;
			case 0x20:	get_pause();						break;

			case 0x21:	ignore_group_start();				break;
			case 0x22:	ignore_group_end();					break;
			case 0x23:	ignore_jump_to_block();				break;
			case 0x24:	ignore_loop_start();				break;
			case 0x25:	ignore_loop_end();					break;
			case 0x26:	ignore_call_sequence();				break;
			case 0x27:	ignore_return_from_sequence();		break;
			case 0x28:	ignore_select_block();				break;
			case 0x2a:	ignore_stop_tape_if_in_48kb_mode();	break;

			case 0x2b:	get_set_signal_level();				break;

			case 0x30:	ignore_text_description();			break;
			case 0x31:	ignore_message_block();				break;
			case 0x32:	ignore_archive_info();				break;
			case 0x33:	get_hardware_type();				break;
			case 0x35:	ignore_custom_info_block();			break;

			case 0x4b:	get_kansas_city_block();			break;

			case 0x5a:	ignore_glue_block();				break;

			default:
				// In TZX each chunk has a different way of stating or implying its length,
				// so there is no route past an unimplemented chunk.
				Logger::error().append("Unknown TZX chunk: %04x", chunk_id);
				set_is_at_end(true);
			return;
		}
	}
}

void TZX::Serialiser::get_csw_recording_block() {
	const auto block_length = file_.get_le<uint32_t>();
	const auto pause_after_block = file_.get_le<uint16_t>();
	const auto sampling_rate = file_.get_le<uint32_t, 3>();
	const auto compression_type = file_.get();
	const auto number_of_compressed_pulses = file_.get_le<uint32_t>();

	std::vector<uint8_t> raw_block = file_.read(block_length - 10);

	const CSW csw(
		std::move(raw_block),
		(compression_type == 2) ? CSW::CompressionType::ZRLE : CSW::CompressionType::RLE,
		current_level_,
		sampling_rate
	);
	auto serialiser = csw.serialiser();
	while(!serialiser->is_at_end()) {
		Pulse next_pulse = serialiser->next_pulse();
		current_level_ = (next_pulse.type == Pulse::High);
		push_back(next_pulse);
	}

	(void)number_of_compressed_pulses;
	post_gap(pause_after_block);
}

void TZX::Serialiser::get_generalised_data_block() {
	const auto block_length = file_.get_le<uint32_t>();
	const long endpoint = file_.tell() + long(block_length);
	const auto pause_after_block = file_.get_le<uint16_t>();

	const auto total_pilot_symbols = file_.get_le<uint32_t>();
	const uint8_t maximum_pulses_per_pilot_symbol = file_.get();
	const uint8_t symbols_in_pilot_table = file_.get();

	const auto total_data_symbols = file_.get_le<uint32_t>();
	const uint8_t maximum_pulses_per_data_symbol = file_.get();
	const uint8_t symbols_in_data_table = file_.get();

	get_generalised_segment(total_pilot_symbols, maximum_pulses_per_pilot_symbol, symbols_in_pilot_table, false);
	get_generalised_segment(total_data_symbols, maximum_pulses_per_data_symbol, symbols_in_data_table, true);
	post_gap(pause_after_block);

	// This should be unnecessary, but intends to preserve sanity.
	file_.seek(endpoint, Whence::SET);
}

void TZX::Serialiser::get_generalised_segment(
	const uint32_t output_symbols,
	const uint8_t max_pulses_per_symbol,
	const uint8_t number_of_symbols,
	const bool is_data
) {
	if(!output_symbols) return;

	// Construct the symbol table.
	struct Symbol {
		uint8_t flags;
		std::vector<uint16_t> pulse_lengths;
	};
	std::vector<Symbol> symbol_table;
	for(int c = 0; c < number_of_symbols; c++) {
		Symbol symbol;
		symbol.flags = file_.get();
		for(int ic = 0; ic < max_pulses_per_symbol; ic++) {
			symbol.pulse_lengths.push_back(file_.get_le<uint16_t>());
		}
		symbol_table.push_back(symbol);
	}

	// Hence produce the output.
	auto stream = file_.bitstream<8, false>();
	int base = 2;
	size_t bits = 1;
	while(base < number_of_symbols) {
		base <<= 1;
		bits++;
	}
	for(std::size_t c = 0; c < output_symbols; c++) {
		uint8_t symbol_value;
		int count;
		if(is_data) {
			symbol_value = stream.next(bits);
			count = 1;
		} else {
			symbol_value = file_.get();
			count = file_.get_le<uint16_t>();
		}
		if(symbol_value > number_of_symbols) {
			continue;
		}
		Symbol &symbol = symbol_table[symbol_value];

		while(count--) {
			// Mutate initial output level.
			switch(symbol.flags & 3) {
				case 0: break;
				case 1: current_level_ ^= true;	break;
				case 2: current_level_ = true;	break;
				case 3: current_level_ = false;	break;
			}

			// Output waves.
			for(const auto length : symbol.pulse_lengths) {
				if(!length) break;
				post_pulse(length);
			}
		}
	}
}

void TZX::Serialiser::get_standard_speed_data_block() {
	DataBlock data_block;
	data_block.length_of_pilot_pulse = 2168;
	data_block.length_of_sync_first_pulse = 667;
	data_block.length_of_sync_second_pulse = 735;
	data_block.data.length_of_zero_bit_pulse = 855;
	data_block.data.length_of_one_bit_pulse = 1710;
	data_block.data.number_of_bits_in_final_byte = 8;

	data_block.data.pause_after_block = file_.get_le<uint16_t>();
	data_block.data.data_length = file_.get_le<uint16_t>();
	if(!data_block.data.data_length) return;

	const uint8_t first_byte = file_.get();
	data_block.length_of_pilot_tone = (first_byte < 128) ? 8063 : 3223;
	file_.seek(-1, Whence::CUR);

	get_data_block(data_block);
}

void TZX::Serialiser::get_turbo_speed_data_block() {
	DataBlock data_block;
	data_block.length_of_pilot_pulse = file_.get_le<uint16_t>();
	data_block.length_of_sync_first_pulse = file_.get_le<uint16_t>();
	data_block.length_of_sync_second_pulse = file_.get_le<uint16_t>();
	data_block.data.length_of_zero_bit_pulse = file_.get_le<uint16_t>();
	data_block.data.length_of_one_bit_pulse = file_.get_le<uint16_t>();
	data_block.length_of_pilot_tone = file_.get_le<uint16_t>();
	data_block.data.number_of_bits_in_final_byte = file_.get();
	data_block.data.pause_after_block = file_.get_le<uint16_t>();
	data_block.data.data_length = file_.get_le<uint32_t, 3>();

	get_data_block(data_block);
}

void TZX::Serialiser::get_data_block(const DataBlock &data_block) {
	// Output pilot tone.
	post_pulses(data_block.length_of_pilot_tone, data_block.length_of_pilot_pulse);

	// Output sync pulses.
	post_pulse(data_block.length_of_sync_first_pulse);
	post_pulse(data_block.length_of_sync_second_pulse);

	get_data(data_block.data);
}

void TZX::Serialiser::get_data(const Data &data) {
	// Output data.
	for(decltype(data.data_length) c = 0; c < data.data_length; c++) {
		uint8_t next_byte = file_.get();

		auto bits = (c != data.data_length-1) ? 8 : data.number_of_bits_in_final_byte;
		while(bits--) {
			unsigned int pulse_length = (next_byte & 0x80) ? data.length_of_one_bit_pulse : data.length_of_zero_bit_pulse;
			next_byte <<= 1;

			post_pulse(pulse_length);
			post_pulse(pulse_length);
		}
	}

	// Output gap.
	post_gap(data.pause_after_block);
}

void TZX::Serialiser::get_pure_tone_data_block() {
	const auto length_of_pulse = file_.get_le<uint16_t>();
	const auto nunber_of_pulses = file_.get_le<uint16_t>();

	post_pulses(nunber_of_pulses, length_of_pulse);
}

void TZX::Serialiser::get_pure_data_block() {
	Data data;
	data.length_of_zero_bit_pulse = file_.get_le<uint16_t>();
	data.length_of_one_bit_pulse = file_.get_le<uint16_t>();
	data.number_of_bits_in_final_byte = file_.get();
	data.pause_after_block = file_.get_le<uint16_t>();
	data.data_length = file_.get_le<uint32_t, 3>();

	get_data(data);
}

void TZX::Serialiser::get_direct_recording_block() {
	const Storage::Time length_per_sample(unsigned(file_.get_le<uint16_t>()), StandardTZXClock);
	const auto pause_after_block = file_.get_le<uint16_t>();
	uint8_t used_bits_in_final_byte = file_.get();
	const auto length_of_data = file_.get_le<uint32_t, 3>();

	if(used_bits_in_final_byte < 1) used_bits_in_final_byte = 1;
	if(used_bits_in_final_byte > 8) used_bits_in_final_byte = 8;

	uint8_t byte = 0;
	unsigned int bits_at_level = 0;
	uint8_t level = 0;
	for(std::size_t bit = 0; bit < (length_of_data - 1) * 8 + used_bits_in_final_byte; ++bit) {
		if(!(bit&7)) byte = file_.get();
		if(!bit) level = byte&0x80;

		if((byte&0x80) != level) {
			emplace_back(level ? Pulse::High : Pulse::Low, length_per_sample * bits_at_level);
			bits_at_level = 0;
			level = byte&0x80;
		}
		bits_at_level++;
	}

	current_level_ = !!(level);
	emplace_back(level ? Pulse::High : Pulse::Low, length_per_sample * bits_at_level);

	post_gap(pause_after_block);
}

void TZX::Serialiser::get_pulse_sequence() {
	uint8_t number_of_pulses = file_.get();
	while(number_of_pulses--) {
		post_pulse(file_.get_le<uint16_t>());
	}
}

void TZX::Serialiser::get_pause() {
	const auto duration = file_.get_le<uint16_t>();
	if(!duration) {
		// TODO (maybe): post a 'pause the tape' suggestion
	} else {
		post_gap(duration);
	}
}

void TZX::Serialiser::get_set_signal_level() {
	file_.seek(4, Whence::CUR);
	const uint8_t level = file_.get();
	current_level_ = !!level;
}

void TZX::Serialiser::get_kansas_city_block() {
	auto block_length = file_.get_le<uint32_t>();

	const auto pause_after_block = file_.get_le<uint16_t>();
	const auto pilot_pulse_duration = file_.get_le<uint16_t>();
	const auto pilot_length = file_.get_le<uint16_t>();
	uint16_t pulse_durations[2];
	pulse_durations[0] = file_.get_le<uint16_t>();
	pulse_durations[1] = file_.get_le<uint16_t>();
	const uint8_t packed_pulse_counts = file_.get();
	const unsigned int pulse_counts[2] = {
		unsigned((((packed_pulse_counts >> 4) - 1) & 15) + 1),
		unsigned((((packed_pulse_counts & 15) - 1) & 15) + 1)
	};
	const uint8_t padding_flags = file_.get();

	const unsigned int number_of_leading_pulses = ((padding_flags >> 6)&3) * pulse_counts[(padding_flags >> 5) & 1];
	const unsigned int leading_pulse_length = pulse_durations[(padding_flags >> 5) & 1];

	const unsigned int number_of_trailing_pulses = ((padding_flags >> 3)&3) * pulse_counts[(padding_flags >> 2) & 1];
	const unsigned int trailing_pulse_length = pulse_durations[(padding_flags >> 2) & 1];

	block_length -= 12;

	// Output pilot tone.
	post_pulses(pilot_length, pilot_pulse_duration);

	// Output data.
	while(block_length--) {
		post_pulses(number_of_leading_pulses, leading_pulse_length);

		uint8_t new_byte = file_.get();
		int bits = 8;
		if(padding_flags & 1) {
			// Output MSB first.
			while(bits--) {
				int bit = (new_byte >> 7) & 1;
				new_byte <<= 1;
				post_pulses(pulse_counts[bit], pulse_durations[bit]);
			}
		} else {
			// Output LSB first.
			while(bits--) {
				int bit = new_byte & 1;
				new_byte >>= 1;
				post_pulses(pulse_counts[bit], pulse_durations[bit]);
			}
		}

		post_pulses(number_of_trailing_pulses, trailing_pulse_length);
	}

	// Output gap.
	post_gap(pause_after_block);
}

// MARK: - Output

void TZX::Serialiser::post_pulses(unsigned int count, const unsigned int length) {
	while(count--) post_pulse(length);
}

void TZX::Serialiser::post_pulse(const unsigned int length) {
	post_pulse(Storage::Time(length, StandardTZXClock));
}

void TZX::Serialiser::post_gap(const unsigned int milliseconds) {
	if(!milliseconds) return;
	if(milliseconds > 1 && !current_level_) {
		post_pulse(Storage::Time(TZXClockMSMultiplier, StandardTZXClock));
		post_pulse(Storage::Time((milliseconds - 1u)*TZXClockMSMultiplier, StandardTZXClock));
	} else {
		post_pulse(Storage::Time(milliseconds*TZXClockMSMultiplier, StandardTZXClock));
	}
}

void TZX::Serialiser::post_pulse(const Storage::Time &time) {
	emplace_back(current_level_ ? Pulse::High : Pulse::Low, time);
	current_level_ ^= true;
}

// MARK: - Flow control; currently ignored

void TZX::Serialiser::ignore_group_start() {
	const uint8_t length = file_.get();
	file_.seek(length, Whence::CUR);
}

void TZX::Serialiser::ignore_group_end() {
}

void TZX::Serialiser::ignore_jump_to_block() {
	const auto target = file_.get_le<uint16_t>();
	(void)target;
}

void TZX::Serialiser::ignore_loop_start() {
	const auto number_of_repetitions = file_.get_le<uint16_t>();
	(void)number_of_repetitions;
}

void TZX::Serialiser::ignore_loop_end() {
}

void TZX::Serialiser::ignore_call_sequence() {
	const auto number_of_entries = file_.get_le<uint16_t>();
	file_.seek(number_of_entries * sizeof(uint16_t), Whence::CUR);
}

void TZX::Serialiser::ignore_return_from_sequence() {
}

void TZX::Serialiser::ignore_select_block() {
	const auto length_of_block = file_.get_le<uint16_t>();
	file_.seek(length_of_block, Whence::CUR);
}

void TZX::Serialiser::ignore_stop_tape_if_in_48kb_mode() {
	file_.seek(4, Whence::CUR);
}

void TZX::Serialiser::ignore_custom_info_block() {
	file_.seek(0x10, Whence::CUR);
	const auto length = file_.get_le<uint32_t>();
	file_.seek(length, Whence::CUR);
}

// MARK: - Messaging

void TZX::Serialiser::ignore_text_description() {
	const uint8_t length = file_.get();
	file_.seek(length, Whence::CUR);
}

void TZX::Serialiser::ignore_message_block() {
	const uint8_t time_for_display = file_.get();
	const uint8_t length = file_.get();
	file_.seek(length, Whence::CUR);
	(void)time_for_display;
}

void TZX::Serialiser::ignore_archive_info() {
	const auto length = file_.get_le<uint16_t>();
	file_.seek(length, Whence::CUR);
}

void TZX::Serialiser::get_hardware_type() {
	// TODO: pick a way to retain and communicate this.
	const uint8_t number_of_machines = file_.get();
	file_.seek(number_of_machines * 3, Whence::CUR);
}

void TZX::Serialiser::ignore_glue_block() {
	file_.seek(9, Whence::CUR);
}
