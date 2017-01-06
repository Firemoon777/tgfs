#define _XOPEN_SOURCE
#include <stdio.h>
#include <malloc.h>
#include <string.h>
#include <time.h>

#include "json2tg.h"
#include "jsmn/jsmn.h"

static time_t json_datetime_to_posix(char* json) {
	time_t result = 0;
	char time[20];
	strncpy(time, json, 19);
	time[19] = 0;
	struct tm *tm = (struct tm*)malloc(sizeof(struct tm));
	if(strptime(time, "%Y-%m-%d %H:%M:%S", tm)) {
		result = mktime(tm);
	}
	free(tm);
	return result;
}

static void json_parse_peer(char* json, jsmntok_t* tokens, size_t* pos, tg_peer_t* peer) {	
	for(size_t i = *pos; i < (*pos) + 2*tokens[*pos].size + 1; i++) {
		size_t len = tokens[i].end - tokens[i].start;
		char* start = json + tokens[i].start;
		if(len == 0)
			continue;
		if(strncmp(start, "id", len) == 0) {
			if(strlen(peer->id) == 0) {
				i++;
				strncpy(peer->id, json + tokens[i].start, tokens[i].end - tokens[i].start);
			}
		} else if(strncmp(start, "print_name", len) == 0) {
			i++;
			size_t length = tokens[i].end - tokens[i].start;
			peer->print_name = (char*)malloc(length*sizeof(char));
			strncpy(peer->print_name, json + tokens[i].start, length);
			peer->print_name[length] = 0;
		} else if(strncmp(start, "when", len) == 0) {
			i++;
			peer->last_seen = json_datetime_to_posix(json + tokens[i].start);
		}
	}
	*pos += 2*tokens[*pos].size;
}

int json_parse_dialog_list(char* json, size_t size, tg_peer_t** peers, size_t* peers_count) {
	jsmn_parser parser;
	jsmntok_t *tokens;
	jsmn_init(&parser);
	
	size_t tokens_count = jsmn_parse(&parser, json, size, NULL, 0);

	jsmn_init(&parser);
	tokens = (jsmntok_t*)malloc(tokens_count*sizeof(jsmntok_t));
	jsmn_parse(&parser, json, size, tokens, tokens_count);
#ifdef DEBUG
	printf("json_parse_dialog_list(): Dialog count count: %i\n", tokens[0].size);
#endif
	
	*peers = (tg_peer_t*)malloc(tokens[0].size * sizeof(tg_peer_t));
	size_t array_end = 0;
	for(size_t i = 1; i < tokens_count; i++) {
		if(tokens[i].start > array_end) {
			array_end = tokens[i].end;
			json_parse_peer(json, tokens, &i, &(*peers)[*peers_count]);
			(*peers_count)++;
		}
	}
	return 0;
}
