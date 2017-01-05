#include "tg_data.h"

#ifndef _JSON2TG_TASKS_H_ 

#define _JSON2TG_TASKS_H_ 

int json_parse_dialog_list(char* json, size_t size, tg_peer_t** peers, size_t* peers_count);

#endif
