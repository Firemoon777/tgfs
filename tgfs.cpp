#define FUSE_USE_VERSION 26

#include <string.h>
#include <errno.h>
#include <fuse.h>
#include <unistd.h>
#include <sys/types.h>

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

static int tgfs_getattr_root(std::int64_t id_, struct stat *stbuf) {
	auto chat_it = td_tgfs.chats_.find(id_);
	if(chat_it == td_tgfs.chats_.end()) {
		return -ENOENT;
	}
	// inode
	stbuf->st_ino = id_;
	// Perm const 
	stbuf->st_mode = S_IFDIR | 0700;
	// Unread count
	stbuf->st_nlink = chat_it->second->unread_count_;
	// set m(essage)time
	if(chat_it->second->last_message_) {
		auto message = chat_it->second->last_message_->date_;
		stbuf->st_mtime = message;
	}
	if(chat_it->second->type_->get_id() == td_api::chatTypePrivate::ID) {
		// Private chat
		auto user_id = static_cast<td_api::chatTypePrivate*>(chat_it->second->type_.get())->user_id_;
		auto user_it = td_tgfs.users_.find(user_id);
		if(user_it->second->type_->get_id() == td_api::userTypeBot::ID) {
			// Chat with bot
			stbuf->st_mode |= 0006;
			stbuf->st_mtime = stbuf->st_atime;
		} else if(user_it->second->type_->get_id() == td_api::userTypeRegular::ID) {
			// Regular chat
			if(user_it->second->status_->get_id() == td_api::userStatusOnline::ID) {
				// User is online, set atime to now
				stbuf->st_atime = std::time(nullptr);
			} else if(user_it->second->status_->get_id() == td_api::userStatusOffline::ID) {
				// User is offline, but we know accurate date of last online
				auto status = static_cast<td_api::userStatusOffline*>(user_it->second->status_.get());
				stbuf->st_atime = status->was_online_;
			} else {
				// We have no idea about user's last online
				// TODO: round atime to day/week/month ago
			}
		} else {
			// Deleted on unknown user	
		}
	} else if(chat_it->second->type_->get_id() == td_api::chatTypeSupergroup::ID) {
		// Supergroup
		auto type = static_cast<td_api::chatTypeSupergroup*>(chat_it->second->type_.get());
		if(type->is_channel_) {
			// Supergroup-channel, no write permission
			// TODO: check admins if available and set correct permissions
			stbuf->st_mode &=~ 0220;
		}
		// Chat is accessible by people in group, so g+rw
		stbuf->st_mode |= 0060;
		// Since groups have no last seen date, let atime equals to mtime
		stbuf->st_mtime = stbuf->st_atime;
	} else if(chat_it->second->type_->get_id() == td_api::chatTypeBasicGroup::ID) {
		// Basic groups, nothing special
		stbuf->st_mode |= 0060;
		stbuf->st_mtime = stbuf->st_atime;
	}
	return 0;
}

static int tgfs_getattr_chat(std::int64_t id_, std::string token, struct stat *stbuf) {
	auto chat_it = td_tgfs.chats_.find(id_);
	if(chat_it == td_tgfs.chats_.end()) {
		return -ENOENT;
	}
	if(token.compare("photo.jpg") == 0) {
		// Photo file
		stbuf->st_mode = S_IFREG | 0600;
		stbuf->st_size = chat_it->second->photo_->big_->size_;
		if(stbuf->st_size == 0) {
			stbuf->st_size = chat_it->second->photo_->big_->expected_size_;
		}
		if(stbuf->st_size == 0) {
			// 100kb stub
			// Some clever apps prefer to ignore file with zero size
			stbuf->st_size = 100*1024;
		}
	} else {
		// Unknown
		return -ENOENT;
	}
	return 0;
}

static int tgfs_getattr(const char *path, struct stat *stbuf) {
	std::vector<std::string> tokens;
	split(tokens, path);

	std::map<std::int64_t, std::string>::iterator it;

	stbuf->st_uid = getuid();
	stbuf->st_gid = getgid();

	if(tokens.size() == 0) {
		stbuf->st_mode = S_IFDIR | 0500;
		stbuf->st_nlink = 2;
		return 0;
	} else if(tokens.size() == 1) {
		for(it = td_tgfs.chat_title_.begin(); it != td_tgfs.chat_title_.end(); it++) {
			if(tokens[0].compare(it->second) == 0) {
				return tgfs_getattr_root(it->first, stbuf);
			}
		}
	} else if(tokens.size() == 2) {
		for(it = td_tgfs.chat_title_.begin(); it != td_tgfs.chat_title_.end(); it++) {
			if(tokens[0].compare(it->second) == 0) {
				return tgfs_getattr_chat(it->first, tokens[1], stbuf);
			}
		}

	}
	return -ENOENT;
}

static int tgfs_readdir_chat(std::int64_t id_, void* buf, fuse_fill_dir_t filler) {
	auto chat = td_tgfs.chats_.find(id_);
	if(chat == td_tgfs.chats_.end()) {
		return -ENOENT;
	}
	if(chat->second->photo_) {
		std::string name = "photo";
		filler(buf, "photo.jpg", NULL, 0);
	}
	return 0;
}

