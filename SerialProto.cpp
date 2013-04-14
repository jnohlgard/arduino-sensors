#include "SerialProto.h"

SerialProto::SerialProto() :
    packet_type(0),
    packet_length(0),
    buffer_position(0),
    seen_preamble(false)
{
}

bool SerialProto::readOneByteAndVerify(Stream& input_stream)
{
    uint8_t data = input_stream.read();
    if (!seen_preamble)
    {
        // Still haven't seen the packet delimiter
        if (data == preamble_byte)
        {
            // Okay, now we can begin reading a packet, hopefully.
            seen_preamble = true;
        }
        return false;
    }
    buf[buffer_position] = data;
    ++buffer_position;
    if (buffer_position == header_length)
    {
        // Verify length field before reading any more.
        // Big endian (network byte order)
        uint16_t data_length = (buf[length_position] << 8) | buf[length_position + 1];
        uint16_t data_length_complement = (buf[length_complement_position] << 8) | buf[length_complement_position + 1];
        //~ uint16_t len_compl = *(static_cast<uint16_t*>(&buf[length_complement_position]));

        if (data_length != ~data_length_complement)
        {
            // Length parity check failed:
            // Corrupt packet, don't read any further.
            // This is a measure to minimize the possibility of getting a bit error
            // in the MSB of the length field causing the MCU to try to read a
            // packet that is many times larger than the actual packet, which
            // means no packets will be getting through until we reach that
            // number of bytes read and the checksum verification fails.
            seen_preamble = false;
            buffer_position = 0;
            return false;
        }
        packet_length = header_length + data_length + checksum_length;
        packet_type   = (buf[type_position] << 8) | buf[type_position + 1];
        if (packet_length > SERIAL_PROTO_BUFFER_SIZE)
        {
            // Packet too long!! PANIC!
            // this will cause trouble, so don't send too much data on the PC side!
            seen_preamble = false;
            buffer_position = 0;
        }
        return false;
    }
    else if (buffer_position == packet_length)
    {
        if (!verifyPacket())
        {
            // Checksum verification failed.
            seen_preamble = false;
            buffer_position = 0;
            return false;
        }
        return true;
    }
    return false;
}

bool SerialProto::readAllRemainingAndVerify(Stream& input_stream)
{
    while (input_stream.available() > 0)
    {
        if (readOneByteAndVerify(input_stream))
        {
            // Packet verified OK!
            return true;
        }
    }
    // We ran out of data
    return false;
}

bool SerialProto::writePacket(Stream& output_stream, uint16_t packet_type, uint8_t* data, size_t data_length)
{
    uint8_t checkA = 0;
    uint8_t checkB = 0;
    uint8_t header[header_length];
    // Network byte order, big endian.
    header[type_position] = (packet_type >> 8) & 0xFF;
    header[type_position + 1] = packet_type & 0xFF;
    header[length_position] = (data_length >> 8) & 0xFF;
    header[length_position + 1] = data_length & 0xFF;
    header[length_complement_position] = ~((data_length >> 8) & 0xFF);
    header[length_complement_position + 1] = ~(data_length & 0xFF);

    // Header checksum
    fletcher16(&checkA, &checkB, header, header_length, true);
    // Data checksum
    fletcher16(&checkA, &checkB, data, data_length, false);

    // write it
    output_stream.write(preamble_byte);
    output_stream.write(header, header_length);
    output_stream.write(data, data_length);
    output_stream.write(checkA);
    output_stream.write(checkB);

    return true;
}

bool SerialProto::verifyPacket()
{
    uint8_t checkA, checkB;
    fletcher16(&checkA, &checkB, buf, packet_length - checksum_length);
    if (checkA == buf[packet_length-2] && checkB == buf[packet_length-1])
    {
        // Checksum matches.
        return true;
    }
    else
    {
        return false;
    }
}

/// Fletcher's checksum
/**
 * Used in the serial protocol for content verification.
 * \see http://en.wikipedia.org/wiki/Fletcher%27s_checksum#Implementation
 */
void SerialProto::fletcher16(uint8_t* checkA, uint8_t* checkB, uint8_t* data, size_t len, bool reset)
{
    uint16_t sum1 = 0;
    uint16_t sum2 = 0;
    if (!reset)
    {
        sum1 = *checkA;
        sum2 = *checkB;
    }
    for(int index = 0; index < len; ++index)
    {
        sum1 = (sum1 + data[index]) % 255;
        sum2 = (sum2 + sum1) % 255;
    }

    // Change interval from 0..0xfe to 1..0xff to match the optimized version.
    if (sum1 == 0)
    {
        sum1 = 0xFF;
    }
    if (sum2 == 0)
    {
        sum2 = 0xFF;
    }
    *checkA = (uint8_t) sum1;
    *checkB = (uint8_t) sum2;
    return;
}
// Optimized version found on wikipedia, untested.
//~ void SerialProto::fletcher16(uint8_t* checkA, uint8_t* checkB, uint8_t* data, size_t len, bool reset)
//~ {
    //~ uint16_t sum1 = 0xff, sum2 = 0xff;
//~
    //~ if (!reset)
    //~ {
        //~ sum1 = *checkA;
        //~ sum2 = *checkB;
    //~ }
    //~ while (len) {
        //~ size_t tlen = len > 21 ? 21 : len;
        //~ len -= tlen;
        //~ do {
            //~ sum1 += *data;
            //~ ++data;
            //~ sum2 += sum1;
        //~ } while (--tlen);
        //~ sum1 = (sum1 & 0xff) + (sum1 >> 8);
        //~ sum2 = (sum2 & 0xff) + (sum2 >> 8);
    //~ }
    //~ /* Second reduction step to reduce sums to 8 bits */
    //~ sum1 = (sum1 & 0xff) + (sum1 >> 8);
    //~ sum2 = (sum2 & 0xff) + (sum2 >> 8);
    //~ *checkA = (uint8_t)sum1;
    //~ *checkB = (uint8_t)sum2;
    //~ return;
//~ }

