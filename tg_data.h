#ifndef _TG_DATA_H_

#define _TG_DATA_H_

typedef struct {
	char* peer_name;
} tg_data_peer_t;

typedef struct {
	tg_data_peer_t* peers;
	size_t peers_count;
} tg_data_t;

tg_data_t tg_init();

#endif
