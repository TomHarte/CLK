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

namespace Storage {

class FileHolder {
	public:
		enum {
			ErrorCantOpen = -1
		};

		virtual ~FileHolder();

	protected:
		FileHolder(const std::string &file_name);

		/*!
			Reads @c length bytes from the file and compares them to the first
			@c length bytes of @c signature. If @c length is 0, it is computed
			as the length of @c signature up to and including the terminating null.
			
			@returns @c true if the bytes read match the signature; @c false otherwise.
		*/
		bool check_signature(const char *signature, size_t length);

		/*!
			Performs @c fgetc four times on @c file_, casting each result to a @c uint32_t
			and returning the four assembled in little endian order.
		*/
		uint32_t fgetc32le();

		/*!
			Performs @c fgetc three times on @c file_, casting each result to a @c uint32_t
			and returning the three assembled in little endian order.
		*/
		uint32_t fgetc24le();

		/*!
			Performs @c fgetc two times on @c file_, casting each result to a @c uint32_t
			and returning the two assembled in little endian order.
		*/
		uint16_t fgetc16le();

		/*!
			Performs @c fgetc four times on @c file_, casting each result to a @c uint32_t
			and returning the four assembled in big endian order.
		*/
		uint32_t fgetc32be();

		/*!
			Performs @c fgetc two times on @c file_, casting each result to a @c uint32_t
			and returning the two assembled in big endian order.
		*/
		uint16_t fgetc16be();

		/*!
			Determines and returns the file extension — everything from the final character
			back to the first dot. The string is converted to lowercase before being returned.
		*/
		std::string extension();

		/*!
			Ensures the file is at least @c length bytes long, appending 0s until it is
			if necessary.
		*/
		void ensure_file_is_at_least_length(long length);

		/*!
			@returns @c true if this file is read-only; @c false otherwise.
		*/
		bool get_is_read_only();

		class BitStream {
			public:
				BitStream(FILE *f, bool lsb_first) :
					file_(f),
					lsb_first_(lsb_first),
					next_value_(0),
					bits_remaining_(0) {}

				uint8_t get_bits(int q) {
					uint8_t result = 0;
					while(q--) {
						result = (uint8_t)((result << 1) | get_bit());
					}
					return result;
				}

			private:
				FILE *file_;
				bool lsb_first_;
				uint8_t next_value_;
				int bits_remaining_;

				uint8_t get_bit() {
					if(!bits_remaining_) {
						bits_remaining_ = 8;
						next_value_ = (uint8_t)fgetc(file_);
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

		FILE *file_;
		struct stat file_stats_;
		std::mutex file_access_mutex_;

		const std::string name_;
	private:
		bool is_read_only_;
};

}

#endif /* FileHolder_hpp */
