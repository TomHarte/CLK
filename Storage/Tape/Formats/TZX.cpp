//
//  TZX.cpp
//  Clock Signal
//
//  Created by Thomas Harte on 16/07/2017.
//  Copyright 2017 Thomas Harte. All rights reserved.
//

#include "TZX.hpp"

#include "CSW.hpp"
#include "../../../Outputs/Log.hpp"

using namespace Storage::Tape;

namespace {
const unsigned int StandardTZXClock = 3500000;
const unsigned int TZXClockMSMultiplier = 3500;
}

TZX::TZX(const std::string &file_name) :
	file_(file_name),
	current_level_(false) {

	// Check for signature followed by a 0x1a
	if(!file_.check_signature("ZXTape!")) throw ErrorNotTZX;
	if(file_.get8() != 0x1a) throw ErrorNotTZX;

	// Get version number
	uint8_t major_version = file_.get8();
	uint8_t minor_version = file_.get8();

	// Reject if an incompatible version
	if(major_version != 1 || minor_version > 21)  throw ErrorNotTZX;

	virtual_reset();
}

void TZX::virtual_reset() {
	clear();
	set_is_at_end(false);
	file_.seek(0x0a, SEEK_SET);

	// This is a workaround for arguably dodgy ZX80/ZX81 TZXs; they launch straight
	// into data but both machines require a gap before data begins. So impose
	// an initial gap, in the form of a very long wave.
	current_level_ = false;
	post_gap(500);
}

void TZX::get_next_pulses() {
	while(empty()) {
		uint8_t chunk_id = file_.get8();
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
				LOG("Unknown TZX chunk: " << PADHEX(4) << chunk_id);
				set_is_at_end(true);
			return;
		}
	}
}

void TZX::get_csw_recording_block() {
	const uint32_t block_length = file_.get32le();
	const uint16_t pause_after_block = file_.get16le();
	const uint32_t sampling_rate = file_.get24le();
	const uint8_t compression_type = file_.get8();
	const uint32_t number_of_compressed_pulses = file_.get32le();

	std::vector<uint8_t> raw_block = file_.read(block_length - 10);

	CSW csw(std::move(raw_block), (compression_type == 2) ? CSW::CompressionType::ZRLE : CSW::CompressionType::RLE, current_level_, sampling_rate);
	while(!csw.is_at_end()) {
		Tape::Pulse next_pulse = csw.get_next_pulse();
		current_level_ = (next_pulse.type == Tape::Pulse::High);
		emplace_back(std::move(next_pulse));
	}

	(void)number_of_compressed_pulses;
	post_gap(pause_after_block);
}

void TZX::get_generalised_data_block() {
	uint32_t block_length = file_.get32le();
	long endpoint = file_.tell() + long(block_length);
	uint16_t pause_after_block = file_.get16le();

	uint32_t total_pilot_symbols = file_.get32le();
	uint8_t maximum_pulses_per_pilot_symbol = file_.get8();
	uint8_t symbols_in_pilot_table = file_.get8();

	uint32_t total_data_symbols = file_.get32le();
	uint8_t maximum_pulses_per_data_symbol = file_.get8();
	uint8_t symbols_in_data_table = file_.get8();

	get_generalised_segment(total_pilot_symbols, maximum_pulses_per_pilot_symbol, symbols_in_pilot_table, false);
	get_generalised_segment(total_data_symbols, maximum_pulses_per_data_symbol, symbols_in_data_table, true);
	post_gap(pause_after_block);

	// This should be unnecessary, but intends to preserve sanity.
	file_.seek(endpoint, SEEK_SET);
}

