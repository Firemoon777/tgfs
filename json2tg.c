#define _XOPEN_SOURCE
#include <stdio.h>
#include <malloc.h>
#include <string.h>
#include <time.h>
#include <stdlib.h>
#include <assert.h>

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
		
		if(strncmp("admin", json + tokens[r].start, token_size) == 0) {
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
		if(strncmp(json + tokens[r].start, "flags", token_size) == 0) {
			char a[100];
			strncpy(a, json + tokens[r + 1].start, inner_size);
			a[inner_size] = 0;
			peer->flags = strtol(a, NULL, 10);
			i++;
			r += 2;
			continue; 
		}
		
		i++;
		r += 2;
	}
	*pos = r - 1;
}

static void json_parse_skip(char* json, jsmntok_t* tokens, size_t *r) {
	size_t size = tokens[(*r)+1].size;
	
	*r += 2;
	for(size_t i = 0; i < size; i++) {
		if(tokens[(*r)+1].size > 1) {
			json_parse_skip(json, tokens, r);
			continue;
		}
		*r += 2;
	}
}

static void json_parse_media(char* json, jsmntok_t* tokens, size_t pos, tg_msg_t* msg) {
	
	size_t i = 0, r = pos + 1;
	while(i < tokens[pos].size) {
		size_t token_size = tokens[r].end - tokens[r].start;
		size_t inner_size = tokens[r + 1].end - tokens[r + 1].start;
		
		if(strncmp(json + tokens[r].start, "caption", token_size) == 0 && inner_size > 0) {
			msg->caption = (char*)malloc((inner_size + 5) * sizeof(char));
			strncpy(msg->caption, json + tokens[r + 1].start, inner_size);
			msg->caption[inner_size] = 0;
			for(size_t k = 0; k < strlen(msg->caption); k++) {
				if(msg->caption[k] == '/')
					msg->caption[k] = '_';
			}
			msg->caption_hash = tg_string_hash(msg->caption);
			i++;
			r += 2;
			continue; 
		}
		if(strncmp(json + tokens[r].start, "size", token_size) == 0) {
			char a[100];
			strncpy(a, json + tokens[r + 1].start, inner_size);
			a[inner_size] = 0;
			msg->size = strtol(a, NULL, 10);
			i++;
			r += 2;
			continue; 
		}
		
		i++;
		r += 2;
	}
}

static void json_parse_msg(char* json, jsmntok_t* tokens, size_t* pos, tg_msg_t* msg, int media_type) {
	size_t r = *pos + 1, i = 0;
			
	while(i < tokens[*pos].size) {	
		size_t token_size = tokens[r].end - tokens[r].start;
		size_t inner_size = tokens[r+1].end - tokens[r+1].start;
		
		if(strncmp("media", json + tokens[r].start, token_size) == 0) {
			json_parse_media(json, tokens, r + 1, msg);
			r += 2*tokens[r+1].size + 2;
			i++;
			continue;
		}
		
		if(strncmp("id", json + tokens[r].start, token_size) == 0) {
			strncpy(msg->id, json + tokens[r+1].start, inner_size);
			msg->id[inner_size] = 0;
			i++;
			r += 2;
			continue;
		}
		if(strncmp("date", json + tokens[r].start, token_size) == 0) {
			char* buff = (char*)malloc((inner_size + 1) * sizeof(char));
			strncpy(buff, json + tokens[r+1].start, inner_size);
			buff[inner_size] = 0;
			msg->timestamp = strtol(buff, NULL, 10);
			free(buff);
			i++;
			r += 2;
			continue;
		}
		
		if(tokens[r+1].size > 1) {
			json_parse_skip(json, tokens, &r);
			i++;
			continue;
		}
		
		i++;
		r += 2;
	}
	*pos = r - 1;
	
	char ext[5];
	switch(media_type) {
		case TG_MEDIA_VOICE:
			strcpy(ext, ".ogg");
			break;
		case TG_MEDIA_PHOTO:
			strcpy(ext, ".jpg");
			break;
		default:
			strcpy(ext, "");
			break;
	}
	if(msg->caption) {
		strcat(msg->caption, ext);
	} else {
		assert(media_type > 0);		
		msg->caption = (char*)malloc(100*sizeof(char));
		char time[20];
		strftime(time, sizeof(time), "%Y-%m-%d %H:%M:%S", localtime(&msg->timestamp));
		sprintf(msg->caption, "%s - %u%s", time, (unsigned)tg_string_hash(msg->id), ext);
	}
	msg->caption_hash = tg_string_hash(msg->caption);
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

int json_parse_messages(char* json, size_t size, tg_peer_t* peer, int media_type) {
	int parsed = 0;
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
	size_t message_count = 0;
	tg_msg_t* messages;
	tg_get_msg_array_by_media_type(&messages, &message_count, peer, media_type);

	for(size_t i = 1; i < tokens_count; i++) {
		tg_msg_t* msg = tg_msg_init();
		json_parse_msg(json, tokens, &i, msg, media_type);
		if(msg->timestamp > peer->cached_time[media_type]) {
			tg_msg_add_front(&messages, msg);
			parsed++;
		}
	}
	free(tokens);
	tg_set_msg_array_by_media_type(messages, message_count + parsed, peer, media_type);
	return parsed;
}

int json_parse_filelink(char* link, char* json) {
	printf("Answer: %s\n", json);
	jsmn_parser parser;
	jsmntok_t *tokens;
	jsmn_init(&parser);
	
	size_t tokens_count = jsmn_parse(&parser, json, strlen(json), NULL, 0);
	
	jsmn_init(&parser);
	tokens = (jsmntok_t*)malloc(tokens_count*sizeof(jsmntok_t));
	jsmn_parse(&parser, json, strlen(json), tokens, tokens_count);
	
	for(size_t i = 1; i < tokens_count; i += 2) {
		if(strncmp("result", json + tokens[i].start, tokens[i].end - tokens[i].start) == 0) {
			size_t len = tokens[i+1].end - tokens[i+1].start;
			strncpy(link, json + tokens[i+1].start, len);
			link[len] = 0;
			return 0;
		}
	}
	return 1;
}
