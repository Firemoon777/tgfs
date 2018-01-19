#define FUSE_USE_VERSION 26

#include <string.h>
#include <errno.h>
#include <fuse.h>

#include <algorithm>
#include <iostream>
#include <iterator>
#include <vector>
#include <string>
#include <ctime>

#include "td.hpp"

Td td_tgfs;

void split(std::vector<std::string> & tokens, const char *path) {
	std::stringstream path_stream(path);
	std::string token;
	while(std::getline(path_stream, token,  '/'))
	{
		if(!token.empty())
   			tokens.push_back(token);
	}
}

static int tgfs_getattr(const char *path, struct stat *stbuf) {
	std::vector<std::string> tokens;
	split(tokens, path);

	fprintf(stderr, "getattr(%i): %s\n", tokens.size(), path);
	for(auto i = 0; i < tokens.size(); i++) {
		fprintf(stderr, "token %i: %s\n", i, tokens[i].c_str());
	}

	if(tokens.size() == 0) {
		stbuf->st_mode = S_IFDIR | 0500;
		stbuf->st_nlink = 2;
		return 0;
	} else if(tokens.size() == 1) {
		std::map<std::int64_t, std::string>::iterator it;
		for(it = td_tgfs.chat_title_.begin(); it != td_tgfs.chat_title_.end(); it++) {
			if(tokens[0].compare(it->second) == 0) {
				auto chat_it = td_tgfs.chats_.find(it->first);
				// Perm const 
				stbuf->st_mode = S_IFDIR | 0700;
				// Unread count
				stbuf->st_nlink = chat_it->second->unread_count_;
				// last message time
				if(chat_it->second->last_message_) {
					auto message = chat_it->second->last_message_->date_;
					stbuf->st_atime = message;
					stbuf->st_ctime = message;
					stbuf->st_mtime = message;
				}
				// Determine if public or chat
				if(chat_it->second->type_->get_id() == td_api::chatTypeSupergroup::ID) {
					// Supergroup
					auto type = static_cast<td_api::chatTypeSupergroup*>(chat_it->second->type_.get());
					if(type->is_channel_) {
						// Supergroup-channel
						stbuf->st_mode &=~ 0200;
					}
				} else if(chat_it->second->type_->get_id() == td_api::chatTypePrivate::ID) {
					// Chat with user
					auto type = static_cast<td_api::chatTypePrivate*>(chat_it->second->type_.get());
					auto user = td_tgfs.users_.find(type->user_id_);
					// By default we don't know user's last seen time
					std::int32_t time = 0;
					if(user->second->status_->get_id() == td_api::userStatusOnline::ID) {
						// User is online
						time = std::time(nullptr);
					} else if(user->second->status_->get_id() == td_api::userStatusOffline::ID) {
						// User is offline, but shares last seen time
						auto status = static_cast<td_api::userStatusOffline*>(user->second->status_.get());
						time = status->was_online_;
					}
					stbuf->st_atime = time;
					stbuf->st_ctime = time;
					stbuf->st_mtime = time;
				}

				return 0;
			}
		}
	}
	return -ENOENT;
}

static int tgfs_readdir(const char *path, void *buf, fuse_fill_dir_t filler, off_t offset, struct fuse_file_info *fi) {
	if(strncmp(path, "/", 2) == 0) {
		filler(buf, ".", NULL, 0);
		filler(buf, "..", NULL, 0);
		std::map<std::int64_t, std::string>::iterator it;
		for(it = td_tgfs.chat_title_.begin(); it != td_tgfs.chat_title_.end(); it++) {
			filler(buf, it->second.c_str(), NULL, 0);
		}
		return 0;
	}
	return -ENOENT;
}

static int tgfs_open(const char *path, struct fuse_file_info *fi) {
	return -ENOENT;
}

static int tgfs_read(const char *path, char *buf, size_t size, off_t offset, struct fuse_file_info *fi) {
	return -ENOENT;
}

static const struct fuse_operations tgfs_oper = {
	.getattr	= tgfs_getattr,
	.readdir	= tgfs_readdir,
	.open		= tgfs_open,
	.read		= tgfs_read,
};

int main(int argc, char* argv[]) {
	td_tgfs.auth();
	td_tgfs.getContacts();
	int fuse_result = fuse_main(argc, argv, &tgfs_oper, NULL);
	return fuse_result;
}
