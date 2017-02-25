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

#define PACKAGE_VERSION "0.1"

#define EQ_STR_LIT( x, lit ) (strncmp( (x), (lit), sizeof(lit)) == 0 )
#define TGFS_OPT(t, p, v) { t, offsetof(struct tgfs_config, p), v }

#define TGLUF_SELF (1 << 19)
#define TGLUF_HAS_PHOTO (1 << 1)

#define PATH_MAX_LEVEL 10

extern tg_data_t tg;
extern pthread_mutex_t lock;

tg_fd* tgfs_fd = NULL;

static int slashes_to_index_array(size_t* a, const char *path) {
	size_t n = 0;
	for(size_t i = 0; i < strlen(path); i++) {
		if(path[i] == '/' && n < PATH_MAX_LEVEL - 1) {
			a[n] = i + 1;
			n++;
		}
	}
	a[n] = strlen(path) + 1;
	return n;	
}

static int tgfs_getattr(const char *path, struct stat *stbuf)
{
	memset(stbuf, 0, sizeof(struct stat));
	
	stbuf->st_uid = getuid();
	stbuf->st_gid = getgid();
	
	if (strcmp(path, "/") == 0) {
		stbuf->st_mode = S_IFDIR | 0500;
		stbuf->st_nlink = 2 + tg.peers_count;
		stbuf->st_atime = tg.mount_time;
		stbuf->st_ctime = tg.mount_time;
		stbuf->st_mtime = tg.mount_time;
		return 0;
	}
	
	tg_fd* f = tg_search_fd(tgfs_fd, path);
	if(f) {
		stbuf->st_mode = S_IFREG | 0200;
		stbuf->st_nlink = 1;
		return 0;
	}
	
	size_t c[PATH_MAX_LEVEL];
	size_t n = slashes_to_index_array(c, path);
	
	tg_peer_t* peer = tg_find_peer_by_name(path + 1, c[1] - c[0] - 1);
	
	if(peer == NULL) {
		return -ENOENT;
	}
	
	switch(n) {
		case 1:
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
		case 2:
			if(EQ_STR_LIT(path + c[1], "Photo")) {
				stbuf->st_mode = S_IFDIR | 0500 | (tg.config.enable_unlink * 0200);
				stbuf->st_nlink = peer->photo_size;
				return 0;
			} else if(EQ_STR_LIT(path + c[1], "Audio")) {
				stbuf->st_mode = S_IFDIR | 0500 | (tg.config.enable_unlink * 0200);
				stbuf->st_nlink = peer->audio_size;
				return 0;
			} else if(EQ_STR_LIT(path + c[1], "Voice")) {
				stbuf->st_mode = S_IFDIR | 0500 | (tg.config.enable_unlink * 0200);
				stbuf->st_nlink = peer->voice_size;
				return 0;
			} else if(EQ_STR_LIT(path + c[1], "Documents")) {
				stbuf->st_mode = S_IFDIR | 0500 | (tg.config.enable_unlink * 0200);
				stbuf->st_nlink = peer->documents_size;
				return 0;
			} else if(EQ_STR_LIT(path + c[1], "Video")) {
				stbuf->st_mode = S_IFDIR | 0500;
				stbuf->st_nlink = peer->video_size;
				return 0;
			} else if(EQ_STR_LIT(path + c[1], "git")) {
				stbuf->st_mode = S_IFDIR | 0500;
				stbuf->st_nlink = peer->gif_size;
				return 0;
			} else if(EQ_STR_LIT(path + c[1], "Avatar.jpg")) {
				if(peer->flags & TGLUF_SELF && peer->peer_type == TG_USER) {
					stbuf->st_mode = S_IFREG | 0600;
				} else {
					stbuf->st_mode = S_IFREG | 0500;
				}
				return 0;
			}
			return -ENOENT;
		case 3: {
				size_t message_count;
				tg_msg_t* messages;
				int media_type = tg_get_media_type_by_string(path + c[1]);
				uint32_t hash = tg_string_hash(path + c[2]);
				tg_get_msg_array_by_media_type(&messages, &message_count, peer, media_type);
				while(messages) {
					if(messages->caption_hash == hash) {
						stbuf->st_mode = S_IFREG | 0400  | (tg.config.enable_unlink * 0200);
						stbuf->st_nlink = 1;
						stbuf->st_mtime = messages->timestamp;
						stbuf->st_size = messages->size;
						return 0;
					}
					messages = messages->next;
				}
			}
		default:
			return -ENOENT;
	}
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
	
	size_t c[PATH_MAX_LEVEL];
	size_t n = slashes_to_index_array(c, path);
	
	tg_peer_t* peer = tg_find_peer_by_name(path + 1, c[1] - c[0] - 1);
	
	switch(n) {
		case 1:
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
		case 2: {
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
		default:
			return -ENOENT;
			
	}	
}

static int tgfs_open(const char *path, struct fuse_file_info *fi)
{
	printf("open(): %s\n", path);
	
	size_t c[PATH_MAX_LEVEL];
	size_t n = slashes_to_index_array(c, path);
	
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
	
	size_t c[PATH_MAX_LEVEL];
	slashes_to_index_array(c, path);
	
	tg_peer_t* peer = tg_find_peer_by_name(path + 1, c[1] - c[0] - 1);

	if(peer->peer_type == TG_CHANNEL) {
		return -EACCES;
	}

	size_t len = strlen(path);
	tg_fd* u_file = tg_init_fd();
	u_file->path = (char*)malloc((len+1)*sizeof(char));
	strcpy(u_file->path, path);
	u_file->path[len] = 0;
	u_file->path_hash = tg_string_hash(u_file->path);
	char real_path[MAX_FILEPATH_SIZE];
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
	
	size_t c[PATH_MAX_LEVEL];
	size_t n = slashes_to_index_array(c, path);
	
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

static int tgfs_unlink(const char* path) {
	if(tg.config.enable_unlink == 0) {
		return -ENOSYS;
	}
	printf("unlink: %s\n", path);
	
	size_t c[PATH_MAX_LEVEL];
	size_t n = slashes_to_index_array(c, path);
	
	if(n == 3) {
		tg_peer_t* peer = tg_find_peer_by_name(path + 1, c[1] - c[0] - 1);
		size_t message_count;
		tg_msg_t* messages, *head;
		int media_type = tg_get_media_type_by_string(path + c[1]);
		printf("n = %s\n", peer->print_name);
		uint32_t hash = tg_string_hash(path + c[2]);
		tg_get_msg_array_by_media_type(&messages, &message_count, peer, media_type);
		head = messages;
		while(messages) {
			if(messages->caption_hash == hash) {
				if(strcmp(messages->caption, path + c[2]) == 0) {
					char req[1000];
					sprintf(req, "delete_msg %s\n", messages->id);
					printf("req: %s\n", req);
					
					pthread_mutex_lock(&lock);
					socket_send_string(req, strlen(req));
					char* json;
					size_t len;
					socket_read_data(&json, &len);
					pthread_mutex_unlock(&lock);
					
					tg_msg_remove(&head, messages);
					message_count--;
					tg_set_msg_array_by_media_type(head, message_count, peer, media_type);
					
					return 0;
				}
			}
			messages = messages->next;
		}
	}
	return -ENOENT;
}

static const struct fuse_operations tgfs_oper = {
	.init 		= tgfs_init,
	.getattr	= tgfs_getattr,
	.readdir	= tgfs_readdir,
	.open		= tgfs_open,
	.read		= tgfs_read,
	.write		= tgfs_write,
	.create     = tgfs_create,
	.truncate   = tgfs_truncate,
	.release	= tgfs_release,
	.unlink		= tgfs_unlink
};

enum {
	TGFS_KEY_ENABLE_UNLINK,
	TGFS_KEY_VERSION,
	TGFS_KEY_HELP	
};

static struct fuse_opt tgfs_opts[] = {
     FUSE_OPT_KEY("--enable-unlink", TGFS_KEY_ENABLE_UNLINK),
     FUSE_OPT_KEY("-V",              TGFS_KEY_VERSION),
     FUSE_OPT_KEY("--version",       TGFS_KEY_VERSION),
     FUSE_OPT_KEY("-h",              TGFS_KEY_HELP),
     FUSE_OPT_KEY("--help",          TGFS_KEY_HELP),
     FUSE_OPT_END
};

static int tgfs_opt_proc(void *data, const char *arg, int key, struct fuse_args *outargs) {
	switch(key) {
		case TGFS_KEY_ENABLE_UNLINK:
			tg.config.enable_unlink = 1;
			fprintf(stderr, "Warning: unlink enabled!\n");
			return 0;
		
		case TGFS_KEY_HELP:
			fprintf(stderr,
                     "usage: %s mountpoint [options]\n"
                     "\n"
                     "general options:\n"
                     "    -o opt,[opt...]  mount options\n"
                     "    -h   --help      print help\n"
                     "    -V   --version   print version\n"
                     "\n"
                     "tgfs options:\n"
                     "    --enable-unlink  enable file removing\n"
                     "\n"
                     , outargs->argv[0]);
            fuse_opt_add_arg(outargs, "-ho");
            fuse_main(outargs->argc, outargs->argv, &tgfs_oper, NULL);
			exit(1);
			
		case TGFS_KEY_VERSION:
			fprintf(stderr, "tgfs version %s\n", PACKAGE_VERSION);
			fuse_opt_add_arg(outargs, "--version");
            fuse_main(outargs->argc, outargs->argv, &tgfs_oper, NULL);
			exit(0);
	}
	return 1;
}

int main(int argc, char *argv[]) {
	
	struct fuse_args args = FUSE_ARGS_INIT(argc, argv);

	tg.config.enable_unlink = 0;

    fuse_opt_parse(&args, NULL, tgfs_opts, tgfs_opt_proc);
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
