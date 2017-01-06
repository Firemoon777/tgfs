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

void tg_print_peer_t(tg_peer_t* peer) {
	fprintf(stderr, "Peer:\n");
	fprintf(stderr, "\tid: '%s'\n", peer->id);
	fprintf(stderr, "\tpeer_type: '%s'\n", peer->peer_type);
	fprintf(stderr, "\tpeer_id: '%s'\n", peer->peer_id);
	fprintf(stderr, "\tprint_name: '%s'\n", peer->print_name);
	fprintf(stderr, "\tlast_seen: %li\n", peer->last_seen);
	fprintf(stderr, "\tmsg: %li\n", peer->msg);
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
		peer->msg = r;
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

void tg_reload_peer_date() {
	
}

/*

tg_file_t* tg_get_msg_photo(tg_data_peer_t* peer) {
	int fd = socket_init(SOCKET_NAME);
	tg_file_t* files = NULL;
	char* str = (char*)malloc(256 * sizeof(char));
	char* string = (char*)malloc(255*sizeof(char));
	sprintf(str, "search %s 1\n", peer->peer_name);
	socket_send_string(fd, str, strlen(str));
	
	size_t size = socket_read_answer_size(fd);
	
	char* json = (char*)malloc(size*sizeof(char));
	socket_read_answer(fd, json, size);
	
	jsmn_parser parser;
	jsmntok_t *tokens;
	jsmn_init(&parser);
	
	size_t tokens_count = jsmn_parse(&parser, json, size, NULL, 0);
	printf("Msg token count: %li\n", tokens_count);
	jsmn_init(&parser);
	tokens = (jsmntok_t*)malloc(tokens_count*sizeof(jsmntok_t));
	jsmn_parse(&parser, json, size, tokens, 10000);
	
	files = (tg_file_t*)malloc(tokens[0].size * sizeof(tg_file_t));
	
	for(int i = 1; i < tokens_count; i++) {
		if(tokens[i].type != JSMN_STRING)
			continue;
		len = tokens[i].end - tokens[i].start;
		strncpy(string, json + tokens[i].start, len);
		string[len] = 0;
		if(strcmp(string, "id") == 0) {
			i++;
			len = tokens[i].end - tokens[i].start;
			strncpy(string, json + tokens[i].start, len);
			string[len] = 0;
			if(len != 0 && string[0] != '$') {
				peers[peers_size].peer_name = (char*)malloc(len*sizeof(char));
				strcpy(peers[peers_size].peer_name, string);
				files_size++;
			}
			
		}
	}
	
	close(fd);
	return files;
}

tg_file_t* tg_get_msg_photo(tg_data_peer_t* peer) {
	return NULL;
}
*/