static int tgfs_readdir(const char *path, void *buf, fuse_fill_dir_t filler, off_t offset, struct fuse_file_info *fi) {
	std::vector<std::string> tokens;
	split(tokens, path);
		
	filler(buf, ".", NULL, 0);
	filler(buf, "..", NULL, 0);
		
	std::map<std::int64_t, std::string>::iterator it;

	if(tokens.size() == 0) {
		for(it = td_tgfs.chat_title_.begin(); it != td_tgfs.chat_title_.end(); it++) {
			filler(buf, it->second.c_str(), NULL, 0);
		}
		return 0;
	} else if(tokens.size() == 1) {
		for(it = td_tgfs.chat_title_.begin(); it != td_tgfs.chat_title_.end(); it++) {
			if(tokens[0].compare(it->second) == 0) {
				return tgfs_readdir_chat(it->first, buf, filler);
			}
		}
		return -ENOENT;
	} else if(tokens.size() == 2) {
		return 0;
	}
	return -ENOENT;
}

static int tgfs_open_chat(std::int64_t id_, std::string token, struct fuse_file_info *fi) {
	auto chat = td_tgfs.chats_.find(id_);
	if(chat == td_tgfs.chats_.end()) {
		return -ENOENT;
	}
	if(token.compare("photo.jpg") == 0 && chat->second->photo_) {
		fi->fh = chat->second->photo_->big_->id_;
		auto files_open_it = td_tgfs.files_open_.find(fi->fh); 	
		if(files_open_it != td_tgfs.files_open_.end() && files_open_it->second->fd_ > 0) {
			return 0;
		}
		if(td_tgfs.download_file(fi->fh, 1) == -1) {
			return -ENOENT;
		}
		while(td_tgfs.files_[fi->fh]->local_->path_.empty()) {
		}
		if(td_tgfs.files_open_.find(fi->fh) == td_tgfs.files_open_.end()) {
			//td_tgfs.files_open_.insert(std::make_pair(fi->fh, new Td_file_info()));
			td_tgfs.files_open_[fi->fh] = new Td_file_info();
		}
		td_tgfs.files_open_[fi->fh]->filename_ = td_tgfs.files_[fi->fh]->local_->path_;
		td_tgfs.files_open_[fi->fh]->open_file();

		if(td_tgfs.files_open_[fi->fh]->fd_ > 0) {
			return 0;
		}
		td_tgfs.files_open_[fi->fh]->close_file();

	}
	return -ENOENT;
}

static int tgfs_open(const char *path, struct fuse_file_info *fi) {
	std::cerr << "open ( " << path << ")\n";
	std::vector<std::string> tokens;
	split(tokens, path);

	std::map<std::int64_t, std::string>::iterator it;
		
	if(tokens.size() == 2) {
		for(it = td_tgfs.chat_title_.begin(); it != td_tgfs.chat_title_.end(); it++) {
			if(tokens[0].compare(it->second) == 0) {
				return tgfs_open_chat(it->first, tokens[1], fi);
			}
		}
		return -ENOENT;
	}

	return -ENOENT;
}

static int tgfs_read(const char *path, char *buf, size_t size, off_t offset, struct fuse_file_info *fi) {
	auto file = td_tgfs.files_.find(fi->fh);
	if(file == td_tgfs.files_.end()) {
		return -EIO;
	}
	std::cerr << "read ( " << path << " / " << td_tgfs.files_open_[fi->fh]->filename_ <<")\n";
	std::cerr << "size = " << size << ", offset = " << offset << std::endl;
	auto downloaded = file->second->local_->downloaded_prefix_size_;
	std::cerr << "prefix = " << downloaded << std::endl;
	while(offset + size > downloaded) {
		if(file->second->local_->is_downloading_completed_) {
			td_tgfs.files_open_[fi->fh]->change_name_if_needed(td_tgfs.files_[fi->fh]->local_->path_);
			break;
		}
	}

	int count = pread(td_tgfs.files_open_[fi->fh]->fd_, buf, size, offset);
	std::cerr << "read count = " << count << std::endl;
	return count;
}

int tgfs_release(const char *path, struct fuse_file_info *fi) {
	std::cerr << "release ( " << path << " / " << td_tgfs.files_open_[fi->fh]->filename_ <<")\n";
	td_tgfs.files_open_[fi->fh]->close_file();
	return -ENOENT;
}

static const struct fuse_operations tgfs_oper = {
	.getattr	= tgfs_getattr,
	.readdir	= tgfs_readdir,
	.open		= tgfs_open,
	.read		= tgfs_read,
	.release	= tgfs_release
};

int main(int argc, char* argv[]) {
	td_tgfs.auth();
	td_tgfs.getContacts();
	td_tgfs.loop();
	int fuse_result = fuse_main(argc, argv, &tgfs_oper, NULL);
	return fuse_result;
}
