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
	size_t r = *pos + 1, i = 0;
	
	while(i < tokens[*pos].size) {	
		size_t token_size = tokens[r].end - tokens[r].start;
		size_t inner_size = tokens[r+1].end - tokens[r+1].start;
		
		if(strncmp("admin", json + tokens[r].start, tokens[r].end - tokens[r].start) == 0) {
			r += 2*tokens[r+1].size + 2;
			i++;
			continue;
		}
		if(strncmp("peer_type", json + tokens[r].start, token_size) == 0) {
			switch(json[tokens[r+1].start + 3]) {
				case 'r':
					peer->peer_type	= TG_USER;
					break;
				case 't':
					peer->peer_type	= TG_CHAT;
					break;
				case 'n':
					peer->peer_type	= TG_CHANNEL;
					break;
				default:
					peer->peer_type	= TG_UNKNOWN;
					break;
				
			} 
			i++;
			r += 2;
			continue;
		}
		if(strncmp("print_name", json + tokens[r].start, token_size) == 0) {
			peer->print_name = (char*)malloc((inner_size + 1) * sizeof(char));
			strncpy(peer->print_name, json + tokens[r+1].start, inner_size);
			peer->print_name[inner_size] = 0;
			peer->print_name_hash = tg_string_hash(peer->print_name);
			i++;
			r += 2;
			continue;
		}
		if(strncmp("when", json + tokens[r].start, token_size) == 0) {
			peer->last_seen = json_datetime_to_posix(json + tokens[r+1].start);
			i++;
			r += 2;
			continue;
		}
		if(strncmp("id", json + tokens[r].start, token_size) == 0) {
			strncpy(peer->id, json + tokens[r+1].start, inner_size);
			i++;
			r += 2;
			continue;
		}
		if(strncmp("peer_id", json + tokens[r].start, token_size) == 0) {
			peer->peer_id = strtol(json  + tokens[r+1].start, NULL, 10);
			i++;
			r += 2;
			continue;
		}
		
		
		i++;
		r += 2;
	}
	*pos = r - 1;
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
	
	if(*peers == NULL) {
		*peers = (tg_peer_t*)malloc(tokens[0].size * sizeof(tg_peer_t));
	} else if(tokens[0].size != *peers_count) {
		*peers = (tg_peer_t*)realloc(*peers, tokens[0].size * sizeof(tg_peer_t));
	}
	
	for(size_t i = 1; i < tokens_count; i++) {
		json_parse_peer(json, tokens, &i, &(*peers)[*peers_count]);
		(*peers_count)++;
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
