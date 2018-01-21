#include "td_file_info.hpp"

Td_file_info::Td_file_info() noexcept {
	open_ = 0;
	fd_ = 0;
}

void Td_file_info::open_file() {
	mutex.lock();
	if(open_ == 0) {
		fd_ = open(filename_.c_str(), O_RDONLY);
	}
	open_++;
	mutex.unlock();
}

void Td_file_info::change_name_if_needed(std::string name) {
	mutex.lock();
	if(filename_.compare(name) != 0) {
		std::cerr << "name changed\n";
		close(fd_);
		filename_ = name;
		fd_ = open(filename_.c_str(), O_RDONLY);
	}
	mutex.unlock();	
}

void Td_file_info::close_file() {
	mutex.lock();
	open_--;
	if(open_ == 0) {
		close(fd_);
		fd_ = 0;
	}
	mutex.unlock();
}
