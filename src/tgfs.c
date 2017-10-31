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


#define PACKAGE_VERSION "0.1"

#define EQ_STR_LIT( x, lit ) (strncmp( (x), (lit), sizeof(lit)) == 0 )
#define TGFS_OPT(t, p, v) { t, offsetof(struct tgfs_config, p), v }

#define TGLUF_SELF (1 << 19)
#define TGLUF_HAS_PHOTO (1 << 1)

#define PATH_MAX_LEVEL 10


static int tgfs_getattr(const char *path, struct stat *stbuf)
{
	return -ENOENT;
}

static int tgfs_readdir(const char *path, void *buf, fuse_fill_dir_t filler,
			 off_t offset, struct fuse_file_info *fi)
{
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

int main(int argc, char *argv[]) {
	
	struct fuse_args args = FUSE_ARGS_INIT(argc, argv);


    fuse_opt_parse(&args, NULL, tgfs_opts, tgfs_opt_proc);
    fuse_opt_add_arg(&args, "-odirect_io");
    fuse_opt_add_arg(&args, "-ouse_ino");
#ifdef DEBUG
	fuse_opt_add_arg(&args, "-f");
	/*fuse_opt_add_arg(&args, "-d");*/
#endif
    
	int result = fuse_main(args.argc, args.argv, &tgfs_oper, NULL);
	return result;
}
