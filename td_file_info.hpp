#pragma once

#include <sys/types.h>
#include <sys/stat.h>
#include <fcntl.h>
#include <unistd.h>

#include <iostream>
#include <string>
#include <mutex>

class Td_file_info {
	public:
		Td_file_info() noexcept ;
		// File descriptor
		int fd_;
		// Real filename
		std::string filename_;
		// Open count
		int open_;
		// Mutex
		std::mutex mutex;

		void open_file();
		void change_name_if_needed(std::string name);
		void close_file(); 
};
