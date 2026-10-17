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
#include <bit>
#include <cstdio>
#include <cstdint>
#include <cstring>
#include <filesystem>
#include <mutex>
#include <string>
#include <vector>

namespace Storage {

enum class Whence: int {
	CUR = SEEK_CUR,
	SET = SEEK_SET,
	END = SEEK_END,
};

enum class SignatureType {
	String,
	Binary,
};

enum class FileMode {
	ReadWrite,
	Read,
	Rewrite
};

class FileHolder final {
public:
	enum class Error {
		CantOpen = -1
	};

	~FileHolder();

	/*!
		Attempts to open the file indicated. @c ideal_mode nominates how the file would
		most ideally be opened. It can be one of:

			ReadWrite	attempt to open this file for random access reading and writing. If that fails,
						will attept to open in Read mode.
			Read		attempts to open this file for reading only.
			Rewrite		opens the file for rewriting; none of the original content is preserved; whatever
						the caller outputs will replace the existing file.

		@throws Error::CantOpen if the file cannot be opened.
	*/
	FileHolder(const std::filesystem::path &, FileMode ideal_mode = FileMode::ReadWrite);
	FileHolder(FileHolder &&);

	/*! Reads a single byte from @c file. */
	uint8_t get();

	/*!
		Writes a single byte from @c file.
		@returns @c true on success; @c false on failure.
	*/
	bool put(uint8_t);

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
	std::size_t write(const void *, std::size_t);

	/*! Moves @c bytes from the anchor indicated by @c whence: SEEK_SET, SEEK_CUR or SEEK_END. */
	bool seek(long offset, Whence);

	/*! @returns The current cursor position within this file. */
	long tell() const;

	/*! Flushes any queued content that has not yet been written to disk. */
	void flush();

	/*! @returns @c true if the end-of-file indicator is set, @c false otherwise. */
	bool eof() const;


	/*!
		For SignatureType::Binary:

		compares every byte in @c signature to an incoming byte from the file,
		returning @c true if they matched and @c false otherwise.

		For SignatureType::String:

		compares all bytes but the final in @c signature to an incoming byte from the file,
		returning @c true if they matched and @c false otherwise. This therefore assumes
		that the final byte was a NULL terminator, which is not represented in the file.
	*/
	template <SignatureType type, size_t buffer_size>
	bool check_signature(const char (&signature)[buffer_size]) {
		// Discard C-style trailing NULL if this is a string compare.
		static constexpr auto signature_length = buffer_size - (type == SignatureType::String ? 1 : 0);

		std::array<uint8_t, signature_length> stored_signature;
		if(read(stored_signature) != signature_length) {
			return false;
		}
		return !std::memcmp(stored_signature.data(), signature, signature_length);
	}

	/*!
		Returns the underlying file name.
	*/
	const std::filesystem::path &path() const;

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
	const std::filesystem::path path_;

	struct stat file_stats_;
	bool is_read_only_ = false;

	std::mutex file_access_mutex_;
};

inline std::vector<uint8_t> contents_of(const std::filesystem::path &path) {
	FileHolder file(path, FileMode::Read);
	return file.read(size_t(file.stats().st_size));
}

/*!
	Various free functions that work with an open file.
*/
namespace FileUtils {

/*!
	Obtains a BitStream for reading from the file from the current reading cursor.
*/
template <int max_bits, bool lsb_first>
Numeric::BitStream<max_bits, lsb_first> bitstream(FileHolder &file) {
	return Numeric::BitStream<max_bits, lsb_first>([&] {
		return file.get();
	});
}

/*!
	A local implementation of std::byteswap, which I cannot currently use because it's C++23.
*/
template <typename IntT>
requires (sizeof(IntT) == 1 || sizeof(IntT) == 2 || sizeof(IntT) == 4 || sizeof(IntT) == 8)
IntT byteswap(IntT value) {
	if constexpr (sizeof(IntT) == 1) {
		return value;
	}
	if constexpr (sizeof(IntT) == 2) {
		return __builtin_bswap16(value);
	}
	if constexpr (sizeof(IntT) == 4) {
		return __builtin_bswap32(value);
	}
	if constexpr (sizeof(IntT) == 8) {
		return __builtin_bswap64(value);
	}
}

/*!
	Writes @c value to @c file according to @c endian.
	Optionally limits itself to only @c size bytes.
*/
template <std::endian endian, typename IntT, size_t size = sizeof(IntT)>
void put(FileHolder &file, IntT value) {
	if constexpr (size != sizeof(IntT) && endian == std::endian::big) {
		// Little on little: relevant bytes are the first ones.
		// Little on big: low bytes were swapped into high. Still the first ones. Do nothing.

		// Big on little: pull bytes up to top ready to be swapped into bottom.
		// Big on bug: pull meaningful bytes up to the front.
		value <<= ((sizeof(IntT) - size) / 8);
	}

	if constexpr (endian != std::endian::native && sizeof(IntT) > 1) {
		value = byteswap<IntT>(value);
	}

	file.write(&value, size);
}

/*!
	Reads a value of type @c IntT from @c file using @c endian byte order.
	Optionally limits itself to only @c size bytes.
*/
template <std::endian endian, typename IntT, size_t size = sizeof(IntT)>
IntT get(FileHolder &file) {
	IntT result;
	if constexpr (size != sizeof(IntT)) {
		result = 0;
	}

	file.read(reinterpret_cast<uint8_t *>(&result), size);
	if constexpr (endian != std::endian::native) {
		result = byteswap<IntT>(result);
	}

	if constexpr (size != sizeof(IntT) && endian == std::endian::big) {
		// Little on little: read into low bytes. Do nothing.
		// Little on big: read into high bytes, swapped into low. Do nothing.
		// Big on little: read into low bytes, swapped into high. Shift downward.
		// Big on big: read into high bytes. Shift downward.
		result >>= ((sizeof(IntT) - size) / 8);
	}

	return result;
}

/*
	Helpers below are designed to allow more-compact syntax elsewhere; e.g.
	`using FileUtils::LittleEndian::get`.
*/
namespace LittleEndian {

template <std::unsigned_integral IntT, size_t size = sizeof(IntT)>
IntT get(Storage::FileHolder &file) {
	return Storage::FileUtils::get<std::endian::little, IntT, size>(file);
}

template <typename IntT, size_t size = sizeof(IntT)>
void put(FileHolder &file, IntT value) {
	return Storage::FileUtils::put<std::endian::little, IntT, size>(file, value);
}

}

namespace BigEndian {

template <std::unsigned_integral IntT, size_t size = sizeof(IntT)>
IntT get(Storage::FileHolder &file) {
	return Storage::FileUtils::get<std::endian::big, IntT, size>(file);
}

template <typename IntT, size_t size = sizeof(IntT)>
void put(FileHolder &file, IntT value) {
	return Storage::FileUtils::put<std::endian::big, IntT, size>(file, value);
}

}

}
}
