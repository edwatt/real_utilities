// Real_Utilities.cpp : This file contains the 'main' function. Program execution begins and ends there.
//

#include <stdio.h>
#include <stdlib.h>
#include <stdint.h>
#include <string.h>
#include "hidapi-win/include/hidapi.h"
#include <Windows.h>
#include <iostream>
#include <iomanip>
#include <mutex>
#include <chrono>
#include "protocol.h"
#include "protocol3.h"

//Air USB VID and PID
#define AIR_VID 0x3318
#define AIR_PID 0x0424

static hid_device*
open_device(int interface_num)
{
	struct hid_device_info* devs = hid_enumerate(AIR_VID, AIR_PID);
	struct hid_device_info* cur_dev = devs;
	hid_device* device = NULL;

	while (devs) {
		if (cur_dev->interface_number == interface_num) {
			device = hid_open_path(cur_dev->path);
			break;
		}

		cur_dev = cur_dev->next;
	}

	hid_free_enumeration(devs);
	return device;
}

static void
print_bytes(const uint8_t* buffer, int size)
{
	for (int i = 0; i < size; ++i)
		std::cout << std::setfill('0') << std::setw(2) << std::right << std::hex << (int)buffer[i] << " ";
	// 
	std::cout << std::endl;
}

static void
print_chars(const uint8_t* buffer, int size)
{
	for (int i = 0; i < size; ++i)
		std::cout << buffer[i];
	// 
	//std::cout << std::endl;
}

static int
write_control(hid_device* device_control, uint16_t msgId, const uint8_t* p_buf, int p_size)
{
	uint8_t cmd_buf[1024];
	std::fill(cmd_buf, cmd_buf + sizeof(cmd_buf), 0);

	int cmd_len = protocol::cmd_build(msgId, p_buf, p_size, &cmd_buf[1], sizeof(cmd_buf)-1); // leaves first byte=0x00, hid_write requirement

	int res_control = hid_write(device_control, cmd_buf, cmd_len + 1);
	if (res_control < 0) {
		printf("Unable to write to device\n");
		return 1;
	}

	protocol::parsed_rsp result;
	//std::cout << "Write: ";
	std::cout << "Write(" << res_control << " bytes): ";
	protocol::parse_rsp(&cmd_buf[1], cmd_len, &result);
	protocol::print_summary_rsp(&result);
	// print_bytes(cmd_buf, res_control);

	return 0;
}

static int
write_control(hid_device* device_control, std::string msg_id, const uint8_t* p_buf, int p_size)
{
	uint16_t hex_msg_id = protocol::hexForKey(msg_id);
	
	return write_control(device_control, hex_msg_id, p_buf, p_size);
}

static int
read_control(hid_device* device_control, int timeout_ms)
{
	uint8_t read_buf[1024];
	std::fill(read_buf, read_buf + sizeof(read_buf), 0);

	int res;

	try {
		// code that might throw an exception
		res = hid_read(device_control, read_buf, sizeof(read_buf));
		if (res < 0) {
			return res;
		}
	}
	catch (const std::exception& e) {
		// handle the exception
		std::cerr << e.what();
	}
	
	protocol::parsed_rsp result;
	std::cout << "Read(" << res << " bytes): ";
	protocol::parse_rsp(read_buf, res, &result);
	protocol::print_summary_rsp(&result);
	//print_bytes(read_buf, res);

	return res;
}

static int
write_imu(hid_device* device_imu, uint8_t msgId, const uint8_t* p_buf, int p_size)
{
	uint8_t cmd_buf[1024];
	std::fill(cmd_buf, cmd_buf + sizeof(cmd_buf), 0);

	int cmd_len = protocol3::cmd_build(msgId, p_buf, p_size, &cmd_buf[1], sizeof(cmd_buf) - 1); // leaves first byte=0x00, hid_write requirement

	int res_control = hid_write(device_imu, cmd_buf, cmd_len + 1);
	if (res_control < 0) {
		printf("Unable to write to device\n");
		return 1;
	}

	protocol3::parsed_rsp result;
	//std::cout << "Write: ";
	//std::cout << "Write(" << res_control << " bytes): ";
	protocol3::parse_rsp(&cmd_buf[1], cmd_len, &result);
	//protocol3::print_summary_rsp(&result);
	//print_bytes(cmd_buf, res_control);

	return 0;
}

