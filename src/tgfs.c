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

#include <tgl/tgl.h>
#include <tgl/mtproto-key.h>
#include <tgl/tgl-binlog.h>
#include <tgl/tgl-net.h>
#include <tgl/tgl-timers.h>
#include <tgl/tgl-queries.h>

#  include <event2/event.h>
#  include <event2/bufferevent.h>


#define TGFS_APP_HASH "36722c72256a24c1225de00eb6a1ca74"
#define TGFS_APP_ID 2899

#define PACKAGE_VERSION "0.2"

struct tgl_state *TLS;
int ready = 0;
int peers_length;
tgl_peer_id_t *peers;

static int tgfs_getattr(const char *path, struct stat *stbuf)
{
	int res = 0;
	memset(stbuf, 0, sizeof(struct stat));
	if (strcmp(path, "/") == 0) {
		stbuf->st_mode = S_IFDIR | 0700;
		stbuf->st_nlink = 2 + peers_length;
		
	} else {
		res = -ENOENT;
	}
	return res;
}

static int tgfs_readdir(const char *path, void *buf, fuse_fill_dir_t filler,
			 off_t offset, struct fuse_file_info *fi)
{
	int i;
	printf("readdir: %s\n", path);
	if (strcmp(path, "/") == 0) {
		filler(buf, ".", NULL, 0);
		filler(buf, "..", NULL, 0);
		for(i = 0; i < peers_length; i++) {
			tgl_peer_t *peer = tgl_peer_get(TLS, peers[i]);
			printf("peer: %s\n", peer->print_name);
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
	return -ENOENT;
}

static int tgfs_read(const char *path, char *buf, size_t size, off_t offset,
		      struct fuse_file_info *fi)
{
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

extern struct tgl_update_callback upd_cb;

void read_auth_file();
void* tgfs_tgl_init(void* arg) {
	(void)arg;
	TLS = tgl_state_alloc();
	tgl_set_rsa_key(TLS, "tg-server.pub");
	tgl_set_download_directory(TLS, "~/.tgfs");
	tgl_set_callback(TLS, &upd_cb);

	struct event_base *ev = event_base_new ();
	tgl_set_ev_base (TLS, ev);
	tgl_set_net_methods (TLS, &tgl_conn_methods);
	tgl_set_timer_methods (TLS, &tgl_libevent_timers);
	assert (TLS->timer_methods);


	tgl_register_app_id (TLS, TGFS_APP_ID, TGFS_APP_HASH); 
  	tgl_set_app_version (TLS, "tgfs " PACKAGE_VERSION);

	int init = tgl_init(TLS);
	assert(init >= 0);

	// Simulate empty auth file
    	bl_do_dc_option (TLS, 0, 1, "", 0, TG_SERVER_1, strlen (TG_SERVER_1), 443);
    	bl_do_dc_option (TLS, 0, 2, "", 0, TG_SERVER_2, strlen (TG_SERVER_2), 443);
    	bl_do_dc_option (TLS, 0, 3, "", 0, TG_SERVER_3, strlen (TG_SERVER_3), 443);
    	bl_do_dc_option (TLS, 0, 4, "", 0, TG_SERVER_4, strlen (TG_SERVER_4), 443);
    	bl_do_dc_option (TLS, 0, 5, "", 0, TG_SERVER_5, strlen (TG_SERVER_5), 443);
    	bl_do_set_working_dc (TLS, TG_SERVER_DEFAULT);

	tgl_set_rsa_key_direct (TLS, tglmp_get_default_e (), tglmp_get_default_key_len (), tglmp_get_default_key ());

	read_auth_file();

  	tgl_login (TLS);

	while (1) {
    		event_base_loop (TLS->ev_base, EVLOOP_ONCE);
    		usleep(1000);
	}
	return NULL;
}

pthread_t t;

int main(int argc, char *argv[]) {
	
	printf("Launching tgfs...\n");
	
	struct fuse_args args = FUSE_ARGS_INIT(argc, argv);
	fuse_opt_parse(&args, NULL, tgfs_opts, tgfs_opt_proc);
	fuse_opt_add_arg(&args, "-odirect_io");
	fuse_opt_add_arg(&args, "-ouse_ino");

	pthread_create(&t, NULL, tgfs_tgl_init, NULL);	

	// Block until tg login
	while(ready == 0) {
			usleep(1000);
	}
	printf("Ready!\n");
	int result = fuse_main(args.argc, args.argv, &tgfs_oper, NULL);
	return result;
}
