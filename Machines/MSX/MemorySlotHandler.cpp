//
//  MemorySlotHandler.cpp
//  Clock Signal
//
//  Created by Thomas Harte on 12/01/2023.
//  Copyright © 2023 Thomas Harte. All rights reserved.
//

#include "MemorySlotHandler.hpp"

#include <cassert>

using namespace MSX;

PrimarySlot::PrimarySlot(MemorySlotChangeHandler &handler) :
	subslots_{handler, handler, handler, handler} {}

MemorySlot::MemorySlot(MemorySlotChangeHandler &handler) : handler_(handler) {
	for(int region = 0; region < 8; region++) {
		read_pointers_[region] = unmapped.data();
		write_pointers_[region] = scratch.data();
	}
}

void PrimarySlot::set_secondary_paging(uint8_t value) {
	secondary_paging_ = value;
}

uint8_t PrimarySlot::secondary_paging() const {
	return secondary_paging_;
}

const uint8_t *PrimarySlot::read_pointer(int segment) const {
	const int subslot = (secondary_paging_ >> (segment & ~1)) & 3;
	return subslots_[subslot].read_pointer(segment);
}

uint8_t *PrimarySlot::write_pointer(int segment) const {
	const int subslot = (secondary_paging_ >> (segment & ~1)) & 3;
	return subslots_[subslot].write_pointer(segment);
}

const uint8_t *MemorySlot::read_pointer(int segment) const {
	return read_pointers_[segment];
}

uint8_t *MemorySlot::write_pointer(int segment) const {
	return write_pointers_[segment];
}

void MemorySlot::set_source(const std::vector<uint8_t> &source) {
	source_ = source;
}

void MemorySlot::resize_source(std::size_t size) {
	source_.resize(size);
}

std::vector<uint8_t> &MemorySlot::source() {
	return source_;
}

const std::vector<uint8_t> &MemorySlot::source() const {
	return source_;
}

template <MSX::MemorySlot::AccessType type>
void MemorySlot::map(std::size_t source_address, uint16_t destination_address, std::size_t length) {
	assert(!(destination_address & 8191));
	assert(!(length & 8191));
	assert(size_t(destination_address) + length <= 65536);

	for(std::size_t c = 0; c < (length >> 13); ++c) {
		source_address %= source_.size();

		const int bank = int((destination_address >> 13) + c);
		read_pointers_[bank] = &source_[source_address];
		if constexpr (type == AccessType::ReadWrite) {
			write_pointers_[bank] = read_pointers_[bank];
		}

		source_address += 8192;
	}

	handler_.did_page();
}

void MemorySlot::unmap(uint16_t destination_address, std::size_t length) {
	assert(!(destination_address & 8191));
	assert(!(length & 8191));
	assert(size_t(destination_address) + length <= 65536);

	for(std::size_t c = 0; c < (length >> 13); ++c) {
		read_pointers_[(destination_address >> 13) + c] = nullptr;
	}

	handler_.did_page();
}

MemorySlot &PrimarySlot::subslot(int slot) {
	return subslots_[slot];
}

template void MemorySlot::map<MSX::MemorySlot::AccessType::Read>(std::size_t source_address, uint16_t destination_address, std::size_t length);
template void MemorySlot::map<MSX::MemorySlot::AccessType::ReadWrite>(std::size_t source_address, uint16_t destination_address, std::size_t length);