static int
write_imu(hid_device* device_imu, std::string msg_id, const uint8_t* p_buf, int p_size)
{
	uint8_t hex_msg_id = protocol3::hexForKey(msg_id);

	return write_imu(device_imu, hex_msg_id, p_buf, p_size);
}

static int
read_imu(hid_device* device_imu, int timeout_ms)
{
	uint8_t read_buf[1024];
	std::fill(read_buf, read_buf + sizeof(read_buf), 0);

	int res;

	try {
		// code that might throw an exception
		res = hid_read(device_imu, read_buf, sizeof(read_buf));
		if (res < 0) {
			return res;
		}
	}
	catch (const std::exception& e) {
		// handle the exception
		std::cerr << e.what();
	}

	protocol3::parsed_rsp result;
	std::cout << "Read(" << res << " bytes): ";
	protocol3::parse_rsp(read_buf, res, &result);
	protocol3::print_summary_rsp(&result);
	//print_bytes(read_buf, res);

	return res;
}

static int
read_imu_get_rsp(hid_device* device_imu, int timeout_ms, protocol3::parsed_rsp* out)
{
	uint8_t read_buf[1024];
	std::fill(read_buf, read_buf + sizeof(read_buf), 0);

	int res;

	try {
		// code that might throw an exception
		res = hid_read(device_imu, read_buf, sizeof(read_buf));
		if (res < 0) {
			return res;
		}
	}
	catch (const std::exception& e) {
		// handle the exception
		std::cerr << e.what();
	}

	//std::cout << "Read(" << res << " bytes): ";
	protocol3::parse_rsp(read_buf, res, out);
	//protocol3::print_summary_rsp(out);
	//print_bytes(read_buf, res);

	return res;
}

