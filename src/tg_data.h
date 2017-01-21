#ifndef _TG_DATA_H_

#define _TG_DATA_H_

#include <time.h>
#include <stdint.h>

typedef enum {
	TG_MEDIA_PHOTO = 1,
	TG_MEDIA_VIDEO = 2,
	TG_MEDIA_DOCUMENT = 5,
	TG_MEDIA_VOICE = 6,
	TG_MEDIA_AUDIO = 7,
	TG_MEDIA_GIF = 9
} tg_media_type;

typedef enum {
	TG_FILE_UNKNOWN = 0,
	TG_FILE_UPLOAD = 1,
	TG_FILE_DOWNLOAD = 2
} tg_file_type_t;

typedef struct tg_msg_t {
	char id[50];
	char* caption;
	uint32_t caption_hash;
	
	time_t timestamp;
	size_t size;
	
	struct tg_msg_t* next;
} tg_msg_t;

typedef enum {
	TG_UNKNOWN = -1,
	TG_USER = 0,
	TG_CHAT = 1,
	TG_CHANNEL = 2
} tg_peer_type_t;

typedef struct {
	char id[33];
	tg_peer_type_t peer_type;
	uint64_t peer_id;
	
	char* print_name;
	uint32_t print_name_hash;
	
	time_t last_seen;
	size_t total_message_count;
	
	tg_msg_t* photo, 
			  *video, 
			  *documents, 
			  *audio, 
			  *voice,
			  *gif;
	size_t photo_size, 
		   video_size, 
		   documents_size, 
		   audio_size, 
		   voice_size,
		   gif_size;

	uint32_t flags;
	
	time_t cached_time[10];
} tg_peer_t;

typedef struct tg_fd {
	int fd;
	char* path;
	uint32_t path_hash;
	
	tg_file_type_t file_type;
	
	char* real_path;
	
	struct tg_fd* next;
} tg_fd;

typedef struct {
     int enable_unlink;
} tg_config;

typedef struct {
	tg_peer_t* peers;
	size_t peers_count;
	tg_config config;
	time_t mount_time;
} tg_data_t;

#ifdef DEBUG

void tg_print_peer_t(tg_peer_t* peer);
void tg_print_msg_t(tg_msg_t* msg);

#endif

void tg_msg_free(tg_msg_t* msg);
tg_msg_t* tg_msg_init();
void tg_msg_add_front(tg_msg_t** head, tg_msg_t* item);

int tg_init();
tg_peer_t* tg_find_peer_by_name(const char* name, const size_t len);
int tg_search_msg(tg_peer_t* peer, const int media_type, const char* request);
void tg_peer_search_msg_count(tg_peer_t* peer);
tg_msg_t* tg_find_peer_msg_by_caption(const tg_peer_t* peer, const char* caption, const int media_type);
int tg_get_msg_array_by_media_type(tg_msg_t** msg, size_t* size, const tg_peer_t* peer, const int media_type);
int tg_set_msg_array_by_media_type(tg_msg_t* msg, const size_t size, tg_peer_t* peer, const int media_type);
int tg_download_file(char* download, const tg_peer_t* peer, const char* filename, const int media_type);
int tg_get_media_type_by_string(const char* msg);

uint32_t tg_string_hash(const char* str);

tg_fd* tg_init_fd();
void tg_add_fd(tg_fd** head, tg_fd* item);
void tg_free_fd(tg_fd* item);
void tg_remove_fd(tg_fd **head, tg_fd* item);
tg_fd* tg_search_fd(tg_fd* head, const char* path);

#endif
