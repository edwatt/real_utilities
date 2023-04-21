#include "protocol3.h"

#include <map>
#include <iostream>
#include <iomanip>
#include <stdio.h>
#include <stdlib.h>
#include <stdint.h>
#include <string>
#include <zlib.h>
#include <intrin.h>

const uint8_t HEAD = 0xaa;
const int MSG_ID_OFS = 7;
const int PAYLOAD_OFS = 8;
const int LEN_OFS = 5;
const int CRC_OFS = 1;
const int NO_PAYLOAD_PACKET_LEN = 3;


static const std::map<std::string, uint8_t> MESSAGES = {
    {"GET_CAL_DATA_LENGTH", 0x14},
    {"CAL_DATA_GET_NEXT_SEGMENT", 0x15},
    {"ALLOCATE_CAL_DATA_BUFFER" , 0x16},
    {"WRITE_CAL_DATA_SEGMENT", 0x17},
    {"FREE_CAL_BUFFER" , 0x18},
    {"START_IMU_DATA" , 0x19}, // start glasses if data is 0x01 ? ? ?
    {"GET_STATIC_ID" , 0x1a}, // return static data 0x01012220
    {"UNKNOWN_1D" , 0x1d}
};


std::string
protocol3::keyForHex(uint8_t hex) {
    std::string value;

    std::map<std::string, uint8_t>::const_iterator it;

    for (it = MESSAGES.begin(); it != MESSAGES.end(); it++)
    {
        if (it->second == hex) {
            value = it->first;
            return value;
        }
    }
    value = "UNKNOWN_COMMAND";
    return value;
}

uint8_t
protocol3::hexForKey(std::string key) {
    uint8_t hex_id;

    std::map<std::string, uint8_t>::const_iterator it;

    for (it = MESSAGES.begin(); it != MESSAGES.end(); it++)
    {
        if (key.compare(it->first) == 0) {
            hex_id = it->second;
            return hex_id;
        }
    }
    hex_id = 0x00;
    return hex_id;
}

void
protocol3::listKnownCommands() {

    std::cout << "air commands : " << std::endl;

    std::map<std::string, uint8_t>::const_iterator it;

    for (it = MESSAGES.begin(); it != MESSAGES.end(); it++)
    {
        std::cout << it->first    // string (key)
            << ':'
            << std::hex << it->second   // string's value 
            << std::endl;
    }
}


int
protocol3::cmd_build(std::string msg_id, const uint8_t* p_buf, int p_size, uint8_t* cmd_buf, int cb_size) {
    uint8_t hex_msg_id = hexForKey(msg_id);

    return cmd_build(hex_msg_id, p_buf, p_size, cmd_buf, cb_size);
}

int
protocol3::cmd_build(uint8_t msgId, const uint8_t* p_buf, int p_size, uint8_t* cmd_buf, int cb_size) {

    uint16_t len = /*HEAD*/1 + /*CRC*/4 + /*LEN*/2 + /*MSG_ID*/1; // 0x00 0xAA 4bytes_crc 2bytes_len 1byte_msgid 56bytes_data

    if (p_buf != nullptr && p_size > 0) {
        len += p_size;

        if (cb_size < len) return 0; // check if cmd will fit in buffer

        std::copy(p_buf, p_buf + p_size, &cmd_buf[PAYLOAD_OFS]);
    }

    cmd_buf[0] = HEAD;
    cmd_buf[MSG_ID_OFS] = msgId;


    uint16_t packet_len = len - 5;
    cmd_buf[LEN_OFS] = packet_len & 0xff;
    cmd_buf[LEN_OFS + 1] = (packet_len >> 8) & 0xff;

    uint32_t crc = crc32(0L, Z_NULL, 0);
    crc = crc32(crc, (const unsigned char*)(&cmd_buf[LEN_OFS]), packet_len);

    //crc = _byteswap_ulong(crc);

    uint8_t* crc_ptr = static_cast<uint8_t*>(static_cast<void*>(&crc));

    std::copy(crc_ptr, crc_ptr + sizeof(crc), &cmd_buf[CRC_OFS]);

    return len;
};


static uint8_t
get_msgId(const uint8_t* buffer_in, int size) {
    if (size >= MSG_ID_OFS) {
        return buffer_in[MSG_ID_OFS];
    }
}

static uint16_t
get_length(const uint8_t* buffer_in, int size) {
    if (size > LEN_OFS + 1) {
        return (buffer_in[LEN_OFS] | (buffer_in[LEN_OFS + 1] << 8));
    }
}

static void
print_bytes(const uint8_t* buffer, int size)
{
    for (int i = 0; i < size; i++)
        std::cout << std::setfill('0') << std::setw(2) << std::right << std::hex << (int)buffer[i] << " ";
}

static void
print_chars(const uint8_t* buffer, int size)
{
    for (int i = 0; i < size; i++)
        std::cout << buffer[i];
}

void
protocol3::print_summary_rsp(parsed_rsp* result)
{
    std::cout << "msgId: 0x" << std::setfill('0') << std::setw(2) << std::right << std::hex << (int)result->msgId << ", ";
    if (result->msgId != -1)
    {
        std::cout << "msgId decode: " << keyForHex(result->msgId) << ", ";
    }
   std::cout << "payload_size: 0x" << result->payload_size << ", ";


    std::cout << "payload: ";

    switch (result->msgId) {
    case 0x15: // CAL_DATA
        print_chars(result->payload, result->payload_size);
        //std::cout << std::endl;
        break;
    default:
        print_bytes(result->payload, result->payload_size);
        break;
    }

    std::cout << " " << std::endl;
}

void
protocol3::parse_rsp(const uint8_t* buffer_in, int size, parsed_rsp* result) {
    //initialize result struct
    result->msgId = -1;
    result->payload_size = 0;
    std::fill(result->payload, result->payload + sizeof(result->payload), 0);

    if (buffer_in == NULL || size < 1) {
        return;
    }

    if (buffer_in[0] != HEAD) {
        std::cout << "HEAD mismatch " << std::hex << buffer_in[0] << std::endl;
        return;
    }

    result->msgId = get_msgId(buffer_in, size);

    int packet_len = get_length(buffer_in, size);

    if (packet_len < NO_PAYLOAD_PACKET_LEN) {
        return;
    }
    result->payload_size = packet_len - (PAYLOAD_OFS - LEN_OFS); // ;/* len, ts, msgid, reserve, status*/

    if (result->payload_size <= sizeof(result->payload))
    {
        std::copy(&buffer_in[PAYLOAD_OFS], &buffer_in[PAYLOAD_OFS] + result->payload_size, result->payload);

        /*for (int i = 0; i < result->payload_size; i++)
        {
            result->payload[i] = buffer_in[PAYLOAD_OFS + i];
        }*/
    }

    // CRC
    uint32_t crc = crc32(0L, Z_NULL, 0);
    crc = crc32(crc, (const unsigned char*)(&buffer_in[LEN_OFS]), packet_len);

    /*crc = _byteswap_ulong(crc);

    std::cout << "CRC: " << std::hex << crc << std::endl;*/

    return;
};