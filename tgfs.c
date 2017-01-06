#define FUSE_USE_VERSION 26

#define _XOPEN_SOURCE 600

#include <fuse.h>
#include <stdio.h>
#include <string.h>
#include <errno.h>
#include <fcntl.h>
#include <malloc.h>
#include <stdlib.h>
#include <unistd.h>

#include "socket_tasks.h"
#include "jsmn/jsmn.h"
#include "tg_data.h"

extern tg_data_t tg;

static int tgfs_getattr(const char *path, struct stat *stbuf)
{
	memset(stbuf, 0, sizeof(struct stat));
	
	stbuf->st_uid = getuid();
	stbuf->st_gid = getgid();
	
	if (strcmp(path, "/") == 0) {
		stbuf->st_mode = S_IFDIR | 0500;
		stbuf->st_nlink = 2;
		return 0;
	}
	
	printf("Path: '%s'\n", path);
	size_t c[10];
	size_t n = 0;
	for(size_t i = 0; i < strlen(path); i++) {
		if(path[i] == '/') {
			c[n] = i + 1;
			n++;
		}
	}
	c[n] = strlen(path) + 1;
	
	for(int i = 0; i <= n; i++) {
		printf("%li ", c[i]);
	}
	printf("\n");
	
	tg_peer_t* peer = tg_find_peer_by_name(path + 1, c[1] - c[0] - 1);
	if(peer == NULL)
		return -ENOENT;
	
	if(n == 1) {
		/* Root dir item */
		stbuf->st_mode = S_IFDIR | 0500;
		stbuf->st_nlink = 2;
		stbuf->st_atime = peer->last_seen;
		stbuf->st_ctime = peer->last_seen;
		stbuf->st_mtime = peer->last_seen;
		stbuf->st_size = peer->msg;
		//stbuf->st_blksize = peer->msg;
		return 0;
	}
	
	if(n == 2) {
		if(path[c[1]] == 'P' || path[c[1]] == 'A') {
			stbuf->st_mode = S_IFDIR | 0501;
			stbuf->st_nlink = 2;
			return 0;
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
			if(strlen(tg.peers[i].print_name)) {
				filler(buf, tg.peers[i].print_name, NULL, 0); 
			}
		}
		return 0;
	}
	for(size_t i = 0; i < tg.peers_count; i++) {
		size_t len = strlen(tg.peers[i].print_name);
		if(strcmp(tg.peers[i].print_name, path + 1) == 0) {
			if(len == strlen(path + 1)) {
				filler(buf, ".", NULL, 0);
				filler(buf, "..", NULL, 0);
				filler(buf, "Photo", NULL, 0);
				filler(buf, "Audio", NULL, 0);
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
}

int main(int argc, char *argv[]) {
	socket_init();
	tg_init();
	tg_print_peer_t(&tg.peers[tg.peers_count - 4]);
	return fuse_main(argc, argv, &tgfs_oper, NULL);
}
