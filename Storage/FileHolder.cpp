//
//  FileHolder.cpp
//  Clock Signal
//
//  Created by Thomas Harte on 21/11/2016.
//  Copyright © 2016 Thomas Harte. All rights reserved.
//

#include "FileHolder.hpp"

#include <cstring>

using namespace Storage;

FileHolder::~FileHolder() {
	if(file_) fclose(file_);
}

FileHolder::FileHolder(const std::string &file_name, FileMode ideal_mode)
	: name_(file_name) {
	stat(file_name.c_str(), &file_stats_);
	is_read_only_ = false;

	switch(ideal_mode) {
		case FileMode::ReadWrite:
			file_ = fopen(file_name.c_str(), "rb+");
			if(file_) break;
			is_read_only_ = true;

		// deliberate fallthrough...
		case FileMode::Read:
			file_ = fopen(file_name.c_str(), "rb");
		break;

		case FileMode::Rewrite:
			file_ = fopen(file_name.c_str(), "w");
		break;
	}

	if(!file_) throw ErrorCantOpen;
}

uint32_t FileHolder::get32le() {
    uint32_t result = (uint32_t)fgetc(file_);
    result |= (uint32_t)(fgetc(file_) << 8);
    result |= (uint32_t)(fgetc(file_) << 16);
    result |= (uint32_t)(fgetc(file_) << 24);

    return result;
}

uint32_t FileHolder::get32be() {
    uint32_t result = (uint32_t)(fgetc(file_) << 24);
    result |= (uint32_t)(fgetc(file_) << 16);
    result |= (uint32_t)(fgetc(file_) << 8);
    result |= (uint32_t)fgetc(file_);

    return result;
}

uint32_t FileHolder::get24le() {
    uint32_t result = (uint32_t)fgetc(file_);
    result |= (uint32_t)(fgetc(file_) << 8);
    result |= (uint32_t)(fgetc(file_) << 16);

    return result;
}

uint32_t FileHolder::get24be() {
    uint32_t result = (uint32_t)(fgetc(file_) << 16);
    result |= (uint32_t)(fgetc(file_) << 8);
    result |= (uint32_t)fgetc(file_);

    return result;
}

uint16_t FileHolder::get16le() {
    uint16_t result = static_cast<uint16_t>(fgetc(file_));
    result |= static_cast<uint16_t>(fgetc(file_) << 8);

    return result;
}

uint16_t FileHolder::get16be() {
    uint16_t result = static_cast<uint16_t>(fgetc(file_) << 8);
    result |= static_cast<uint16_t>(fgetc(file_));

    return result;
}

uint8_t FileHolder::get8() {
    return static_cast<uint8_t>(fgetc(file_));
}

void FileHolder::put16be(uint16_t value) {
	fputc(value >> 8, file_);
	fputc(value, file_);
}

void FileHolder::put16le(uint16_t value) {
	fputc(value, file_);
	fputc(value >> 8, file_);
}

void FileHolder::put8(uint8_t value) {
	fputc(value, file_);
}

void FileHolder::putn(size_t repeats, uint8_t value) {
	while(repeats--) put8(value);
}

std::vector<uint8_t> FileHolder::read(size_t size) {
	std::vector<uint8_t> result(size);
	fread(result.data(), 1, size, file_);
	return result;
}

size_t FileHolder::read(uint8_t *buffer, size_t size) {
    return fread(buffer, 1, size, file_);
}

size_t FileHolder::write(const std::vector<uint8_t> &buffer) {
	return fwrite(buffer.data(), 1, buffer.size(), file_);
}

size_t FileHolder::write(const uint8_t *buffer, size_t size) {
    return fwrite(buffer, 1, size, file_);
}

void FileHolder::seek(long offset, int whence) {
	fseek(file_, offset, whence);
}

long FileHolder::tell() {
	return ftell(file_);
}

void FileHolder::flush() {
	fflush(file_);
}

bool FileHolder::eof() {
	return feof(file_);
}

FileHolder::BitStream FileHolder::get_bitstream(bool lsb_first) {
	return BitStream(file_, lsb_first);
}

bool FileHolder::check_signature(const char *signature, size_t length) {
	if(!length) length = strlen(signature);

	// read and check the file signature
	std::vector<uint8_t> stored_signature = read(length);
	if(stored_signature.size() != length)					return false;
	if(memcmp(stored_signature.data(), signature, length)) 	return false;
	return true;
}

std::string FileHolder::extension() {
    size_t pointer = name_.size() - 1;
    while(pointer > 0 && name_[pointer] != '.') pointer--;
    if(name_[pointer] == '.') pointer++;

    std::string extension = name_.substr(pointer);
    std::transform(extension.begin(), extension.end(), extension.begin(), ::tolower);
    return extension;
}

void FileHolder::ensure_is_at_least_length(long length) {
    fseek(file_, 0, SEEK_END);
    long bytes_to_write = length - ftell(file_);
    if(bytes_to_write > 0) {
        uint8_t *empty = new uint8_t[static_cast<size_t>(bytes_to_write)];
        memset(empty, 0, static_cast<size_t>(bytes_to_write));
        fwrite(empty, sizeof(uint8_t), static_cast<size_t>(bytes_to_write), file_);
        delete[] empty;
    }
}

bool FileHolder::get_is_known_read_only() {
	return is_read_only_;
}

struct stat &FileHolder::stats() {
	return file_stats_;
}

std::mutex &FileHolder::get_file_access_mutex() {
	return file_access_mutex_;
}
