#define FUSE_USE_VERSION 26

#include <fuse.h>
#include <iostream>
#include <errno.h>

static int tgfs_getattr(const char *path, struct stat *stbuf) {
	return -ENOENT;
}

static int tgfs_readdir(const char *path, void *buf, fuse_fill_dir_t filler, off_t offset, struct fuse_file_info *fi) {
	return -ENOENT;
}

static int tgfs_open(const char *path, struct fuse_file_info *fi) {
	return -ENOENT;
}

static int tgfs_read(const char *path, char *buf, size_t size, off_t offset, struct fuse_file_info *fi) {
	return -ENOENT;
}

static const struct fuse_operations tgfs_oper = {
	.getattr	= tgfs_getattr,
	.readdir	= tgfs_readdir,
	.open		= tgfs_open,
	.read		= tgfs_read,
};

int main(int argc, char* argv[]) {
	int fuse_result = fuse_main(argc, argv, &tgfs_oper, NULL);
	return fuse_result;
}
