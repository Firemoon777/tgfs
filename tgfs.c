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

#include <sys/types.h>
#include <sys/stat.h>
#include <fcntl.h>


#include "socket_tasks.h"
#include "jsmn/jsmn.h"
#include "tg_data.h"

#define TGLUF_SELF (1 << 19)

extern tg_data_t tg;

int tgfs_fd = -1;
char tgfs_buff[1000];

static int tgfs_getattr(const char *path, struct stat *stbuf)
{
	//printf("getattr(): %s\n", path);
	memset(stbuf, 0, sizeof(struct stat));
	
	stbuf->st_uid = getuid();
	stbuf->st_gid = getgid();
	
	//printf("buff: %s\n", tgfs_buff);
	
	if(strcmp(path, tgfs_buff) == 0) {
		printf("Ok\n");
		stbuf->st_mode = S_IFREG | 0200;
		stbuf->st_nlink = 1;
		return 0;
	}
	
	if (strcmp(path, "/") == 0) {
		stbuf->st_mode = S_IFDIR | 0500;
		stbuf->st_nlink = 2;
		return 0;
	}
	
	size_t c[10];
	size_t n = 0;
	for(size_t i = 0; i < strlen(path); i++) {
		if(path[i] == '/') {
			c[n] = i + 1;
			n++;
		}
	}
	c[n] = strlen(path) + 1;
	
	tg_peer_t* peer = tg_find_peer_by_name(path + 1, c[1] - c[0] - 1);
	
	if(peer == NULL) {
		return -ENOENT;
	}
	
	if(n == 1) {
		/* Root dir item */
		if(peer->peer_type != TG_CHANNEL) {
			stbuf->st_mode = S_IFDIR | 0700;
		} else {
			stbuf->st_mode = S_IFDIR | 0500;
		}
		if(peer->flags & TGLUF_SELF) {
			stbuf->st_mode |= 4000;
		}
		stbuf->st_atime = peer->last_seen;
		stbuf->st_ctime = peer->last_seen;
		stbuf->st_mtime = peer->last_seen;
		stbuf->st_nlink = peer->photo_size + 
							peer->audio_size + 
							peer->voice_size + 
							peer->documents_size + 
							peer->gif_size + 
							peer->video_size;
		stbuf->st_ino = peer->peer_id;
		return 0;
	}
	
	if(n == 2) {
		if(strncmp(path + c[1], "Photo", 5) == 0) {
			stbuf->st_mode = S_IFDIR | 0500;
			stbuf->st_nlink = peer->photo_size;
			return 0;
		} else if(strncmp(path + c[1], "Audio", 5) == 0) {
			stbuf->st_mode = S_IFDIR | 0500;
			stbuf->st_nlink = peer->audio_size;
			return 0;
		} else if(strncmp(path + c[1], "Voice", 5) == 0) {
			stbuf->st_mode = S_IFDIR | 0500;
			stbuf->st_nlink = peer->voice_size;
			return 0;
		} else if(strncmp(path + c[1], "Document", 8) == 0) {
			stbuf->st_mode = S_IFDIR | 0500;
			stbuf->st_nlink = peer->documents_size;
			return 0;
		} else if(strncmp(path + c[1], "Video", 5) == 0) {
			stbuf->st_mode = S_IFDIR | 0500;
			stbuf->st_nlink = peer->video_size;
			return 0;
		} else if(strncmp(path + c[1], "Gif", 3) == 0) {
			stbuf->st_mode = S_IFDIR | 0500;
			stbuf->st_nlink = peer->gif_size;
			return 0;
		} 
	}
	
	if(n == 3) {
		size_t message_count;
		tg_msg_t* messages;
		int media_type = tg_get_media_type_by_string(path + c[1]);
		uint32_t hash = tg_string_hash(path + c[2]);
		tg_get_msg_array_by_media_type(&messages, &message_count, peer, media_type);
		while(messages) {
			if(messages->caption_hash == hash) {
				stbuf->st_mode = S_IFREG | 0400;
				stbuf->st_nlink = 1;
				stbuf->st_mtime = messages->timestamp;
				stbuf->st_size = messages->size;
				return 0;
			}
			messages = messages->next;
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
	
	size_t c[10];
	size_t n = 0;
	for(size_t i = 0; i < strlen(path); i++) {
		if(path[i] == '/') {
			c[n] = i + 1;
			n++;
		}
	}
	c[n] = strlen(path) + 1;
	
	if(n == 1) {
		filler(buf, ".", NULL, 0);
		filler(buf, "..", NULL, 0);
		filler(buf, "Photo", NULL, 0);
		filler(buf, "Audio", NULL, 0);
		filler(buf, "Video", NULL, 0);
		filler(buf, "Voice", NULL, 0);
		filler(buf, "Documents", NULL, 0);
		return 0;
	}	
	tg_peer_t* peer = tg_find_peer_by_name(path + 1, c[1] - c[0] - 1);
	if(n == 2) {
		int media_type = tg_get_media_type_by_string(path + c[1]);
		if(media_type > 0) {
			filler(buf, ".", NULL, 0);
			filler(buf, "..", NULL, 0);
			tg_search_msg(peer, media_type, "");
			tg_msg_t* msg;
			size_t size;
			tg_get_msg_array_by_media_type(&msg, &size, peer, media_type);
				
			while(msg) {
				filler(buf, msg->caption, NULL, 0);
				msg = msg->next;
			}
			return 0;
		}
	}
	return -ENOENT;
}

static int tgfs_open(const char *path, struct fuse_file_info *fi)
{
	printf("open(): %s\n", path);
	
	if(strcmp(path, tgfs_buff) == 0) {
		return 0;
	}
	
	size_t c[10];
	size_t n = 0;
	for(size_t i = 0; i < strlen(path); i++) {
		if(path[i] == '/') {
			c[n] = i + 1;
			n++;
		}
	}
	c[n] = strlen(path) + 1;
	
	tg_peer_t* peer = tg_find_peer_by_name(path + 1, c[1] - c[0] - 1);
	if(peer == NULL) {
		return -ENOENT;
	}
	if(n == 3) {
		
		char name[255];
		if(tg_download_file(name, peer, path + c[2], tg_get_media_type_by_string(path + c[1]))) {
			return -ENOENT;
		}
		printf("Filename: %s\n", name);
		tgfs_fd = open(name, O_RDONLY);
		return tgfs_fd > 0 ? 0 : -ENOENT;
	}
	return -ENOENT;
}

static int tgfs_read(const char *path, char *buf, size_t size, off_t offset,
		      struct fuse_file_info *fi)
{
	printf("read(): %s\n", path);
	(void) fi;
	return pread(tgfs_fd, buf, size, offset);
}

static int tgfs_create(const char *path, mode_t mode,
					struct fuse_file_info *fi) {
						
	(void) fi;
	printf("create(): %s\n", path);
	strcpy(tgfs_buff, path);
	remove("/tmp/tgfs_tmp.jpg");
	tgfs_fd = open("/tmp/tgfs_tmp.jpg",  O_WRONLY | O_CREAT);
	printf(" = %i\n", tgfs_fd);
	return 0;
}

static int tgfs_write(const char *path, const char *buf, size_t size, off_t offset, 
			struct fuse_file_info *fi) {
	(void) fi;
	int res = write(tgfs_fd, buf, size);
	printf("write: %s #%i(%li -> %li) = %i\n", path, tgfs_fd, size, offset, res);
	
	return res;
}

int tgfs_release(const char *path, struct fuse_file_info *fi) {
	printf("release:  %s\n", path);
	close(tgfs_fd);
	tgfs_fd = -1;
	
	//size_t c[10];
	size_t n = 0;
	for(size_t i = 0; i < strlen(path); i++) {
		if(path[i] == '/') {
			//c[n] = i + 1;
			n++;
		}
	}
	//c[n] = strlen(path) + 1;
	
	if(n == 2) {
		
		char buff[1000];
		char req[1000];
		size_t s = 1;
		while(path[s] != '/')
			s++;
		strncpy(buff, path + 1, s - 1);
		buff[s] = 0;
		sprintf(req, "post_photo %s %s %s\n", buff, "/tmp/tgfs_tmp.jpg", path + s + 1);
		printf("req: %s\n", req);
		//socket_send_string(req, strlen(req));
	}
	return 0;
}

int tgfs_truncate(const char *path, off_t newsize) {
	printf("truncate:  %s\n", path);
	truncate("/tmp/tgfs_tmp.jpg", newsize);
	return 0;
}

static struct fuse_operations tgfs_oper = {
	.getattr	= tgfs_getattr,
	.readdir	= tgfs_readdir,
	.open		= tgfs_open,
	.read		= tgfs_read,
	.write		= tgfs_write,
	.create     = tgfs_create,
	.truncate   = tgfs_truncate,
	.release	= tgfs_release
};

int main(int argc, char *argv[]) {
	// 
	// ./$(PROJECT_NAME) -f -s -o direct_io test -o use_ino #-o debug
	socket_init();
	tg_init();
	int result = fuse_main(argc, argv, &tgfs_oper, NULL);
	socket_close();
	return result;
}
