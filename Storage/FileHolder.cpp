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

FileHolder::~FileHolder()
{
	if(file_) fclose(file_);
}

FileHolder::FileHolder(const char *file_name) : file_(nullptr)
{
	stat(file_name, &file_stats_);
	is_read_only_ = false;
	file_ = fopen(file_name, "rb+");
	if(!file_)
	{
		is_read_only_ = true;
		file_ = fopen(file_name, "rb");
	}
	if(!file_) throw ErrorCantOpen;
}

bool FileHolder::check_signature(const char *signature, size_t length)
{
	if(!length) length = strlen(signature)+1;

	// read and check the file signature
	char stored_signature[12];
	if(fread(stored_signature, 1, length, file_) != length)	return false;
	if(memcmp(stored_signature, signature, length)) return false;
	return true;
}

uint32_t FileHolder::fgetc32le()
{
	uint32_t result = (uint32_t)fgetc(file_);
	result |= (uint32_t)(fgetc(file_) << 8);
	result |= (uint32_t)(fgetc(file_) << 16);
	result |= (uint32_t)(fgetc(file_) << 24);

	return result;
}

uint32_t FileHolder::fgetc24le()
{
	uint32_t result = (uint32_t)fgetc(file_);
	result |= (uint32_t)(fgetc(file_) << 8);
	result |= (uint32_t)(fgetc(file_) << 16);

	return result;
}

uint16_t FileHolder::fgetc16le()
{
	uint16_t result = (uint16_t)fgetc(file_);
	result |= (uint16_t)(fgetc(file_) << 8);

	return result;
}

uint32_t FileHolder::fgetc32be()
{
	uint32_t result = (uint32_t)(fgetc(file_) << 24);
	result |= (uint32_t)(fgetc(file_) << 16);
	result |= (uint32_t)(fgetc(file_) << 8);
	result |= (uint32_t)fgetc(file_);

	return result;
}

uint16_t FileHolder::fgetc16be()
{
	uint16_t result = (uint16_t)(fgetc(file_) << 8);
	result |= (uint16_t)fgetc(file_);

	return result;
}

void FileHolder::ensure_file_is_at_least_length(long length)
{
	fseek(file_, 0, SEEK_END);
	long bytes_to_write = length - ftell(file_);
	if(bytes_to_write > 0)
	{
		uint8_t *empty = new uint8_t[bytes_to_write];
		memset(empty, 0, (size_t)bytes_to_write);
		fwrite(empty, sizeof(uint8_t), (size_t)bytes_to_write, file_);
		delete[] empty;
	}
}
