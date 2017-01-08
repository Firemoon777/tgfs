#ifndef _TG_DATA_H_

#define _TG_DATA_H_

#include <time.h>
#include <stdint.h>

typedef enum {
	TG_MEDIA_PHOTO = 1,
	TG_MEDIA_AUDIO_DOCUMENT = 7
} tg_media_type;

typedef struct {
	char id[50];
	char* caption;
	time_t timestamp;
	size_t size;
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
	
	tg_msg_t* messages;
	size_t message_count;
} tg_peer_t;

typedef struct {
	tg_peer_t* peers;
	size_t peers_count;
} tg_data_t;

#ifdef DEBUG

void tg_print_peer_t(tg_peer_t* peer);
void tg_print_msg_t(tg_msg_t* msg);

#endif

int tg_init();
tg_peer_t* tg_find_peer_by_name(const char* name, size_t len);
int tg_search_msg(tg_peer_t* peer, int type, char* request);
void tg_peer_search_msg_count(tg_peer_t* peer);

uint32_t tg_string_hash(const char* str);

#endif
