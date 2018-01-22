#pragma once

#include <map>

#include "td_message.hpp"

class Td_chat {
	public:
		std::map<std::int64_t, Td_message*>messages;
		void add_message(td_api::object_ptr<td_api::message> && m);

};
