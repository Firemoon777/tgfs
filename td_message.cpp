#include "td_message.hpp"
#include <iostream>

Td_message::Td_message(td::td_api::object_ptr<td_api::message> && m) noexcept {
	change_time_ = m->edit_date_;
	message_time_ = m->date_;
	switch(m->content_->get_id()) {
		case td::td_api::messageAudio::ID: {
			type_ = AUDIO;
			auto audio = static_cast<td::td_api::messageAudio*>(m->content_.get());
			id_ = audio->audio_->audio_->id_;
			size_ = audio->audio_->audio_->size_;
			name_ = audio->audio_->file_name_;
			break;
		}
		case td::td_api::messageDocument::ID: {
			type_ = DOCUMENTS;
			auto doc = static_cast<td::td_api::messageDocument*>(m->content_.get());
			id_ = doc->document_->document_->id_;
			size_ = doc->document_->document_->size_;
			name_ = doc->document_->file_name_;
		}
		default:
			//std::cerr << "Unknown, id = " << m->content_->get_id() << std::endl;
			return;
	}
	//std::cerr << "Parsed: " << name_ << ", size = " << size_ << std::endl;
	if(name_.empty()) {
		name_ = "stub";
	}
	
}
