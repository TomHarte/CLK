//
//  FileHolder.hpp
//  Clock Signal
//
//  Created by Thomas Harte on 21/11/2016.
//  Copyright 2016 Thomas Harte. All rights reserved.
//

#pragma once

#include "Numeric/BitStream.hpp"
#include <sys/stat.h>
#include <array>
#include <cstdio>
#include <cstdint>
#include <mutex>
#include <string>
#include <vector>

namespace Storage {

enum class Whence: int {
	CUR = SEEK_CUR,
	SET = SEEK_SET,
	END = SEEK_END,
};

class FileHolder final {
public:
	enum class Error {
		CantOpen = -1
	};

	enum class FileMode {
		ReadWrite,
		Read,
		Rewrite
	};

	~FileHolder();

	/*!
		Attempts to open the file indicated by @c file_name. @c ideal_mode nominates how the file would
		most ideally be opened. It can be one of:

			ReadWrite	attempt to open this file for random access reading and writing. If that fails,
						will attept to open in Read mode.
			Read		attempts to open this file for reading only.
			Rewrite		opens the file for rewriting; none of the original content is preserved; whatever
						the caller outputs will replace the existing file.

		@throws ErrorCantOpen if the file cannot be opened.
	*/
	FileHolder(const std::string &file_name, FileMode ideal_mode = FileMode::ReadWrite);

	/*!
		Writes @c value using successive @c puts, in little endian order.
		Optionally limits itself to only @c size bytes.
	*/
	template <typename IntT, size_t size = sizeof(IntT)>
	void put_le(IntT value) {
		for(size_t c = 0; c < size; c++) {
			put(uint8_t(value));
			value >>= 8;
		}
	}

	/*!
		Writes @c value using successive @c puts, in big endian order.
		Optionally limits itself to only @c size bytes.
	*/
	template <typename IntT, size_t size = sizeof(IntT)>
	void put_be(IntT value) {
		auto shift = size * 8;
		while(shift) {
			shift -= 8;
			put((value >> shift)&0xff);
		}
	}

	/*!
		Reads a value of type @c IntT using successive @c gets, in little endian order.
		Optionally limits itself to only @c size bytes.
	*/
	template <typename IntT, size_t size = sizeof(IntT)>
	IntT get_le() {
		IntT result{};
		for(size_t c = 0; c < size; c++) {
			result >>= 8;
			result |= IntT(get() << ((size - 1) * 8));
		}
		return result;
	}

	/*!
		Reads a value of type @c IntT using successive @c gets, in big endian order.
		Optionally limits itself to only @c size bytes.
	*/
	template <typename IntT, size_t size = sizeof(IntT)>
	IntT get_be() {
		IntT result{};
		for(size_t c = 0; c < size; c++) {
			result <<= 8;
			result |= get();
		}
		return result;
	}

	/*! Reads a single byte from @c file. */
	uint8_t get();

	/*! Writes a single byte from @c file. */
	void put(uint8_t);

	/*! Writes @c value a total of @c repeats times. */
	void putn(std::size_t repeats, uint8_t value);

	/*! Reads @c size bytes and returns them as a vector. */
	std::vector<uint8_t> read(std::size_t);

	/*! Reads @c a.size() bytes into @c a.data(). */
	template <size_t size> std::size_t read(std::array<uint8_t, size> &a) {
		return read(a.data(), a.size());
	}

	/*! Reads @c size bytes and writes them to @c buffer. */
	std::size_t read(uint8_t *, std::size_t);

	/*! Writes @c buffer one byte at a time in order. */
	std::size_t write(const std::vector<uint8_t> &);

	/*! Writes @c buffer one byte at a time in order, writing @c size bytes in total. */
	std::size_t write(const uint8_t *, std::size_t);

	/*! Moves @c bytes from the anchor indicated by @c whence: SEEK_SET, SEEK_CUR or SEEK_END. */
	void seek(long offset, Whence);

	/*! @returns The current cursor position within this file. */
	long tell() const;

	/*! Flushes any queued content that has not yet been written to disk. */
	void flush();

	/*! @returns @c true if the end-of-file indicator is set, @c false otherwise. */
	bool eof() const;

	/*!
		Obtains a BitStream for reading from the file from the current reading cursor.
	*/
	template <int max_bits, bool lsb_first>
	Numeric::BitStream<max_bits, lsb_first> bitstream() {
		return Numeric::BitStream<max_bits, lsb_first>([&] {
			return get();
		});
	}

	/*!
		Reads @c length bytes from the file and compares them to the first
		@c length bytes of @c signature. If @c length is 0, it is computed
		as the length of @c signature not including the terminating null.

		@returns @c true if the bytes read match the signature; @c false otherwise.
	*/
	template <size_t size>
	bool check_signature(const char (&signature)[size]) {
		constexpr auto signature_length = size - 1;

		std::array<uint8_t, size> stored_signature;
		if(read(stored_signature) != size) {
			return false;
		}
		return !std::memcmp(stored_signature.data(), signature, signature_length);
	}

	/*!
		Determines and returns the file extension: everything from the final character
		back to the first dot. The string is converted to lowercase before being returned.
	*/
	std::string extension() const;

	/*!
		Returns the underlying file name.
	*/
	const std::string &name() const;

	/*!
		Ensures the file is at least @c length bytes long, appending 0s until it is
		if necessary.
	*/
	void ensure_is_at_least_length(long length);

	/*!
		@returns @c true if an attempt was made to read this file in ReadWrite mode but it could be opened only for reading; @c false otherwise.
	*/
	bool is_known_read_only() const;

	/*!
		@returns the stat struct describing this file.
	*/
	const struct stat &stats() const;

	/*!
		@returns a mutex owned by the file that can be used to serialise file access.
	*/
	std::mutex &file_access_mutex();

private:
	FILE *file_ = nullptr;
	const std::string name_;

	struct stat file_stats_;
	bool is_read_only_ = false;

	std::mutex file_access_mutex_;
};

inline std::vector<uint8_t> contents_of(const std::string &file_name) {
	FileHolder file(file_name, FileHolder::FileMode::Read);
	return file.read(size_t(file.stats().st_size));
}

}
