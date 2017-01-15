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
#include <pthread.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <fcntl.h>


#include "socket_tasks.h"
#include "jsmn/jsmn.h"
#include "tg_data.h"

#define TGLUF_SELF (1 << 19)
#define TGLUF_HAS_PHOTO (1 << 1)

extern tg_data_t tg;
extern pthread_mutex_t lock;

tg_fd* tgfs_fd = NULL;
char tgfs_buff[1000];

static int tgfs_getattr(const char *path, struct stat *stbuf)
{
	//printf("getattr(): %s\n", path);
	memset(stbuf, 0, sizeof(struct stat));
	
	stbuf->st_uid = getuid();
	stbuf->st_gid = getgid();
	
	//printf("buff: %s\n", tgfs_buff);
	
	tg_fd* f = tg_search_fd(tgfs_fd, path);
	if(f) {
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
		if(peer->flags & TGLUF_SELF && peer->peer_type == TG_USER) {
			stbuf->st_mode = S_IFDIR | 01700;
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
		} else if(strncmp(path + c[1], "Avatar.jpg", 10) == 0) {
			if(peer->flags & TGLUF_SELF && peer->peer_type == TG_USER) {
				stbuf->st_mode = S_IFREG | 0600;
			} else {
				stbuf->st_mode = S_IFREG | 0500;
			}
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
	tg_peer_t* peer = tg_find_peer_by_name(path + 1, c[1] - c[0] - 1);
	if(n == 1) {
		filler(buf, ".", NULL, 0);
		filler(buf, "..", NULL, 0);
		filler(buf, "Photo", NULL, 0);
		filler(buf, "Audio", NULL, 0);
		filler(buf, "Video", NULL, 0);
		filler(buf, "Voice", NULL, 0);
		filler(buf, "Documents", NULL, 0);
		if(peer->flags & TGLUF_HAS_PHOTO) {
			filler(buf, "Avatar.jpg", NULL, 0);
		}
		return 0;
	}	
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
		tg_fd* d_file = tg_init_fd();
		size_t len = strlen(path);
		d_file->path = (char*)malloc((len+1)*sizeof(char));
		strcpy(d_file->path, path);
		d_file->path[len] = 0;
		d_file->path_hash = tg_string_hash(d_file->path);
		d_file->fd = open(name, O_RDONLY);
		d_file->file_type = TG_FILE_DOWNLOAD;
		if(d_file->fd > 0) {
			tg_add_fd(&tgfs_fd, d_file);
			return 0;
		}
		int t = d_file->fd;
		free(d_file);
		return t;
	}
	return -ENOENT;
}

static int tgfs_read(const char *path, char *buf, size_t size, off_t offset,
		      struct fuse_file_info *fi)
{
	printf("read(): %s\n", path);
	(void) fi;
	tg_fd* u_file = tg_search_fd(tgfs_fd, path);
	return pread(u_file->fd, buf, size, offset);
}

static int tgfs_create(const char *path, mode_t mode,
					struct fuse_file_info *fi) {
						
	(void) fi;
	printf("create(): %s\n", path);

	size_t len = strlen(path);
	tg_fd* u_file = tg_init_fd();
	u_file->path = (char*)malloc((len+1)*sizeof(char));
	strcpy(u_file->path, path);
	u_file->path[len] = 0;
	u_file->path_hash = tg_string_hash(u_file->path);
	char real_path[4096];
	char ext[4];
	strncpy(ext, path + len - 3, 3);
	ext[3] = 0;
	//get_downloads_dir(real_path);
	strcpy(real_path, "/tmp/");
	strcat(real_path, strrchr(path, '/') + 1);
	printf("real path: %s\n", real_path);
	u_file->real_path = (char*)malloc(strlen(real_path) * sizeof(char));
	strcpy(u_file->real_path, real_path);
	u_file->fd = open(real_path,  O_WRONLY | O_CREAT, 0777);
	u_file->file_type = TG_FILE_UPLOAD;
	if(u_file->fd > 0) {
		tg_add_fd(&tgfs_fd, u_file);
		return 0;
	}
	int t = u_file->fd;
	free(u_file);
	return t;
}

static int tgfs_write(const char *path, const char *buf, size_t size, off_t offset, 
			struct fuse_file_info *fi) {
	(void) fi;
	tg_fd* u_file = tg_search_fd(tgfs_fd, path);
	int res = pwrite(u_file->fd, buf, size, offset);
	printf("write: %s #%i(%li -> %li) = %i\n", path, u_file->fd, size, offset, res);
	
	return res;
}

int tgfs_release(const char *path, struct fuse_file_info *fi) {
	printf("release:  %s\n", path);
	
	size_t c[10];
	size_t n = 0;
	for(size_t i = 0; i < strlen(path); i++) {
		if(path[i] == '/') {
			c[n] = i + 1;
			n++;
		}
	}
	c[n] = strlen(path) + 1;
	
	tg_fd* f = tg_search_fd(tgfs_fd, path);
	if(f == NULL) {
		return -ENOENT;
	}
	
	if(n >= 2  && f->file_type == TG_FILE_UPLOAD) {
		
		char buff[1000];
		char req[1000];
		size_t s = 1;
		while(path[s] != '/')
			s++;
			
		strncpy(buff, path + 1, s - 1);
		buff[s-1] = 0;
		if(n == 3) {
			int media_type = tg_get_media_type_by_string(path + c[1]);
			switch(media_type) {
				case TG_MEDIA_PHOTO:
					sprintf(req, "post_photo %s %s\n", buff, f->real_path);
					break;
				case TG_MEDIA_DOCUMENT:
					sprintf(req, "post_document %s %s\n", buff, f->real_path);
					break;
				case TG_MEDIA_VIDEO:
					sprintf(req, "post_video %s %s\n", buff, f->real_path);
					break;
				case TG_MEDIA_AUDIO:
					sprintf(req, "post_audio %s %s\n", buff, f->real_path);
					break;
				default:
					sprintf(req, "post_file %s %s\n", buff, f->real_path);
					break;
			}
		} else {
			sprintf(req, "post_file %s %s\n", buff, f->real_path);
		}
		printf("req: %s\n", req);

		pthread_mutex_lock(&lock);
		socket_send_string(req, strlen(req));
		char* json;
		size_t len;
		socket_read_data(&json, &len);
		pthread_mutex_unlock(&lock);		
	}
	
	remove(f->real_path);
	close(f->fd);
	tg_remove_fd(&tgfs_fd, f);
	
	return 0;
}

int tgfs_truncate(const char *path, off_t newsize) {
	printf("truncate:  %s\n", path);
	return 0;
}

static void* tgfs_init(struct fuse_conn_info *conn) {
	(void) conn;
	return NULL;
}

static struct fuse_operations tgfs_oper = {
	.init 		= tgfs_init,
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
	
	struct fuse_args args = FUSE_ARGS_INIT(argc, argv);

    fuse_opt_parse(&args, NULL, NULL, NULL);
    fuse_opt_add_arg(&args, "-odirect_io");
    fuse_opt_add_arg(&args, "-ouse_ino");
#ifdef DEBUG
	fuse_opt_add_arg(&args, "-f");
	/*fuse_opt_add_arg(&args, "-d");*/
#endif
    
	socket_init();
	tg_init();
	int result = fuse_main(args.argc, args.argv, &tgfs_oper, NULL);
	socket_close();
	return result;
}
