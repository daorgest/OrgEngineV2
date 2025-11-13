//
// Created by Orgest on 10/22/2025.
//

#pragma once
#include <chrono>
#include <random>

#include "Array.h"

class OrgUUID
{
public:
	OrgUUID()
	{
		// based off of the v7 uuid

		const auto now = std::chrono::system_clock::now();
		const auto duration = now.time_since_epoch();
		const auto timestamp_ms = std::chrono::duration_cast<std::chrono::milliseconds>(duration).count();

		thread_local std::random_device rd;
		thread_local std::mt19937_64 gen(rd());

		const uint64_t rand_bits_1 = gen();
		const uint64_t rand_bits_2 = gen();

		// unix_ts_ms
		// Big-endian timestamp (most significant bits first)
		data[0] = (timestamp_ms >> 40);
		data[1] = (timestamp_ms >> 32);
		data[2] = (timestamp_ms >> 24);
		data[3] = (timestamp_ms >> 16);
		data[4] = (timestamp_ms >> 8);
		data[5] = timestamp_ms;

		// --- Bytes 6-15 (80 bits): Random data ---
		// Fill this with our random bits first, then overwrite
		// the version and variant bits.
		data[6] = (rand_bits_1 >> 56);
		data[7] = (rand_bits_1 >> 48);
		data[8] = (rand_bits_1 >> 40);
		data[9] = (rand_bits_1 >> 32);
		data[10] = (rand_bits_1 >> 24);
		data[11] = (rand_bits_1 >> 16);
		data[12] = (rand_bits_1 >> 8);
		data[13] = rand_bits_1;
		data[14] = (rand_bits_2 >> 56); // Using top bits of 2nd rand
		data[15] = (rand_bits_2 >> 48);

		// 4. Set the standard UUIDv7 bits

		// --- Byte 6: Set Version (0111) ---
		// Clear top 4 bits (AND 0x0F), then set to 0b0111 (OR 0x70)
		data[6] = (data[6] & 0x0F) | (0x7 << 4);

		// --- Byte 8: Set Variant (10) ---
		// Clear top 2 bits (AND 0x3F), then set to 0b10 (OR 0x80)
		data[8] = (data[8] & 0x3F) | (0x2 << 6);
	}

	std::string toString()
	{
		std::stringstream ss;
		ss << std::hex << std::setfill('0');

		for (int i = 0; i < 16; i++)
		{
			ss << std::setw(2) << static_cast<int>(data[i]);
			if (i == 3 || i == 5 || i == 7 || i == 9)
			{
				ss << "-";
			}
		}
		return ss.str();
	}

private:
	Array<u8, 16> data;
};
