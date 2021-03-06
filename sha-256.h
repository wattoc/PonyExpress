#ifndef SHA_256_H
#define SHA_256_H

#include <stdint.h>
#include <cstring>

class SHA
{
public:
	static void calc_sha_256(uint8_t hash[32], const void *input, size_t len);
	static void hash_to_string(char string[65], const uint8_t hash[32]);
};

#endif
