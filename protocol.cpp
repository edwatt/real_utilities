#include "protocol.h"

#include <map>
#include <iostream>
#include <iomanip>
#include <stdio.h>
#include <stdlib.h>
#include <stdint.h>
#include <string>
#include <zlib.h>
#include <intrin.h>

const uint8_t HEAD = 0xfd;
const int MSG_ID_OFS = 15;
const int PAYLOAD_OFS = 22;
const int STATUS_OFS = 22;
const int LEN_OFS = 5;
const int CRC_OFS = 1;
const int TS_OFS = 7;
const int RESERVED_OFS = 17;

static const std::map<std::string, uint16_t> MESSAGES = {
    {"W_CANCEL_ACTIVATION", 0x19},
    {"R_MCU_APP_FW_VERSION", 0x26},//MCU APP FW version.
    {"R_GLASSID" , 0x15},//GLASS HW ID.
    {"R_DSP_APP_FW_VERSION", 0x21},//DSP APP FW version.
    {"R_DP7911_FW_VERSION" , 0x16},//DP APP FW version.
    {"R_ACTIVATION_TIME" , 0x29},//Read activation time
    {"W_ACTIVATION_TIME" , 0x2A},//Write activation time
    {"W_SLEEP_TIME" , 0x1E},//Write unsleep time
    {"R_IS_NEED_UPGRADE_DSP_FW", 0x49},//Check whether the DSP needs to be upgraded.
    {"W_FORCE_UPGRADE_DSP_FW", 0x69},//Force upgrade DSP.
    {"R_DSP_VERSION", 0x18}, //DSP APP FW version.
    {"W_UPDATE_DSP_APP_FW_PREPARE" , 0x45},	//(Implemented in APP)
    {"W_UPDATE_DSP_APP_FW_START" , 0x46},	//(Implemented in APP)
    {"W_UPDATE_DSP_APP_FW_TRANSMIT" , 0x47},	//(Implemented in APP)
    {"E_DSP_ONE_PACKGE_WRITE_FINISH" , 0x6C0E},	//(check 4K as one package send)
    {"W_UPDATE_DSP_APP_FW_FINISH" , 0x48},	//(Implemented in APP)
    {"E_DSP_UPDATE_ENDING" , 0x6C11}, //whether the upgrade is complete.
    {"E_DSP_UPDATE_PROGRES" , 0x6C10}, //before upgrade dsp, air for update dsp boot

    {"W_UPDATE_MCU_APP_FW_PREPARE" , 0x3E},//Preparations for mcu app fw upgrade
    {"W_UPDATE_MCU_APP_FW_START" , 0x3F},	//(Implemented in Boot)
    {"W_UPDATE_MCU_APP_FW_TRANSMIT" , 0x40},	//(Implemented in Boot)
    {"W_UPDATE_MCU_APP_FW_FINISH" , 0x41},	//(Implemented in Boot)
    {"W_BOOT_JUMP_TO_APP" , 0x42},	//(Implemented in Boot)
    {"W_MCU_APP_JUMP_TO_BOOT" , 0x44},
    {"R_DP7911_FW_IS_UPDATE" , 0x3C},
    {"W_UPDATE_DP" , 0x3D},


    {"W_BOOT_UPDATE_MODE" , 0x1100},
    {"W_BOOT_UPDATE_CONFIRM" , 0x1101},
    {"W_BOOT_UPDATE_PREPARE" , 0x1102},

    {"W_BOOT_UPDATE_START" , 0x1103},
    {"W_BOOT_UPDATE_TRANSMIT" , 0x1104},
    {"W_BOOT_UPDATE_FINISH" , 0x1105},

    // P_ = pushed from device

    // 11-bit payload
    {"P_BUTTON_PRESSED", 0x6C05},

    // appear to fire every 5 seconds with payload = 0
    {"P_UKNOWN_HEARTBEAT" , 0x6c02},
    {"P_UKNOWN_HEARTBEAT_2" , 0x6c12},

    {"W_DISP_MODE", 0x08},
    {"ASYNC_TEXT_LOG", 0x6c09},
    {"HEARTBEAT", 0x1A}
};

std::string
protocol::keyForHex(uint16_t hex) {
    std::string value;
        
    std::map<std::string, uint16_t>::const_iterator it;

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

uint16_t
protocol::hexForKey(std::string key) {
    uint16_t hex_id;

    std::map<std::string, uint16_t>::const_iterator it;

    for (it = MESSAGES.begin(); it != MESSAGES.end(); it++)
    {
        if (key.compare(it->first)==0) {
            hex_id = it->second;
            return hex_id;
        }
    }
    hex_id = 0x0000;
    return hex_id;
}

void
protocol::listKnownCommands() {
    
    std::cout << "air commands : " << std::endl;
    
    std::map<std::string, uint16_t>::const_iterator it;

    for (it = MESSAGES.begin(); it != MESSAGES.end(); it++)
    {
        std::cout << it->first    // string (key)
            << ':'
            << std::hex << it->second   // string's value 
            << std::endl;
    }
}

int 
protocol::cmd_build(std::string msg_id, const uint8_t* p_buf, int p_size, uint8_t* cmd_buf, int cb_size) {
    uint16_t hex_msg_id = hexForKey(msg_id);

    return cmd_build(hex_msg_id, p_buf, p_size, cmd_buf, cb_size);
}

int
protocol::cmd_build(uint16_t msgId, const uint8_t* p_buf, int p_size, uint8_t* cmd_buf, int cb_size) {

    uint16_t len = /*HEAD*/1+ /*CRC*/4 + /*LEN*/2 + /*TS*/8 + /*MSG_ID*/2 + /*RESERVED*/5;

