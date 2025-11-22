//
//  FileHolder.cpp
//  Clock Signal
//
//  Created by Thomas Harte on 21/11/2016.
//  Copyright 2016 Thomas Harte. All rights reserved.
//

#include "FileHolder.hpp"

#include <algorithm>
#include <cassert>

using namespace Storage;

FileHolder::FileHolder(FileHolder &&rhs) {
	file_ = rhs.file_;
	rhs.file_ = nullptr;
	// TODO: this leaves the RHS in an invalid state, which isn't appropriate for move semantics.
}

FileHolder::~FileHolder() {
	if(file_) std::fclose(file_);
}

FileHolder::FileHolder(const std::string &file_name, const FileMode ideal_mode)
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

uint8_t FileHolder::get() {
	return uint8_t(std::fgetc(file_));
}

bool FileHolder::put(const uint8_t value) {
	return std::fputc(value, file_) == value;
}

void FileHolder::putn(std::size_t repeats, const uint8_t value) {
	while(repeats--) put(value);
}

std::vector<uint8_t> FileHolder::read(const std::size_t size) {
	std::vector<uint8_t> result(size);
	result.resize(std::fread(result.data(), 1, size, file_));
	return result;
}

std::size_t FileHolder::read(uint8_t *const buffer, const std::size_t size) {
	return std::fread(buffer, 1, size, file_);
}

std::size_t FileHolder::write(const std::vector<uint8_t> &buffer) {
	return std::fwrite(buffer.data(), 1, buffer.size(), file_);
}

std::size_t FileHolder::write(const uint8_t *buffer, const std::size_t size) {
	return std::fwrite(buffer, 1, size, file_);
}

void FileHolder::seek(const long offset, const Whence whence) {
	[[maybe_unused]] const auto result = std::fseek(file_, offset, int(whence));
	assert(!result);
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

std::string FileHolder::extension() const {
	const auto final_dot = name_.rfind('.');
	if(final_dot == std::string::npos) {
		return "";
	}

	std::string extension = name_.substr(final_dot + 1);
	std::transform(extension.begin(), extension.end(), extension.begin(), ::tolower);
	return extension;
}

const std::string &FileHolder::name() const {
	return name_;
}

void FileHolder::ensure_is_at_least_length(const long length) {
	std::fseek(file_, 0, SEEK_END);
	long bytes_to_write = length - ftell(file_);
	if(bytes_to_write > 0) {
		std::vector<uint8_t> empty(size_t(bytes_to_write), 0);
		std::fwrite(empty.data(), sizeof(uint8_t), size_t(bytes_to_write), file_);
	}
}

bool FileHolder::is_known_read_only() const {
	return is_read_only_;
}

const struct stat &FileHolder::stats() const {
	return file_stats_;
}

std::mutex &FileHolder::file_access_mutex() {
	return file_access_mutex_;
}
