//
//  FileHolder.cpp
//  Clock Signal
//
//  Created by Thomas Harte on 21/11/2016.
//  Copyright 2016 Thomas Harte. All rights reserved.
//

#include "FileHolder.hpp"

#include <algorithm>
#include <cstring>

using namespace Storage;

FileHolder::~FileHolder() {
	if(file_) std::fclose(file_);
}

FileHolder::FileHolder(const std::string &file_name, FileMode ideal_mode)
	: name_(file_name) {
	stat(file_name.c_str(), &file_stats_);
	is_read_only_ = false;

	switch(ideal_mode) {
		case FileMode::ReadWrite:
			file_ = std::fopen(file_name.c_str(), "rb+");
			if(file_) break;
			is_read_only_ = true;
			[[fallthrough]];

		case FileMode::Read:
			file_ = std::fopen(file_name.c_str(), "rb");
		break;

		case FileMode::Rewrite:
			file_ = std::fopen(file_name.c_str(), "w");
		break;
	}

	if(!file_) throw Error::CantOpen;
}

uint8_t FileHolder::get8() {
	return uint8_t(std::fgetc(file_));
}

void FileHolder::put8(uint8_t value) {
	std::fputc(value, file_);
}

void FileHolder::putn(std::size_t repeats, uint8_t value) {
	while(repeats--) put8(value);
}

std::vector<uint8_t> FileHolder::read(std::size_t size) {
	std::vector<uint8_t> result(size);
	result.resize(std::fread(result.data(), 1, size, file_));
	return result;
}

std::size_t FileHolder::read(uint8_t *buffer, std::size_t size) {
	return std::fread(buffer, 1, size, file_);
}

std::size_t FileHolder::write(const std::vector<uint8_t> &buffer) {
	return std::fwrite(buffer.data(), 1, buffer.size(), file_);
}

std::size_t FileHolder::write(const uint8_t *buffer, std::size_t size) {
	return std::fwrite(buffer, 1, size, file_);
}

void FileHolder::seek(long offset, int whence) {
	std::fseek(file_, offset, whence);
}

long FileHolder::tell() const {
	return std::ftell(file_);
}

void FileHolder::flush() {
	std::fflush(file_);
}

bool FileHolder::eof() const {
	return std::feof(file_);
}

bool FileHolder::check_signature(const char *signature, std::size_t length) {
	if(!length) length = std::strlen(signature);

	// read and check the file signature
	std::vector<uint8_t> stored_signature = read(length);
	if(stored_signature.size() != length)							return false;
	if(std::memcmp(stored_signature.data(), signature, length))		return false;
	return true;
}

std::string FileHolder::extension() const {
	std::size_t pointer = name_.size() - 1;
	while(pointer > 0 && name_[pointer] != '.') pointer--;
	if(name_[pointer] == '.') pointer++;

	std::string extension = name_.substr(pointer);
	std::transform(extension.begin(), extension.end(), extension.begin(), ::tolower);
	return extension;
}

const std::string &FileHolder::name() const {
	return name_;
}

void FileHolder::ensure_is_at_least_length(long length) {
	std::fseek(file_, 0, SEEK_END);
	long bytes_to_write = length - ftell(file_);
	if(bytes_to_write > 0) {
		std::vector<uint8_t> empty(size_t(bytes_to_write), 0);
		std::fwrite(empty.data(), sizeof(uint8_t), size_t(bytes_to_write), file_);
	}
}

bool FileHolder::get_is_known_read_only() const {
	return is_read_only_;
}

const struct stat &FileHolder::stats() const {
	return file_stats_;
}

std::mutex &FileHolder::get_file_access_mutex() {
	return file_access_mutex_;
}
