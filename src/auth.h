#ifndef _AUTH_H_
#define _AUTH_H_

#include <tgl/tgl.h>
#include <tgl/tgl-binlog.h>
#include <tgl/mtproto-client.h>
#include <tgl/tgl-queries.h>
#include <tgl/tools.h>

#include <unistd.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <fcntl.h>
#include <pwd.h>

#define CONFIG_DIRECTORY ".tgfs"
#define AUTH_KEY_FILE "auth"

void read_auth_file (void);
void write_auth_file (void);

char *get_home_directory (void);

#endif
