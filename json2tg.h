#include "tg_data.h"

#ifndef _JSON2TG_TASKS_H_ 

#define _JSON2TG_TASKS_H_ 

int json_parse_dialog_list(const char* json, const size_t size, tg_peer_t** peers, size_t* peers_count);
int json_parse_messages(const char* json, const size_t size, tg_peer_t* peer, const int media_type);
int json_parse_filelink(char* link, const char* json);

#endif
