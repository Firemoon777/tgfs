#define FUSE_USE_VERSION 26

#define _XOPEN_SOURCE 600

#include <fuse.h>
#include <stdio.h>
#include <string.h>
#include <errno.h>
#include <fcntl.h>
#include <malloc.h>
#include <stdlib.h>

#include "socket_tasks.h"
#include "jsmn/jsmn.h"
#include "tg_data.h"
 /*
static int tgfs_getattr(const char *path, struct stat *stbuf)
{
	 memset(stbuf, 0, sizeof(struct stat));
	if (strcmp(path, "/") == 0) {
		stbuf->st_mode = S_IFDIR | 0755;
		stbuf->st_nlink = 2;
		return 0;
	} else {
		for(size_t i = 0; i < tg.peers_count; i++) {
			size_t len = strlen(tg.peers[i].peer_name);
			if(strncmp(tg.peers[i].peer_name, path + 1, len) == 0) {
				if(len == strlen(path + 1)) {
					stbuf->st_mode = S_IFDIR | 0755;
					stbuf->st_nlink = 2;
					stbuf->st_mtime = 90000;
					return 0;
				} else if(strncmp(path + 2 + len, "Photo", 5) == 0) {
					if(strlen(path) == len + 2 + 5) {
						stbuf->st_mode = S_IFDIR | 0755;
						stbuf->st_nlink = 2;
						stbuf->st_mtime = 90000;
					} else {
						stbuf->st_mode = S_IFREG | 0644;
						stbuf->st_nlink = 2;
						stbuf->st_mtime = 90000;
					}
					return 0;
				} else if(strcmp(path + 2 + len, "Video") == 0) {
					stbuf->st_mode = S_IFDIR | 0755;
					stbuf->st_nlink = 2;
					stbuf->st_mtime = 90000;
					return 0;
				} else if(strcmp(path + 2 + len, "Audio") == 0) {
					stbuf->st_mode = S_IFDIR | 0755;
					stbuf->st_nlink = 2;
					stbuf->st_mtime = 90000;
					return 0;
				} else if(strcmp(path + 2 + len, "Voice") == 0) {
					stbuf->st_mode = S_IFDIR | 0755;
					stbuf->st_nlink = 2;
					stbuf->st_mtime = 90000;
					return 0;
				} else if(strcmp(path + 2 + len, "test.msg")  == 0) {
					stbuf->st_mode = S_IFREG | 0644;
					stbuf->st_nlink = 1;
					stbuf->st_mtime = 90000;
					return 0;
				}
				return -ENOENT;
			}
		}
	}
	return -ENOENT;
}

static int tgfs_readdir(const char *path, void *buf, fuse_fill_dir_t filler,
			 off_t offset, struct fuse_file_info *fi)
{
	(void) offset;
	(void) fi;

	if (strcmp(path, "/") == 0) {
		filler(buf, ".", NULL, 0);
		filler(buf, "..", NULL, 0);
		 for(size_t i = 0; i < tg.peers_count; i++) {
			filler(buf, tg.peers[i].peer_name, NULL, 0); 
		}
		return 0;
	}
	
	 for(size_t i = 0; i < tg.peers_count; i++) {
		if(strncmp(tg.peers[i].peer_name, path + 1, strlen(tg.peers[i].peer_name)) == 0) {
			if(strcmp(path + 2 + strlen(tg.peers[i].peer_name), "Photo") == 0) {
				filler(buf, ".", NULL, 0);
				filler(buf, "..", NULL, 0);
				tg_get_msg_photo(&tg.peers[i]);
				return 0;
			} else {
				filler(buf, ".", NULL, 0);
				filler(buf, "..", NULL, 0);
				filler(buf, "test.msg", NULL, 0);
				filler(buf, "Photo", NULL, 0);
				filler(buf, "Video", NULL, 0);
				filler(buf, "Audio", NULL, 0);
				filler(buf, "Voice", NULL, 0);
				return 0;
			}
		}
	}
	return -ENOENT;
}

static int tgfs_open(const char *path, struct fuse_file_info *fi)
{

	return -ENOENT;

	 if ((fi->flags & 3) != O_RDONLY)
		return -EACCES;

	return 0;
}

static int tgfs_read(const char *path, char *buf, size_t size, off_t offset,
		      struct fuse_file_info *fi)
{
	(void) fi;
	char p[1035];
	
	size_t len = strlen(p);
	if (offset < len) {
		if (offset + size > len)
			size = len - offset;
		memcpy(buf, p + offset, size);
	} else
		size = 0;
	return size;
}

static struct fuse_operations tgfs_oper = {
	.getattr	= tgfs_getattr,
	.readdir	= tgfs_readdir,
	.open		= tgfs_open,
	.read		= tgfs_read,
};

char* getNameFromString(char* src) {
	size_t i, j = 1;
	size_t len = strlen(src);
	char* result;
	if(len < 15)
		return NULL;
	result = (char*)malloc(sizeof(char)*len);
	i = (src[4] == ' ') ? 5 : 8;
	result[0] = '/';
	for(; i < len; i++, j++) {
		if(src[i] == ':') 
			break;
		if(src[i] == ' ') {
			result[j] = '_';
			continue;
		}
		result[j] = src[i];
	}
	if(i == len) {
		free(result);
		return NULL;
	}
	result[j+1] = 0;
	return result;
}*/

int main(int argc, char *argv[]) {
	
	socket_init();
	tg_init();
	return 0; /*fuse_main(argc, argv, &tgfs_oper, NULL);*/
}
