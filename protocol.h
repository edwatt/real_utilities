#pragma once
#include <string>

class protocol
{
    public:
        typedef struct {
            uint16_t msgId;
            uint8_t status;
            uint8_t payload[200];
            uint16_t payload_size;
        } parsed_rsp;

        static void listKnownCommands();
        static std::string keyForHex(uint16_t hex);
        static uint16_t hexForKey(std::string key);
        static void parse_rsp(const uint8_t* buffer_in, int size, parsed_rsp* result);
        static int cmd_build(uint16_t msgId, const uint8_t* p_buf, int p_size, uint8_t* cmd_buf, int cb_size);
        static int cmd_build(std::string msg_id, const uint8_t* p_buf, int p_size, uint8_t* cmd_buf, int cb_size);
        static void print_summary_rsp(parsed_rsp* result);
};