    if (p_buf != nullptr && p_size > 0) {
        len += p_size;

        if (cb_size < len) return 0; // check if cmd will fit in buffer

        std::copy(p_buf, p_buf + p_size, &cmd_buf[PAYLOAD_OFS]);
    }
    
    cmd_buf[0] = HEAD;
    cmd_buf[MSG_ID_OFS] = (msgId >> 0) & 0xff;
    cmd_buf[MSG_ID_OFS + 1] = (msgId >> 8) & 0xff;   


    uint16_t packet_len = len - 5;
    cmd_buf[LEN_OFS] = packet_len & 0xff;
    cmd_buf[LEN_OFS + 1] = (packet_len >> 8) & 0xff;

    uint8_t ts_buf[] = { 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00 };  // zeros seem to work fine for now

    std::copy(ts_buf, ts_buf + sizeof(ts_buf), &cmd_buf[TS_OFS]);

    uint32_t crc = crc32(0L, Z_NULL, 0);
    crc = crc32(crc, (const unsigned char*)(&cmd_buf[LEN_OFS]), packet_len);

    //crc = _byteswap_ulong(crc);

    uint8_t* crc_ptr = static_cast<uint8_t*>(static_cast<void*>(&crc));

    std::copy(crc_ptr, crc_ptr + sizeof(crc), &cmd_buf[CRC_OFS]);

    return len;
};


static uint16_t 
get_msgId(const uint8_t* buffer_in, int size) {
    if (size > MSG_ID_OFS+1) {
        return (buffer_in[MSG_ID_OFS] | (buffer_in[MSG_ID_OFS + 1] << 8));
    }
}


static uint8_t
get_status_byte(const uint8_t* buffer_in, int size) {
    if (size > STATUS_OFS) {
        return buffer_in[STATUS_OFS];
    }
}

static uint16_t
get_length(const uint8_t* buffer_in, int size) {
    if (size > LEN_OFS +1) {
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
protocol::print_summary_rsp(parsed_rsp* result)
{
    std::cout << "msgId: 0x" << std::setfill('0') << std::setw(4) << std::right << std::hex << (int)result->msgId << ", ";
    if (result->msgId != -1)
    {
        std::cout << "msgId decode: " << keyForHex(result->msgId) << ", ";
    }
    std::cout << "status: 0x" << std::setfill('0') << std::setw(2) << std::right << std::hex << (int)result->status << ", ";
    std::cout << "payload_size: 0x" << result->payload_size << ", ";

    
    std::cout << "payload: ";

    switch (result->msgId) {
    case 0x6c09: // ASYNC TEXT LOG
    case 0x0015: // GLASS HW ID.
    case 0x0026: // MCU APP FW version.
    case 0x0021: // DSP APP FW version.
    case 0x0016: // DP APP FW version.
    case 0x0018: // DSP version.
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
protocol::parse_rsp(const uint8_t* buffer_in, int size, parsed_rsp* result) {
    //initialize result struct
    result->msgId = -1;
    result->status = 0;
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
    result->status = get_status_byte(buffer_in, size);

    int packet_len = get_length(buffer_in, size);
    
    if (packet_len < 18) {
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
//
//// todo: learn bitshifts/bitmasks and stuff instead of writing it out
//// export const brightness_map = [
////     //     3: 6 = increase (3= button id)
////     //        7 = decrease
////     //     7: intensity 0-7
////     // 0 0 0 4 - 0 0 0 8 - 0 0 0
////     "0 0 0 6 0 0 0 0 0 0 0", // dimmest
////     "0 0 0 6 0 0 0 1 0 0 0",
////     "0 0 0 6 0 0 0 2 0 0 0",
////     "0 0 0 6 0 0 0 3 0 0 0",
////     "0 0 0 6 0 0 0 4 0 0 0",
////     "0 0 0 6 0 0 0 5 0 0 0",
////     "0 0 0 6 0 0 0 6 0 0 0",
////     "0 0 0 6 0 0 0 7 0 0 0", // brightest
//// ]
//
//// arg brightness_int 1-8, 8 being the brightest
//export function brightInt2Bytes(brightness_int) {
//    // const req_bright = brightness_map[parseInt(brightness_int)-1];
//    // const byte_arr = new Uint8Array(req_bright.split(' ').map(x => parseInt(x)));
//    // return byte_arr
//    return new Uint8Array(['0' + brightness_int]);
//}
//
//// arg Uint8Array, returns string
//export function brightBytes2Int(bright_byte_arr) {
//    return bright_byte_arr[7] + 1;
//    //return brightness_map.indexOf(bright_byte_arr.join(' ')) + 1;
//}
//
//
//export function bytes2Time(bytes) {
//    let time = '';
//    for (let i = bytes.byteLength - 1; i >= 0; i--) {
//        if (i > 3) {
//            time += bytes[i].toString(2)
//        }
//        else {
//            time += bytes[i].toString(2)
//                // time += bytes[i] << (i * 8);
//        }
//    }
//    return time;
//};
//
//export function hex2Decimal(byte) {
//    return parseInt(byte, 16);
//}
//
//export function time2Bytes(timeStamp) {
//    let arr = new Uint8Array(8)
//        let len = Math.floor((Number(timeStamp).toString(2).length) / 8)
//        let longN = parseInt(Number(timeStamp).toString(2).substring(0, Number(timeStamp).toString(2).length - 32), 2)
//        for (let i = len; i >= 0; i--) {
//            if (i > 3) {
//                arr[i] = ((longN >> > ((i - 4) * 8)) & 0xFF);
//            }
//            else {
//                arr[i] = ((timeStamp >> > (i * 8)) & 0xFF);
//            }
//        }
//    return arr;
//}
