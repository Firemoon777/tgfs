#include <stdio.h>
#include <malloc.h>
#include <stdlib.h>
#include <string.h>

#include "tg_data.h"
#include "string_list.h"
#include "socket_tasks.h"
#include "jsmn/jsmn.h"

#define SOCKET_NAME "tg_socket"

tg_data_t tg_init() {
	char* string = (char*)malloc(255*sizeof(char));
	int fd = socket_init(SOCKET_NAME);
	tg_data_t result;
	size_t peers_size = 0;
	
	socket_send_string(fd, "dialog_list\n", 12);
	size_t size = socket_read_answer_size(fd);
	
	char* json = (char*)malloc(size*sizeof(char));
	socket_read_answer(fd, json, size);
	jsmn_parser parser;
	jsmntok_t *tokens;
	jsmn_init(&parser);
	size_t tokens_count = jsmn_parse(&parser, json, size, NULL, 0);
	printf("Tokens count: %li\n", tokens_count);
	jsmn_init(&parser);
	tokens = (jsmntok_t*)malloc(tokens_count*sizeof(jsmntok_t));
	jsmn_parse(&parser, json, size, tokens, 10000);
	
	tg_data_peer_t* peers = (tg_data_peer_t*)malloc(tokens[0].size * sizeof(tg_data_peer_t));
	peers_size = 0;
	size_t len;
	for(int i = 1; i < tokens_count; i++) {
		if(tokens[i].type != JSMN_STRING)
			continue;
		len = tokens[i].end - tokens[i].start;
		strncpy(string, json + tokens[i].start, len);
		string[len] = 0;
		if(strcmp(string, "print_name") == 0) {
			i++;
			len = tokens[i].end - tokens[i].start;
			strncpy(string, json + tokens[i].start, len);
			string[len] = 0;
			if(len != 0) {
				//printf("Found: %s\n", string);
				peers[peers_size].peer_name = (char*)malloc(len*sizeof(char));
				strcpy(peers[peers_size].peer_name, string);
				peers_size++;
			}
			
		}
	}
	result.peers_count = peers_size;
	result.peers = peers;
	return result;
}
