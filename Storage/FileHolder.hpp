//
//  FileHolder.hpp
//  Clock Signal
//
//  Created by Thomas Harte on 21/11/2016.
//  Copyright 2016 Thomas Harte. All rights reserved.
//

#ifndef FileHolder_hpp
#define FileHolder_hpp

#include <sys/stat.h>
#include <array>
#include <cstdio>
#include <cstdint>
#include <mutex>
#include <string>
#include <vector>

namespace Storage {

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
			Performs @c get8 four times on @c file, casting each result to a @c uint32_t
			and returning the four assembled in little endian order.
		*/
		uint32_t get32le();

		/*!
			Writes @c value using successive @c put8s, in little endian order.
		*/
		template <typename T> void put_le(T value) {
			auto bytes = sizeof(T);
			while(bytes--) {
				put8(value&0xff);
				value >>= 8;
			}
		}

		/*!
			Writes @c value using successive @c put8s, in big endian order.
		*/
		template <typename T> void put_be(T value) {
			auto shift = sizeof(T) * 8;
			while(shift) {
				shift -= 8;
				put8((value >> shift)&0xff);
			}
		}

		/*!
			Performs @c get8 four times on @c file, casting each result to a @c uint32_t
			and returning the four assembled in big endian order.
		*/
		uint32_t get32be();

		/*!
			Performs @c get8 three times on @c file, casting each result to a @c uint32_t
			and returning the three assembled in little endian order.
		*/
		uint32_t get24le();

		/*!
			Performs @c get8 three times on @c file, casting each result to a @c uint32_t
			and returning the three assembled in big endian order.
		*/
		uint32_t get24be();

		/*!
			Performs @c get8 two times on @c file, casting each result to a @c uint32_t
			and returning the two assembled in little endian order.
		*/
		uint16_t get16le();

		/*!
			Writes @c value using two successive @c put8s, in little endian order.
		*/
		void put16le(uint16_t value);

		/*!
			Performs @c get8 two times on @c file, casting each result to a @c uint32_t
			and returning the two assembled in big endian order.
		*/
		uint16_t get16be();

		/*!
			Writes @c value using two successive @c put8s, in big endian order.
		*/
		void put16be(uint16_t value);

		/*! Reads a single byte from @c file. */
		uint8_t get8();

		/*! Writes a single byte from @c file. */
		void put8(uint8_t value);

		/*! Writes @c value a total of @c repeats times. */
		void putn(std::size_t repeats, uint8_t value);

		/*! Reads @c size bytes and returns them as a vector. */
		std::vector<uint8_t> read(std::size_t size);

		/*! Reads @c a.size() bytes into @c a.data(). */
		template <size_t size> std::size_t read(std::array<uint8_t, size> &a) {
			return read(a.data(), a.size());
		}

		/*! Reads @c size bytes and writes them to @c buffer. */
		std::size_t read(uint8_t *buffer, std::size_t size);

		/*! Writes @c buffer one byte at a time in order. */
		std::size_t write(const std::vector<uint8_t> &buffer);

		/*! Writes @c buffer one byte at a time in order, writing @c size bytes in total. */
		std::size_t write(const uint8_t *buffer, std::size_t size);

		/*! Moves @c bytes from the anchor indicated by @c whence: SEEK_SET, SEEK_CUR or SEEK_END. */
		void seek(long offset, int whence);

		/*! @returns The current cursor position within this file. */
		long tell();

		/*! Flushes any queued content that has not yet been written to disk. */
		void flush();

		/*! @returns @c true if the end-of-file indicator is set, @c false otherwise. */
		bool eof();

		class BitStream {
			public:
				uint8_t get_bits(int q) {
					uint8_t result = 0;
					while(q--) {
						result = uint8_t((result << 1) | get_bit());
					}
					return result;
				}

			private:
				BitStream(FILE *file, bool lsb_first) :
					file_(file),
					lsb_first_(lsb_first),
					next_value_(0),
					bits_remaining_(0) {}
				friend FileHolder;

				FILE *file_;
				bool lsb_first_;
				uint8_t next_value_;
				int bits_remaining_;

				uint8_t get_bit() {
					if(!bits_remaining_) {
						bits_remaining_ = 8;
						next_value_ = uint8_t(fgetc(file_));
					}

					uint8_t bit;
					if(lsb_first_) {
						bit = next_value_ & 1;
						next_value_ >>= 1;
					} else {
						bit = next_value_ >> 7;
						next_value_ <<= 1;
					}

					bits_remaining_--;

					return bit;
				}
		};

		/*!
			Obtains a BitStream for reading from the file from the current reading cursor.
		*/
		BitStream get_bitstream(bool lsb_first);

		/*!
			Reads @c length bytes from the file and compares them to the first
			@c length bytes of @c signature. If @c length is 0, it is computed
			as the length of @c signature not including the terminating null.

			@returns @c true if the bytes read match the signature; @c false otherwise.
		*/
		bool check_signature(const char *signature, std::size_t length = 0);

		/*!
			Determines and returns the file extension: everything from the final character
			back to the first dot. The string is converted to lowercase before being returned.
		*/
		std::string extension();

		/*!
			Ensures the file is at least @c length bytes long, appending 0s until it is
			if necessary.
		*/
		void ensure_is_at_least_length(long length);

		/*!
			@returns @c true if an attempt was made to read this file in ReadWrite mode but it could be opened only for reading; @c false otherwise.
		*/
		bool get_is_known_read_only();

		/*!
			@returns the stat struct describing this file.
		*/
		struct stat &stats();

		/*!
			@returns a mutex owned by the file that can be used to serialise file access.
		*/
		std::mutex &get_file_access_mutex();

	private:
		FILE *file_ = nullptr;
		const std::string name_;

		struct stat file_stats_;
		bool is_read_only_ = false;

		std::mutex file_access_mutex_;
};

}

#endif /* FileHolder_hpp */
