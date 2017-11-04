//
//  FileHolder.hpp
//  Clock Signal
//
//  Created by Thomas Harte on 21/11/2016.
//  Copyright © 2016 Thomas Harte. All rights reserved.
//

#ifndef FileHolder_hpp
#define FileHolder_hpp

#include <sys/stat.h>
#include <cstdio>
#include <cstdint>
#include <mutex>
#include <string>
#include <vector>

namespace Storage {

class FileHolder final {
	public:
		enum {
			ErrorCantOpen = -1
		};
	
		enum class FileMode {
			ReadWrite,
			Read,
			Rewrite
		};

		~FileHolder();
		FileHolder(const std::string &file_name, FileMode ideal_mode = FileMode::ReadWrite);

		/*!
			Performs @c get8 four times on @c file, casting each result to a @c uint32_t
			and returning the four assembled in little endian order.
		*/
		uint32_t get32le();

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
		void put16le(uint16_t value);

		/*!
			Performs @c get8 two times on @c file, casting each result to a @c uint32_t
			and returning the two assembled in big endian order.
		*/
		uint16_t get16be();
		void put16be(uint16_t value);

		/*! Reads a single byte from @c file. */
		uint8_t get8();

		/*! Writes a single byte from @c file. */
		void put8(uint8_t value);
		void putn(size_t repeats, uint8_t value);

		std::vector<uint8_t> read(size_t size);
		size_t read(uint8_t *buffer, size_t size);
		size_t write(const std::vector<uint8_t> &);
		size_t write(const uint8_t *buffer, size_t size);

		void seek(long offset, int whence);
		long tell();
		void flush();
		bool eof();

		class BitStream {
			public:
				uint8_t get_bits(int q) {
					uint8_t result = 0;
					while(q--) {
						result = static_cast<uint8_t>((result << 1) | get_bit());
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
						next_value_ = static_cast<uint8_t>(fgetc(file_));
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
		bool check_signature(const char *signature, size_t length = 0);

		/*!
			Determines and returns the file extension — everything from the final character
			back to the first dot. The string is converted to lowercase before being returned.
		*/
		std::string extension();

		/*!
			Ensures the file is at least @c length bytes long, appending 0s until it is
			if necessary.
		*/
		void ensure_is_at_least_length(long length);

		/*!
			@returns @c true if this file is read-only; @c false otherwise.
		*/
		bool get_is_known_read_only();

		/*!
			@returns the stat struct describing this file.
		*/
		struct stat &stats();
	
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
