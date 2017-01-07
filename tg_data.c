#define _DEFAULT_SOURCE
#include <stdio.h>
#include <malloc.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>

#include "tg_data.h"
#include "json2tg.h"
#include "socket_tasks.h"
#include "jsmn/jsmn.h"

tg_data_t tg;

/* djb2 */
size_t tg_string_hash(char* str) {
	size_t hash = 5381;
	int c;

	while ((c = *str++) == 0)
		hash = ((hash << 5) + hash) + c; /* hash * 33 + c */

	return hash;
}

int tg_init() {
	char* json;
	size_t len;
	char* request = "dialog_list\n";
	socket_send_string(request, strlen(request));
	socket_read_data(&json, &len);
	if(json_parse_dialog_list(json, len, &tg.peers, &tg.peers_count)) 
		return 1;
	free(json);
	/*for(size_t i = 0; i < tg.peers_count; i++) {
		tg_peer_search_msg_count(&tg.peers[i]); 
	}*/
	return 0;
}

#ifdef DEBUG

void tg_print_msg_t(tg_msg_t* msg) {
	fprintf(stderr, "Msg:\n");
	fprintf(stderr, "\tid: '%s'\n", msg->id);
	fprintf(stderr, "\tcaption: '%s'\n", msg->caption);
	fprintf(stderr, "\ttimestamp: %li\n", msg->timestamp);
}

void tg_print_peer_t(tg_peer_t* peer) {
	fprintf(stderr, "Peer:\n");
	fprintf(stderr, "\tid: '%s'\n", peer->id);
	switch(peer->peer_type) {
		case TG_USER:
			fprintf(stderr, "\tpeer_type: user\n");
			break;
		case TG_CHAT:
			fprintf(stderr, "\tpeer_type: chat\n");
			break;
		case TG_CHANNEL:
			fprintf(stderr, "\tpeer_type: channel\n");
			break;
		default:
			fprintf(stderr, "\tpeer_type: unknown\n");
			break;
	}
	
	fprintf(stderr, "\tpeer_id: %lu\n", peer->peer_id);
	fprintf(stderr, "\tprint_name: '%s'\n", peer->print_name);
	fprintf(stderr, "\tlast_seen: %li\n", peer->last_seen);
	fprintf(stderr, "\ttotal_message_count: %li\n", peer->total_message_count);
}

#endif

void tg_peer_search_msg_count(tg_peer_t* peer) {
	if(strlen(peer->print_name) > 0) {
		printf("Try to count peer %s\n", peer->print_name);
		size_t l = 0, r = 130000;
		size_t middle;
		char* str = (char*)malloc(255*sizeof(char));
		char* data;
		size_t size = 0;
		while(l + 1 < r) {
			middle = (l + r) / 2;
			sprintf(str, "history %s 1 %li\n", peer->print_name, middle);
			printf("debug: %s", str);
			socket_send_string(str, strlen(str));
			socket_read_data(&data, &size); 
			printf("(%li < %li) -> %li\n", l, r, size);
			fflush(stdout);
			if(size == 3) { 
				r = middle;
			} else {
				l = middle;
			}
			free(data);
			//usleep(400000);
		}
		free(str);
		printf("Answer: %li\n", r);
		peer->total_message_count = r;
	}
}

tg_peer_t* tg_find_peer_by_name(const char* name, size_t len) {
	for(size_t i = 0; i < tg.peers_count; i++) {
		if(strncmp(name, tg.peers[i].print_name, len) == 0) {
			return tg.peers + i;
		}
	}
	return NULL;
}

int tg_get_msg_photo(tg_peer_t* peer) {
	char* json;
	size_t len;
	int result;
	char* str = (char*)malloc(256 * sizeof(char));
	
	sprintf(str, "search %s 1\n", peer->print_name);
	socket_send_string(str, strlen(str));
	socket_read_data(&json, &len);
	printf("Answer: %s\n", json);
	
	result = json_parse_messages(json, len, peer);
	
	free(str);
	free(json);
	return result;
}

void tg_reload_peer_date() {
	
}
