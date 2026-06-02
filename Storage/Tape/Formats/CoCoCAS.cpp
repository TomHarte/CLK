//
//  CoCoCAS.cpp
//  Clock Signal
//
//  Created by Thomas Harte on 07/05/2026.
//  Copyright © 2026 Thomas Harte. All rights reserved.
//

#include "CoCoCAS.hpp"

using namespace Storage::Tape;

/*
	[CoCo-style] CAS files are a raw dump of source bytes as per Microsoft's 6809 BASIC and
	the Tandy (or Dragon) encoding.

	It is therefore very similar to the Thomson K7 file format, but weird:

		Contents are not necessarily byte-aligned with original content; within a file block
		it's an arbitrarily-aligned bitstream.

		... but, most files abbreviate the sync periods. So you've still to apply
		ROM-style formatting.
*/
CoCoCAS::CoCoCAS(const std::string &file_name) {
	struct Shifter {
		Shifter(const std::string &file_name) : file_(file_name, FileMode::Read) {}

		uint32_t value() const {
			return uint32_t(shifter_);
		}

		void advance(size_t length) {
			while(length--) shift();
		}

		void shift() {
			if(!depth_ && file_.eof()) {
				return;
			}

			shifter_ >>= 1;
			--depth_;
			++shifted_;
			while(depth_ <= 56 && !file_.eof()) {
				shifter_ |= uint64_t(file_.get()) << depth_;
				depth_ += 8;
			}
		}

		size_t offset() const {
			return shifted_;
		}

		void set_offset(const size_t offset) {
			file_.seek(0, Whence::SET);
			shifted_ = 0;
			advance(offset);
		}

		bool eof() const {
			return file_.eof() && !depth_;
		}

	private:
		Storage::FileHolder file_;

		uint64_t shifter_ = 0;
		int depth_ = 0;
		size_t shifted_ = 0;
	};

	Shifter shifter(file_name);
	while(!shifter.eof()) {
		// Find next sync byte.
		while(!shifter.eof() && (shifter.value() & 0xff) != 0x3c) {
			shifter.shift();
		}
		const auto offset = shifter.offset();
		if(shifter.eof()) break;

		auto &block = blocks_.emplace_back();
		shifter.advance(8);
		const auto type = uint8_t(shifter.value());

		shifter.advance(8);
		const auto length = uint8_t(shifter.value());

		block.data.reserve(length + 3);
		block.data.push_back(0x3c);
		block.data.push_back(type);
		block.data.push_back(length);

		for(int c = 0; c <= length; c++) {
			shifter.advance(8);
			block.data.push_back(uint8_t(shifter.value()));
		}

		const auto checksum = uint8_t(std::accumulate(block.data.begin() + 1, block.data.end() - 1, 0));
		if(checksum != block.data.back()) {
			blocks_.pop_back();
			shifter.set_offset(offset + 1);
		}
	}

	if(blocks_.empty()) {
		throw ErrorBadFormat;
	}
}

std::unique_ptr<FormatSerialiser> CoCoCAS::format_serialiser() const {
	return std::make_unique<Serialiser>(blocks_);
}

CoCoCAS::Serialiser::Serialiser(const std::vector<Block> &blocks) : blocks_(blocks) {
	reset();
}

void CoCoCAS::Serialiser::reset() {
	block_ = blocks_.begin();
	state_ = State::LeadIn;
	state_length_ = 0;
}

void CoCoCAS::Serialiser::push_next_pulses() {
	const auto post_bit = [&](const bool bit) {
		// Generate a single wave of either 1200Hz (for a 0) or 2400Hz tone (for a 1).
		const Time length(
			1,
			bit ? 4800 : 2400
		);
		emplace_back(Pulse::Low, length);
		emplace_back(Pulse::High, length);
	};

	const auto serialise = [&](uint8_t next) {
		for(int c = 0; c < 8; c++) {
			post_bit(next & 1);
			next >>= 1;
		}
	};

	switch(state_) {
		case State::LeadIn:
			serialise(0x55);
			++state_length_;
			if(state_length_ == 150 && block_ != blocks_.end()) {
				state_ = State::Body;
				state_length_ = 0;
			}
		break;

		case State::Body:
			serialise(block_->data[state_length_]);
			++state_length_;
			if(state_length_ == block_->data.size()) {
				state_ = State::LeadIn;
				state_length_ = 0;
				++block_;
			}
		break;

		default: __builtin_unreachable();
	}
}
