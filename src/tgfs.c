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

static int tgfs_getattr(const char *path, struct stat *stbuf)
{
	if(strcmp(path, "/test") == 0) {
		stbuf->st_mode = S_IFREG | 0600;
		return 0;
	}
	int pos = 1;
	memset(stbuf, 0, sizeof(struct stat));
	if (strcmp(path, "/") == 0) {
		stbuf->st_mode = S_IFDIR | 0700;
		stbuf->st_nlink = 2 + peers_length;
		return 0;		
	}
	while(path[pos] != '/' && path[pos] != '\0') {
		pos++;
	}
	char* peer_name = (char*)malloc(pos*sizeof(char));
	strncpy(peer_name, path + 1, pos - 1);
	peer_name[pos-1] = '\0';
	tgl_peer_t *peer = tgl_peer_get_by_name(TLS, peer_name);
	free(peer_name);
	if(peer == NULL) {
		return -ENOENT;
	}
	if(path[pos] == '\0') {
		stbuf->st_mode = S_IFDIR | 0700;
		stbuf->st_nlink = 2;
		struct tgl_message *message = peer->last;
		if(message) {
			stbuf->st_mtime = message->date;
		}
		return 0;
	}
	return -ENOENT;

}

static int tgfs_readdir(const char *path, void *buf, fuse_fill_dir_t filler,
			 off_t offset, struct fuse_file_info *fi)
{
	int i;
	if (strcmp(path, "/") == 0) {
		filler(buf, ".", NULL, 0);
		filler(buf, "..", NULL, 0);
		filler(buf, "test", NULL, 0);
		for(i = 0; i < peers_length; i++) {
			tgl_peer_t *peer = tgl_peer_get(TLS, peers[i]);
			if(peer->print_name) {
				filler(buf, peer->print_name, NULL, 0);
			} else {
				// null peer
			}
		}
		return 0;
	}
	
	return -ENOENT;
}

static int tgfs_open(const char *path, struct fuse_file_info *fi)
{
	if(strcmp(path, "/test") == 0) {
		return 0;
	}
	return -ENOENT;
}

static void read_callback(struct tgl_state *TLS, void *callback_extra, int success) {
	struct tgl_read_data *data = (struct tgl_read_data*)callback_extra;
	data->success = success;
	printf("success: %i\n", success);
	if(success) {
		printf("len = %i\n", data->len);
	}
}	

static int tgfs_read(const char *path, char *buf, size_t size, off_t offset,
		      struct fuse_file_info *fi)
{
	if(strcmp(path, "/test") == 0) {
		struct tgl_read_data *data = (struct tgl_read_data*)malloc(sizeof(struct tgl_read_data));
		data->success = -1;
		data->offset = offset;
		data->len = size > 1 << 14 ? size : 1 << 14;
		tgl_peer_t *selfchat = tgl_peer_get(TLS, TLS->our_id);
		assert(selfchat);
		assert(selfchat->last);
		tgl_do_read_audio(TLS, selfchat->last->media.document, read_callback, data);
		while(data->success == -1) {
			//sleep(0.1);
		}
		if(data->success == 0) {
			return -EIO;
		}
		data->len = data->len > size ? size : data->len;
		printf("retured to read, len = %i, size = %li offset = %li\n", data->len, size, offset);
		memcpy(buf, data->bytes, data->len);
		size = data->len;
		free(data);
		return size;
		
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


	// Block until tg login
	while(ready == 0) {
			usleep(1000);
	}
	printf("Ready!\n");
	int result = 0;//fuse_main(args.argc, args.argv, &tgfs_oper, NULL);
	return result;
}