void TZX::get_generalised_segment(uint32_t output_symbols, uint8_t max_pulses_per_symbol, uint8_t number_of_symbols, bool is_data) {
	if(!output_symbols) return;

	// Construct the symbol table.
	struct Symbol {
		uint8_t flags;
		std::vector<uint16_t> pulse_lengths;
	};
	std::vector<Symbol> symbol_table;
	for(int c = 0; c < number_of_symbols; c++) {
		Symbol symbol;
		symbol.flags = file_.get8();
		for(int ic = 0; ic < max_pulses_per_symbol; ic++) {
			symbol.pulse_lengths.push_back(file_.get16le());
		}
		symbol_table.push_back(symbol);
	}

	// Hence produce the output.
	FileHolder::BitStream stream = file_.get_bitstream(false);
	int base = 2;
	int bits = 1;
	while(base < number_of_symbols) {
		base <<= 1;
		bits++;
	}
	for(std::size_t c = 0; c < output_symbols; c++) {
		uint8_t symbol_value;
		int count;
		if(is_data) {
			symbol_value = stream.get_bits(bits);
			count = 1;
		} else {
			symbol_value = file_.get8();
			count = file_.get16le();
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

void TZX::get_standard_speed_data_block() {
	DataBlock data_block;
	data_block.length_of_pilot_pulse = 2168;
	data_block.length_of_sync_first_pulse = 667;
	data_block.length_of_sync_second_pulse = 735;
	data_block.data.length_of_zero_bit_pulse = 855;
	data_block.data.length_of_one_bit_pulse = 1710;
	data_block.data.number_of_bits_in_final_byte = 8;

	data_block.data.pause_after_block = file_.get16le();
	data_block.data.data_length = file_.get16le();
	if(!data_block.data.data_length) return;

	uint8_t first_byte = file_.get8();
	data_block.length_of_pilot_tone = (first_byte < 128) ? 8063  : 3223;
	file_.seek(-1, SEEK_CUR);

	get_data_block(data_block);
}

void TZX::get_turbo_speed_data_block() {
	DataBlock data_block;
	data_block.length_of_pilot_pulse = file_.get16le();
	data_block.length_of_sync_first_pulse = file_.get16le();
	data_block.length_of_sync_second_pulse = file_.get16le();
	data_block.data.length_of_zero_bit_pulse = file_.get16le();
	data_block.data.length_of_one_bit_pulse = file_.get16le();
	data_block.length_of_pilot_tone = file_.get16le();
	data_block.data.number_of_bits_in_final_byte = file_.get8();
	data_block.data.pause_after_block = file_.get16le();
	data_block.data.data_length = file_.get24le();

	get_data_block(data_block);
}

void TZX::get_data_block(const DataBlock &data_block) {
	// Output pilot tone.
	post_pulses(data_block.length_of_pilot_tone, data_block.length_of_pilot_pulse);

	// Output sync pulses.
	post_pulse(data_block.length_of_sync_first_pulse);
	post_pulse(data_block.length_of_sync_second_pulse);

	get_data(data_block.data);
}

void TZX::get_data(const Data &data) {
	// Output data.
	for(decltype(data.data_length) c = 0; c < data.data_length; c++) {
		uint8_t next_byte = file_.get8();

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

void TZX::get_pure_tone_data_block() {
	uint16_t length_of_pulse = file_.get16le();
	uint16_t nunber_of_pulses = file_.get16le();

	post_pulses(nunber_of_pulses, length_of_pulse);
}

void TZX::get_pure_data_block() {
	Data data;
	data.length_of_zero_bit_pulse = file_.get16le();
	data.length_of_one_bit_pulse = file_.get16le();
	data.number_of_bits_in_final_byte = file_.get8();
	data.pause_after_block = file_.get16le();
	data.data_length = file_.get24le();

	get_data(data);
}

void TZX::get_direct_recording_block() {
	const Storage::Time length_per_sample(unsigned(file_.get16le()), StandardTZXClock);
	const uint16_t pause_after_block = file_.get16le();
	uint8_t used_bits_in_final_byte = file_.get8();
	const uint32_t length_of_data = file_.get24le();

	if(used_bits_in_final_byte < 1) used_bits_in_final_byte = 1;
	if(used_bits_in_final_byte > 8) used_bits_in_final_byte = 8;

	uint8_t byte = 0;
	unsigned int bits_at_level = 0;
	uint8_t level = 0;
	for(std::size_t bit = 0; bit < (length_of_data - 1) * 8 + used_bits_in_final_byte; ++bit) {
		if(!(bit&7)) byte = file_.get8();
		if(!bit) level = byte&0x80;

		if((byte&0x80) != level) {
			emplace_back(level ? Tape::Pulse::High : Tape::Pulse::Low, length_per_sample * bits_at_level);
			bits_at_level = 0;
			level = byte&0x80;
		}
		bits_at_level++;
	}

	current_level_ = !!(level);
	emplace_back(level ? Tape::Pulse::High : Tape::Pulse::Low, length_per_sample * bits_at_level);

	post_gap(pause_after_block);
}

void TZX::get_pulse_sequence() {
	uint8_t number_of_pulses = file_.get8();
	while(number_of_pulses--) {
		post_pulse(file_.get16le());
	}
}

void TZX::get_pause() {
	uint16_t duration = file_.get16le();
	if(!duration) {
		// TODO (maybe): post a 'pause the tape' suggestion
	} else {
		post_gap(duration);
	}
}

void TZX::get_set_signal_level() {
	file_.seek(4, SEEK_CUR);
	const uint8_t level = file_.get8();
	current_level_ = !!level;
}

void TZX::get_kansas_city_block() {
	uint32_t block_length = file_.get32le();

	const uint16_t pause_after_block = file_.get16le();
	const uint16_t pilot_pulse_duration = file_.get16le();
	const uint16_t pilot_length = file_.get16le();
	uint16_t pulse_durations[2];
	pulse_durations[0] = file_.get16le();
	pulse_durations[1] = file_.get16le();
	const uint8_t packed_pulse_counts = file_.get8();
	const unsigned int pulse_counts[2] = {
		unsigned((((packed_pulse_counts >> 4) - 1) & 15) + 1),
		unsigned((((packed_pulse_counts & 15) - 1) & 15) + 1)
	};
	const uint8_t padding_flags = file_.get8();

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

		uint8_t new_byte = file_.get8();
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

void TZX::post_pulses(unsigned int count, unsigned int length) {
	while(count--) post_pulse(length);
}

void TZX::post_pulse(unsigned int length) {
	post_pulse(Storage::Time(length, StandardTZXClock));
}

void TZX::post_gap(unsigned int milliseconds) {
	if(!milliseconds) return;
	if(milliseconds > 1 && !current_level_) {
		post_pulse(Storage::Time(TZXClockMSMultiplier, StandardTZXClock));
		post_pulse(Storage::Time((milliseconds - 1u)*TZXClockMSMultiplier, StandardTZXClock));
	} else {
		post_pulse(Storage::Time(milliseconds*TZXClockMSMultiplier, StandardTZXClock));
	}
}

void TZX::post_pulse(const Storage::Time &time) {
	emplace_back(current_level_ ? Tape::Pulse::High : Tape::Pulse::Low, time);
	current_level_ ^= true;
}

// MARK: - Flow control; currently ignored

void TZX::ignore_group_start() {
	uint8_t length = file_.get8();
	file_.seek(length, SEEK_CUR);
}

void TZX::ignore_group_end() {
}

void TZX::ignore_jump_to_block() {
	uint16_t target = file_.get16le();
	(void)target;
}

void TZX::ignore_loop_start() {
	uint16_t number_of_repetitions = file_.get16le();
	(void)number_of_repetitions;
}

void TZX::ignore_loop_end() {
}

void TZX::ignore_call_sequence() {
	uint16_t number_of_entries = file_.get16le();
	file_.seek(number_of_entries * sizeof(uint16_t), SEEK_CUR);
}

void TZX::ignore_return_from_sequence() {
}

void TZX::ignore_select_block() {
	uint16_t length_of_block = file_.get16le();
	file_.seek(length_of_block, SEEK_CUR);
}

void TZX::ignore_stop_tape_if_in_48kb_mode() {
	file_.seek(4, SEEK_CUR);
}

void TZX::ignore_custom_info_block() {
	file_.seek(0x10, SEEK_CUR);
	uint32_t length = file_.get32le();
	file_.seek(length, SEEK_CUR);
}

// MARK: - Messaging

void TZX::ignore_text_description() {
	uint8_t length = file_.get8();
	file_.seek(length, SEEK_CUR);
}

void TZX::ignore_message_block() {
	uint8_t time_for_display = file_.get8();
	uint8_t length = file_.get8();
	file_.seek(length, SEEK_CUR);
	(void)time_for_display;
}

void TZX::ignore_archive_info() {
	uint16_t length = file_.get16le();
	file_.seek(length, SEEK_CUR);
}

void TZX::get_hardware_type() {
	// TODO: pick a way to retain and communicate this.
	uint8_t number_of_machines = file_.get8();
	file_.seek(number_of_machines * 3, SEEK_CUR);
}

void TZX::ignore_glue_block() {
	file_.seek(9, SEEK_CUR);
}
