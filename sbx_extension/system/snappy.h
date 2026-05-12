#pragma once

#include <cstdint>
#include <cstring>

extern "C" {
extern int LZ4_compressBound(int inputSize);
extern int LZ4_compress_default(const char *src, char *dst, int srcSize, int dstCapacity);
extern int LZ4_decompress_safe(const char *src, char *dst, int compressedSize, int dstCapacity);
}

namespace snappy {

inline size_t MaxCompressedLength(size_t source_length) {
	return sizeof(uint32_t) + LZ4_compressBound(source_length);
}

inline bool RawCompress(const char *input, size_t input_length, char *compressed,
		size_t *compressed_length) {
	char *p = compressed + sizeof(uint32_t);
	int dst_size = LZ4_compress_default(input, p, input_length, LZ4_compressBound(input_length));
	if (dst_size <= 0) {
		return false;
	}
	*compressed_length = sizeof(uint32_t) + dst_size;
	uint32_t len32 = static_cast<uint32_t>(input_length);
	std::memcpy(compressed, &len32, sizeof(uint32_t));
	return true;
}

inline bool GetUncompressedLength(const char *start, size_t n, size_t *result) {
	if (n < sizeof(uint32_t)) {
		return false;
	}
	uint32_t tmp;
	std::memcpy(&tmp, start, sizeof(uint32_t));
	*result = tmp;
	if (*result >= INT32_MAX) {
		return false;
	}
	return true;
}

inline bool RawUncompress(const char *compressed, size_t compressed_length,
		char *uncompressed) {
	if (compressed_length < sizeof(uint32_t)) {
		return false;
	}
	const char *p = compressed + sizeof(uint32_t);
	uint32_t raw_len;
	std::memcpy(&raw_len, compressed, sizeof(uint32_t));
	size_t uncompressed_length = raw_len;
	if (uncompressed_length >= INT32_MAX) {
		return false;
	}
	int dst_size = LZ4_decompress_safe(p, uncompressed, compressed_length - sizeof(uint32_t), uncompressed_length);
	if (dst_size < 0 || static_cast<size_t>(dst_size) != uncompressed_length) {
		return false;
	}
	return true;
}

} // namespace snappy