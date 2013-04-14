#ifndef _SERIALPROTO_H_
#define _SERIALPROTO_H_

#include <stdlib.h>
#include <stdint.h>

#if defined(ARDUINO) && ARDUINO >= 100
    #include "Arduino.h"
#else
    #include "WProgram.h"
#endif

#define SERIAL_PROTO_BUFFER_SIZE 20

/*
struct SerialProtoHeader
{
    uint16_t type; ///< packet type
    uint16_t length; ///< Length of data, excluding header and checksum.
    uint16_t length_compl; ///< 1's complement of length
};
*/

/**
 * \class SerialProto
 * \brief Send packet data over serial port, with checksum verification.
 */
class SerialProto
{
    public:
        SerialProto();

        /**
         * \brief Read one byte from stream.
         * \return true if packet was completed and checksum was correct.
         */
        bool readOneByteAndVerify(Stream&);

        /**
         * \brief Read until there are no more bytes currently on the stream or
         *        the packet is done, whatever happens first.
         * \return true if packet was completed and checksum was correct.
         */
        bool readAllRemainingAndVerify(Stream&);

        static bool writePacket(Stream&, uint16_t packet_type, uint8_t* data, size_t data_length);

    protected:
        bool verifyPacket();

    private:
        static void fletcher16(uint8_t* checkA, uint8_t* checkB, uint8_t* data, size_t len, bool = true);

        uint16_t packet_type;
        uint16_t packet_length;
        unsigned int buffer_position;
        bool seen_preamble;
        uint8_t buf[SERIAL_PROTO_BUFFER_SIZE];

        /**
         * \brief Preamble byte is used for synchronization between packets.
         */
        static const uint8_t preamble_byte = 0xAA;

        /**
         * \brief Length of header in packet.
         *
         * For reliability we do some extra verification before reading any more
         * after reading the full header.
         */
        static const uint8_t header_length = 0x06;
        static const uint8_t checksum_length = 0x02;
        static const uint8_t type_position = 0x00;
        static const uint8_t length_position = 0x02;
        static const uint8_t length_complement_position = 0x04;
};
#endif
