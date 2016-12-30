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

namespace Storage {

class FileHolder {
	public:
		enum {
			ErrorCantOpen = -1
		};

		virtual ~FileHolder();

	protected:
		FileHolder(const char *file_name);

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
			Ensures the file is at least @c length bytes long, appending 0s until it is
			if necessary.
		*/
		void ensure_file_is_at_least_length(long length);

		FILE *file_;
		struct stat file_stats_;
		bool is_read_only_;
};

}

#endif /* FileHolder_hpp */
