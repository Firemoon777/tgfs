#define _XOPEN_SOURCE
#include <stdio.h>
#include <malloc.h>
#include <string.h>
#include <time.h>
#include <stdlib.h>

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

static void json_parse_msg(char* json, jsmntok_t* tokens, size_t* pos, tg_msg_t* msg) {
	size_t bounds = 2*tokens[*pos].size + 1;
	msg->caption = NULL;
	for(size_t i = *pos + 1; tokens[i].end < tokens[*pos].end; i++) {
		size_t len = tokens[i].end - tokens[i].start;
		char* start = json + tokens[i].start;
		if(len == 0)
			continue;
		if(strncmp(start, "from", len) == 0 || strncmp(start, "to", len) == 0) {
			//printf("FROM OR TO\n");
			bounds += 2*tokens[i].size;
			i += 2*tokens[i].size;
			continue;
		}
		if(strncmp(start, "id", len) == 0) {
			if(strlen(msg->id) == 0) {
				i++;
				size_t length = tokens[i].end - tokens[i].start;
				strncpy(msg->id, json + tokens[i].start, length);
				msg->id[length] = 0;
			}
		} else if(strncmp(start, "caption", len) == 0) {
			//printf("Caption!\n");
			i++;
			size_t length = tokens[i].end - tokens[i].start;
			msg->caption = (char*)malloc((length+1)*sizeof(char));
			strncpy(msg->caption, json + tokens[i].start, length);
			msg->caption[length] = 0;
		} else if(strncmp(start, "date", len) == 0) {
			i++;
			size_t length = tokens[i].end - tokens[i].start;
			char* time = (char*)malloc((length+1) * sizeof(char));
			strncpy(time, json + tokens[i].start, length);
			time[length] = 0;
			//printf("Time: %s\n", time);
			msg->timestamp = strtol(time, NULL, 10);
		}
	}
	//tg_print_msg_t(msg);
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
	printf("json_parse_dialog_list(): Dialog count: %i\n", tokens[0].size);
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

int json_parse_messages(char* json, size_t size, tg_peer_t* peer) {
	jsmn_parser parser;
	jsmntok_t *tokens;
	jsmn_init(&parser);
	
	size_t tokens_count = jsmn_parse(&parser, json, size, NULL, 0);

	jsmn_init(&parser);
	tokens = (jsmntok_t*)malloc(tokens_count*sizeof(jsmntok_t));
	jsmn_parse(&parser, json, size, tokens, tokens_count);
#ifdef DEBUG
	printf("json_parse_messages(): Messages count: %i\n", tokens[0].size);
#endif
	peer->message_count = 0;
	peer->messages	= (tg_msg_t*)malloc(tokens_count * sizeof(tg_msg_t));
	size_t array_end = 0;
	for(size_t i = 1; i < tokens_count; i++) {
		if(tokens[i].start > array_end) {
			array_end = tokens[i].end;
			json_parse_msg(json, tokens, &i, &peer->messages[peer->message_count]);
			peer->message_count++;
		}
	}
	return 0;
}
