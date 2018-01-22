#include "td_chat.hpp"

void Td_chat::add_message(td_api::object_ptr<td_api::message> && m) {
	auto id = m->id_;
	messages[id] = new Td_message(std::move(m));
}
