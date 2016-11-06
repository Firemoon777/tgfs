/*
  FUSE: Filesystem in Userspace
  Copyright (C) 2001-2007  Miklos Szeredi <miklos@szeredi.hu>

  This program can be distributed under the terms of the GNU GPL.
  See the file COPYING.

  gcc -Wall `pkg-config fuse --cflags --libs` hello.c -o hello
*/

#define FUSE_USE_VERSION 26

#include <fuse.h>
#include <stdio.h>
#include <string.h>
#include <errno.h>
#include <fcntl.h>
#include <malloc.h>

#include "string_list.h"

string_list* files;

static int tgfs_getattr(const char *path, struct stat *stbuf)
{
	memset(stbuf, 0, sizeof(struct stat));
	if (strcmp(path, "/") == 0) {
		stbuf->st_mode = S_IFDIR | 0755;
		stbuf->st_nlink = 2;
		return 0;
	} else {
		string_list* cur = files;
		while(cur != NULL) {
			if(strcmp(path, cur->name) == 0) {
				stbuf->st_mode = S_IFREG | 0444;
				stbuf->st_nlink = 1;
				stbuf->st_size = strlen(cur->name);
				return 0;
			}
			cur = cur->next;
		}
	}

	return -ENOENT;
}

static int tgfs_readdir(const char *path, void *buf, fuse_fill_dir_t filler,
			 off_t offset, struct fuse_file_info *fi)
{
	(void) offset;
	(void) fi;

	if (strcmp(path, "/") != 0)
		return -ENOENT;

	filler(buf, ".", NULL, 0);
	filler(buf, "..", NULL, 0);
	string_list* cur = files;
	while(cur != NULL) {
		filler(buf, cur->name + 1, NULL, 0);
		cur = cur->next;
	}

	return 0;
}

static int tgfs_open(const char *path, struct fuse_file_info *fi)
{
	return -ENOENT;

	/*if ((fi->flags & 3) != O_RDONLY)
		return -EACCES;*/

	return 0;
}

static int tgfs_read(const char *path, char *buf, size_t size, off_t offset,
		      struct fuse_file_info *fi)
{
	/*
	size_t len;
	(void) fi;
	if(strcmp(path, tgfs_path) != 0)
		return -ENOENT;

	len = strlen(tgfs_str);
	if (offset < len) {
		if (offset + size > len)
			size = len - offset;
		memcpy(buf, tgfs_str + offset, size);
	} else
		size = 0;

	return size;*/
	return 0;
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
		result[j] = src[i];
	}
	if(i == len) {
		free(result);
		return NULL;
	}
	result[j+1] = 0;
	return result;
}

int main(int argc, char *argv[])
{
	files = string_list_init("/config");
	
	FILE *fp;
	char path[1035];

	/* Open the command for reading. */
	fp = popen("telegram-cli -D -R -C -e 'dialog_list'", "r");
	if (fp == NULL) {
		printf("Failed to run command\n" );
		return 1;
	}

	/* Read the output a line at a time - output it. */
	while (fgets(path, sizeof(path)-1, fp) != NULL) {
		string_list_add_front(&files, getNameFromString(path));
	}
	return fuse_main(argc, argv, &tgfs_oper, NULL);
}
