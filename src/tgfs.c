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
#include <assert.h>

#include "tg.h"

#include <sqlite3.h>

int ready = 0;
int peers_length;
tgl_peer_id_t *peers;

static int parse_path(const char *path, tgl_peer_t **peer, int *type, char **filename) {
	//printf("path: %s\n", path);
	*peer = NULL;
	*type = TGFS_UNKNOWN;
	*filename = NULL; 
	if(strcmp(path, "/") == 0) {
		return 0;
	}

	int s = 1;
	while(path[s] != '/' && path[s]) {
		s++;
	}
	char *peer_name = (char*)malloc(s*sizeof(char));
	strncpy(peer_name, path + 1, s);
	peer_name[s-1] = 0;
	//printf("first: %s\n", peer_name);
	tgl_peer_t *founded = tgl_peer_get_by_name(TLS, peer_name);
	*peer = founded;
	free(peer_name);

	if(path[s] == 0) {
		return 1;
	}

	path += s;
	s = 1;
	while(path[s] != '/' && path[s]) {
		s++;
	}
	char *dir = (char*)malloc(s*sizeof(char)); 
	strncpy(dir, path + 1, s);
	dir[s-1] = 0;
	//printf("second: %s\n", dir);
	if(strncmp(dir, "Video", 6) == 0) {
		*type = TGFS_VIDEOS;
	}
	if(strncmp(dir, "Photo", 6) == 0) {
		*type = TGFS_PHOTOS;
	}
	if(strncmp(dir, "Documents", 10) == 0) {
		*type = TGFS_DOCUMENTS;
	}
	if(strncmp(dir, "Music", 6) == 0) {
		*type = TGFS_MUSIC;
	}
	free(dir);
	

	if(path[s] == 0 || *type == TGFS_UNKNOWN) {
		return 2;
	}

	*filename = (char*)path + s + 1;
	//printf("third: %s\n", path + s + 1);

	return 3;
}

static int tgfs_getattr(const char *path, struct stat *stbuf)
{
	tgl_peer_t *peer;
	int type;
	char *filename = (char*)path;
	int result = parse_path(path, &peer, &type, &filename);

	stbuf->st_uid = getuid();
	stbuf->st_gid = getgid();

	if(peer == NULL) {
		// Root
		if(result != 0) {
			return -ENOENT;
		}
		stbuf->st_mode = S_IFDIR | 0700;
		stbuf->st_nlink = 2;
		return 0;		
	} else if(type == TGFS_UNKNOWN) {
		// Peer dir
		if(result != 1) {
			return -ENOENT;
		}
		stbuf->st_mode = S_IFDIR | 0700;
		stbuf->st_nlink = 2;
		struct tgl_message *message = peer->last;
		if(message) {
			stbuf->st_mtime = message->date;
		}
		return 0;
	} else if(filename == NULL) {
		// Documents dir
		if(result != 2) {
			return -ENOENT;
		}
		stbuf->st_mode = S_IFDIR | 0700;
		stbuf->st_nlink = 2 + peers_length;
		return 0;		
	} else {
		// File in doc dir
		if(result != 3) {
			return -ENOENT;
		}
		return tg_storage_msg_stat(filename, stbuf, type);
	}
	return -ENOENT;

}

static int tgfs_readdir(const char *path, void *buf, fuse_fill_dir_t filler,
			 off_t offset, struct fuse_file_info *fi)
{
	fprintf(stderr, "readdir started; path: %s\n", path);
	tgl_peer_t *peer;
	int type;
	char *filename = (char*)path;
	int result = parse_path(path, &peer, &type, &filename);
	filler(buf, ".", NULL, 0);
	filler(buf, "..", NULL, 0);
	if (peer == NULL) {
		if(result != 0) {
			return -ENOENT;
		}
		tg_storage_peer_enumerate(buf, filler);
		return 0;
	} else if(type == TGFS_UNKNOWN) {
		if(result != 1) {
			return -ENOENT;
		}
		filler(buf, "Video", NULL, 0);
		filler(buf, "Photo", NULL, 0);
		filler(buf, "Documents", NULL, 0);
		return 0;
	} else if(filename == NULL) {
		if(result != 2) {
			return -ENOENT;
		}
		fprintf(stderr, "ready for downloading!\n");
		tg_donwload_attachments(peer->id, type);	
		fprintf(stderr, "downloaded!\n");
		tg_storage_msg_enumerate_name(peer->id, type, buf, filler);	
		return 0;	
	}
	
	return -ENOENT;
}

static int tgfs_open(const char *path, struct fuse_file_info *fi)
{
	tgl_peer_t *peer;
	int type;
	char *filename = (char*)path;
	int result = parse_path(path, &peer, &type, &filename);
	if(filename != NULL && result == 3) {
		struct tgl_message *msg = tg_storage_msg_by_name(peer, type, filename);
		if(msg == NULL) 
			return -ENOENT;
		return 0;

	}
	return -ENOENT;
}

static int tgfs_read(const char *path, char *buf, size_t size, off_t offset,
		      struct fuse_file_info *fi)
{
	tgl_peer_t *peer;
	int type;
	char *filename = (char*)path;
	int result = parse_path(path, &peer, &type, &filename);
	if(filename != NULL && result == 3) {
		printf("read(%li, %li): %s\n", size, offset, path);
		struct tgl_message *msg = tg_storage_msg_by_name(peer, type, filename);
		if(msg == NULL) 
			return -ENOENT;
		int result = tg_read_file(msg, buf, size, offset);
		printf("read success = %i\n", result);
		return result;
	}
	return -ENOENT;
}

static int tgfs_create(const char *path, mode_t mode,
					struct fuse_file_info *fi) {
						
	(void) fi;
	return -ENOENT;
}

static int tgfs_write(const char *path, const char *buf, size_t size, off_t offset, 
			struct fuse_file_info *fi) {
	(void) fi;
	return -ENOENT;
}

int tgfs_release(const char *path, struct fuse_file_info *fi) {
	return 0;
}

int tgfs_truncate(const char *path, off_t newsize) {
	return 0;
}

static void* tgfs_init(struct fuse_conn_info *conn) {
	(void) conn;
	return NULL;
}

static int tgfs_unlink(const char* path) {
	return -ENOSYS;
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
	
	printf("%s\n", sqlite3_libversion()); 
	
	printf("Launching tgfs...\n");
	
	struct fuse_args args = FUSE_ARGS_INIT(argc, argv);
	fuse_opt_parse(&args, NULL, tgfs_opts, tgfs_opt_proc);
	fuse_opt_add_arg(&args, "-odirect_io");
	fuse_opt_add_arg(&args, "-ouse_ino");
	//fuse_opt_add_arg(&args, "-f");

	tg_tgl_init();

	// Block until tg login
	while(ready == 0) {
			usleep(1000);
	}
	printf("Ready!\n");
	int result = fuse_main(args.argc, args.argv, &tgfs_oper, NULL);
	tg_tgl_destruct();
	return result;
}
