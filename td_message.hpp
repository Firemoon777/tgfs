#pragma once

#include <td/telegram/Client.h>
#include <td/telegram/Log.h>
#include <td/telegram/td_api.h>
#include <td/telegram/td_api.hpp>

namespace td_api = td::td_api;

enum media_type {
	DOCUMENTS,
	AUDIO
};

class Td_message {
	public:
		Td_message(td_api::object_ptr<td_api::message> && m) noexcept;
		std::string name_;
		std::int32_t id_;
		std::int32_t size_;
		std::int32_t change_time_;
		std::int32_t message_time_;
		media_type type_;
};
