#ifndef AUDIOSOURCE_H
#define AUDIOSOURCE_H

#include <vector>
#include <cstdint>

#include <QIODevice>

/*!
 * \brief Provides an intermediate receipticle for audio data.
 *
 * Provides a QIODevice that will attempt to buffer the minimum amount
 * of data before handing it off to a polling QAudioOutput.
 *
 * Adding an extra buffer increases worst-case latency but resolves a
 * startup race condition in which it is difficult to tell how much data a
 * QAudioOutput that is populated by pushing data currently has buffered;
 * it also works around what empirically seemed to be a minimum 16384-byte
 * latency on push audio generation.
 */
struct AudioBuffer: public QIODevice {
	AudioBuffer() {
		open(QIODevice::ReadOnly | QIODevice::Unbuffered);
	}

	void setDepth(size_t depth) {
		buffer.resize(depth);
	}

	// AudioBuffer-specific behaviour: always provide the latest data,
	// even if that means skipping some.
	qint64 readData(char *data, const qint64 maxlen) override {
		if(!maxlen) {
			return 0;
		}

		std::lock_guard lock(mutex);
		if(readPointer == writePointer) return 0;

		const size_t dataAvailable = std::min(writePointer - readPointer, size_t(maxlen));
		size_t dataToCopy = dataAvailable;

		// Push the read pointer such that only the most recent chunk is returned;
		// nevertheless don't allow it to be pushed to a point where less than half
		// a buffer is left, if avoidable. QAudioOutput doesn't make any guarantees
		// about how much data it will read at a time so there's some second guessing here.
		//
		// TODO: can I be smarter than this?
//		const size_t newReadPointer = std::min(writePointer - dataToCopy, writePointer - (buffer.size() >> 1));
//		readPointer = std::max(readPointer, newReadPointer);

		while(dataToCopy) {
			const size_t nextLength = std::min(buffer.size() - (readPointer % buffer.size()), dataToCopy);
			memcpy(data, &buffer[readPointer % buffer.size()], nextLength);

			dataToCopy -= nextLength;
			data += nextLength;
			readPointer += nextLength;
		}

		return dataAvailable;
	}

	qint64 bytesAvailable() const override {
		std::lock_guard lock(mutex);
		return writePointer - readPointer;
	}

	// Required to make QIODevice concrete; not used.
	qint64 writeData(const char *, qint64) override {
		return 0;
	}

	// Posts a new set of source data. This buffer permits only the amount of data
	// specified by @c setDepth to be enqueued into the future. Additional writes
	// after the buffer is full will overwrite the newest data.
	void write(const std::vector<int16_t> &source) {
		std::lock_guard lock(mutex);
		const size_t sourceSize = source.size() * sizeof(int16_t);
		size_t endPoint = std::min(writePointer + sourceSize, readPointer + buffer.size());

		writePointer = endPoint - sourceSize;
		size_t bytesToCopy = sourceSize;
		auto data = reinterpret_cast<const uint8_t *>(source.data());

		while(bytesToCopy) {
			size_t nextLength = std::min(buffer.size() - (writePointer % buffer.size()), bytesToCopy);
			memcpy(&buffer[writePointer % buffer.size()], data, nextLength);

			bytesToCopy -= nextLength;
			data += nextLength;
			writePointer += nextLength;
		}
	}

	private:
		mutable std::mutex mutex;
		std::vector<uint8_t> buffer;
		mutable size_t readPointer = 0;
		size_t writePointer = 0;
};

#endif // AUDIOSOURCE_H
