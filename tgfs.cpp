#define FUSE_USE_VERSION 26

#include <string.h>
#include <errno.h>
#include <fuse.h>

#include <algorithm>
#include <iostream>
#include <iterator>
#include <vector>
#include <string>

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
			fprintf(stderr, "compare: %s == %s\n", it->second.c_str(), tokens[0].c_str());
			if(tokens[0].compare(it->second) == 0) {
				fprintf(stderr, "success\n");
				stbuf->st_mode = S_IFDIR | 0500;
				stbuf->st_nlink = 2;
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