int main()
{

	hid_device* device_imu;
	hid_device* device_control;

	uint8_t cmd_buf[64];


	printf("Opening Device\n");
	// open device
	device_imu = open_device(3);
	if (!device_imu) {
		printf("Unable to open device\n");
		return 1;
	}

	device_control = open_device(4);
	if (!device_control) {
		printf("Unable to open device\n");
		return 1;
	}

	int res_control, res_read;
	std::string msg_str;


	msg_str = "GET_STATIC_ID";
	res_control = write_imu(device_imu, msg_str, nullptr, 0);
	res_read = read_imu(device_imu, -1); 

	protocol3::parsed_rsp result;
	
	msg_str = "GET_CAL_DATA_LENGTH";
	res_control = write_imu(device_imu, msg_str, nullptr, 0);
	res_read = read_imu_get_rsp(device_imu, -1, &result);

	if (result.msgId == protocol3::hexForKey(msg_str) && result.payload_size == 4) {
		uint32_t cal_data_len = (result.payload[0] | result.payload[1] << 8 | result.payload[2] << 16 | result.payload[3] << 24);
		uint32_t remaining_bytes = cal_data_len;
		std::cout << "Calibration data bytes: " << std::dec << cal_data_len << std::endl;



		while (remaining_bytes > 0) {
			msg_str = "CAL_DATA_GET_NEXT_SEGMENT";
			res_control = write_imu(device_imu, msg_str, nullptr, 0);
			res_read = read_imu_get_rsp(device_imu, -1, &result);

			uint16_t last_bytes = result.payload_size;

			remaining_bytes -= last_bytes;
			print_chars(result.payload, result.payload_size);
			//std::cout << "Packet bytes: " << std::dec << last_bytes <<  " , Bytes remaining: " << remaining_bytes << std::endl;

		}

	}


	


	//msg_str = "R_GLASSID";
	//res_control = write_control(device_control, msg_str, nullptr, 0);
	//res_read = read_control(device_control, -1);

	//msg_str = "R_DP7911_FW_VERSION";
	//res_control = write_control(device_control, msg_str, nullptr, 0);
	//res_read = read_control(device_control, -1);

	//msg_str = "R_DSP_VERSION";
	//res_control = write_control(device_control, msg_str, nullptr, 0);
	//res_read = read_control(device_control, -1); 
	//
	//msg_str = "R_DSP_APP_FW_VERSION";
	//res_control = write_control(device_control, msg_str, nullptr, 0);
	//res_read = read_control(device_control, -1); 
	//
	//msg_str = "R_MCU_APP_FW_VERSION";
	//res_control = write_control(device_control, msg_str, nullptr, 0);
	//res_read = read_control(device_control, -1);
	//
	//// res_control = write_control(device_control, 0x0003, nullptr, 0);
	//// res_read = read_control(device_control, -1);

	//msg_str = "HEARTBEAT";
	//res_control = write_control(device_control, msg_str, nullptr, 0);
	//res_read = read_control(device_control, -1);

	//msg_str = "HEARTBEAT";
	//res_control = write_control(device_control, msg_str, nullptr, 0);
	//res_read = read_control(device_control, -1);



	/* DISP MODE and HEARTBEAT
	msg_str = "W_DISP_MODE";
	const uint8_t mode_change_p_buf[] = { 0x03, 0x00, 0x00, 0x00 };
	res_control = write_control(device_control, msg_str, mode_change_p_buf, sizeof(mode_change_p_buf));
	res_read = read_control(device_control, -1);

	std::chrono::steady_clock::time_point previous = std::chrono::steady_clock::now();
	std::chrono::steady_clock::time_point now = previous;

	int read_count = 0;

	previous = std::chrono::steady_clock::now();
	while (true) {
		std::this_thread::sleep_for(std::chrono::milliseconds(1));
		res_read = read_control(device_control, -1);
		read_count++;

		now = std::chrono::steady_clock::now();

		if (std::chrono::duration_cast<std::chrono::milliseconds>(now - previous).count() >= 300)
		{
			msg_str = "W_DISP_MODE";
			const uint8_t mode_change_p_buf[] = { 0x03, 0x00, 0x00, 0x00 };
			res_control = write_control(device_control, msg_str, mode_change_p_buf, sizeof(mode_change_p_buf));
			res_read = read_control(device_control, -1);
			break;
		}
	}

	
	
	previous = std::chrono::steady_clock::now();
	now = previous;
	
	read_count = 0;
	int hb_count = 0;

	while (true) {
		std::this_thread::sleep_for(std::chrono::milliseconds(1));
		res_read = read_control(device_control, -1);
		read_count++;

		now = std::chrono::steady_clock::now();

		if (std::chrono::duration_cast<std::chrono::milliseconds>(now - previous).count() >= 100)
		{
			msg_str = "HEARTBEAT";
			res_control = write_control(device_control, 0x001A, nullptr, 0);
			res_read = read_control(device_control, -1);
			previous = now;

			hb_count++;

			if (hb_count == 15) {
				msg_str = "W_DISP_MODE";
				const uint8_t mode_change_p_buf[] = { 0x03, 0x00, 0x00, 0x00 };
				res_control = write_control(device_control, msg_str, mode_change_p_buf, sizeof(mode_change_p_buf));
				res_read = read_control(device_control, -1);
			}
		}
	}*/


	//uint8_t sbs_set[] = { 0x00, 0xfd, 0xc2, 0x60, 0xdb, 0x9d, 0x15, 0x00, 0x19, 0x00, 0x00, 0x00, 0xfa, 0x73, 0x85, 0xa4, 0x08, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x03, 0x00, 0x00, 0x00 };

	//res_control = hid_write(device_control, sbs_set, sizeof(sbs_set));
	//if (res_control < 0) {
	//	printf("Unable to write to device\n");
	//	return 1;
	//}

	//std::cout << "Write: ";
	//protocol::parse_rsp(&sbs_set[1], sizeof(sbs_set) - 1, &result);
	//protocol::print_summary_rsp(&result);

	//while (true) {

	//	std::fill(read_buf, read_buf + sizeof(read_buf), 0);

	//	try {
	//		// code that might throw an exception
	//		int res = hid_read(device_control, read_buf, sizeof(read_buf));
	//		if (res < 0) {
	//			break;
	//		}
	//	}
	//	catch (const std::exception& e) {
	//		// handle the exception
	//		std::cerr << e.what();
	//	}

	//	std::cout << "Read: ";
	//	protocol::parse_rsp(read_buf, sizeof(read_buf), &result);
	//	protocol::print_summary_rsp(&result);
	//	read_count++;
	//}
}
