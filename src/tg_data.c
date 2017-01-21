#define _DEFAULT_SOURCE
#include <stdio.h>
#include <malloc.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <assert.h>
#include <pthread.h>

#include "tg_data.h"
#include "json2tg.h"
#include "socket_tasks.h"
#include "jsmn/jsmn.h"

tg_data_t tg;
pthread_mutex_t lock;

/* djb2 */
uint32_t tg_string_hash(const char* str) {
	uint32_t hash = 5381;
	int c;

	while ((c = *str++) != 0)
		hash = ((hash << 5) + hash) + c; /* hash * 33 + c */

	return hash;
}

int tg_init() {
	if (pthread_mutex_init(&lock, NULL) != 0) {
        printf("\n mutex init failed\n");
        exit(1);
    }
    tg.mount_time = time(NULL);
	char* json;
	size_t len;
	char* request = "dialog_list\n";
	
	pthread_mutex_lock(&lock);
	socket_send_string(request, strlen(request));
	socket_read_data(&json, &len);
	pthread_mutex_unlock(&lock);
	
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
	fprintf(stderr, "\tcaption_hash: '%u'\n", msg->caption_hash);
	fprintf(stderr, "\tsize: '%li'\n", msg->size);
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
	fprintf(stderr, "\tflags: %u\n", peer->flags);
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

tg_peer_t* tg_find_peer_by_name(const char* name, const size_t len) {
	char* buff = (char*)malloc((len + 1) * sizeof(char));
	strncpy(buff, name, len);
	buff[len] = 0;
	uint32_t hash = tg_string_hash(buff);
	
	for(size_t i = 0; i < tg.peers_count; i++) {
		if(hash == tg.peers[i].print_name_hash) {
			if(strncmp(buff, tg.peers[i].print_name, len) == 0) {
				free(buff);
				return tg.peers + i;
			}
		}
	}
	free(buff);
	return NULL;
}

int tg_get_media_type_by_string(const char* msg) {
	if(strncmp(msg, "Photo", 5) == 0) {
		return TG_MEDIA_PHOTO;
	} else if(strncmp(msg, "Audio", 5) == 0) {
		return TG_MEDIA_AUDIO;
	} else if(strncmp(msg, "Voice", 5) == 0) {
		return TG_MEDIA_VOICE;
	} else if(strncmp(msg, "Document", 8) == 0) {
		return TG_MEDIA_DOCUMENT;
	} else if(strncmp(msg, "Video", 5) == 0) {
		return TG_MEDIA_VIDEO;
	} else if(strncmp(msg, "Gif", 3) == 0) {
		return TG_MEDIA_GIF;
	} 
	return -1;
}

int tg_get_msg_array_by_media_type(tg_msg_t** msg, size_t* size, const tg_peer_t* peer, const int media_type) {
	switch(media_type) {
		case TG_MEDIA_AUDIO:
			*size = peer->audio_size;
			*msg = peer->audio;
			break;
		case TG_MEDIA_PHOTO:
			*size = peer->photo_size;
			*msg = peer->photo;
			break;
		case TG_MEDIA_VIDEO:
			*size = peer->video_size;
			*msg = peer->video;
			break;
		case TG_MEDIA_DOCUMENT:
			*size = peer->documents_size;
			*msg = peer->documents;
			break;
		case TG_MEDIA_VOICE:
			*size = peer->voice_size;
			*msg = peer->voice;
			break;
		case TG_MEDIA_GIF:
			*size = peer->gif_size;
			*msg = peer->gif;
			break;
		default:
			return 1;
	}
	return 0;
}

int tg_set_msg_array_by_media_type(tg_msg_t* msg, const size_t size, tg_peer_t* peer, const int media_type) {
	switch(media_type) {
		case TG_MEDIA_AUDIO:
			peer->audio_size = size;
			peer->audio = msg;
			break;
		case TG_MEDIA_PHOTO:
			peer->photo_size = size;
			peer->photo = msg;
			break;
		case TG_MEDIA_VIDEO:
			peer->video_size = size;
			peer->video = msg;
			break;
		case TG_MEDIA_DOCUMENT:
			peer->documents_size = size;
			peer->documents = msg;
			break;
		case TG_MEDIA_VOICE:
			peer->voice_size = size;
			peer->voice = msg;
			break;
		case TG_MEDIA_GIF:
			peer->gif_size = size;
			peer->gif = msg;
			break;
		default:
			return 1;
	}
	return 0;
}

tg_msg_t* tg_find_peer_msg_by_caption(const tg_peer_t* peer, const char* caption, const int media_type) {
	uint32_t hash = tg_string_hash(caption);
	size_t n;
	tg_msg_t* msg;
	tg_get_msg_array_by_media_type(&msg, &n, peer, media_type);
	for(size_t i = 0; i < n; i++) {
		if(msg[i].caption_hash == hash) {
			if(strcmp(msg[i].caption, caption) == 0) {
				return msg + i;
			}
		}
	}
	return NULL;
}

void tg_msg_free(tg_msg_t* msg) {
	tg_msg_t* temp;
	while(msg) {
		temp = msg->next;
		free(msg->caption);
		free(msg);
		msg = temp;
	}
}

tg_msg_t* tg_msg_init() {
	tg_msg_t* result = (tg_msg_t*)malloc(sizeof(tg_msg_t));
	if(result) {
		result->caption = NULL;
		result->timestamp = 0;
		result->size = 0;
		result->next = NULL;
	}
	return result;
}

void tg_msg_add_front(tg_msg_t** head, tg_msg_t* item) {
	if(*head) {
		item->next = *head; 
	}
	*head = item;
}



int tg_search_msg(tg_peer_t* peer, const int media_type, const char* request) {
	size_t size;
	tg_msg_t* msg;
	tg_get_msg_array_by_media_type(&msg, &size, peer, media_type);
	
	char* json;
	size_t len;
	int result;
	char* str = (char*)malloc(256 * sizeof(char));
	
	int count = 100;
	int offset = 0;
	result = 2;
	while(result > 1) {
		sprintf(str, "search %s %i %i 0 0 %i %s\n", peer->print_name, media_type, count, offset, request);
		
		pthread_mutex_lock(&lock);
		socket_send_string(str, strlen(str));
		socket_read_data(&json, &len);
		pthread_mutex_unlock(&lock);
		
		assert(json);
		result = json_parse_messages(json, len, peer, media_type);
#ifdef DEBUG
		printf("tg_search_msg(%s, %i) = %i\n", peer->print_name, offset, result);
#endif
		offset += count;
		free(json);
		json = NULL;
	}
	peer->cached_time[media_type] = time(NULL) - 3 * 60;
	free(str);
	return result;
}

int tg_download_file(char* download, const tg_peer_t* peer, const char* filename, const int media_type) {
	tg_msg_t* msg;
	size_t hash;
	tg_get_msg_array_by_media_type(&msg, &hash, peer, media_type);
	hash = tg_string_hash(filename);
	char* file_id = NULL;
	while(msg) {
		if(hash == msg->caption_hash) {
			if(strcmp(filename, msg->caption) == 0) {
				file_id = msg->id;
				break;
			}
		}
		msg = msg->next;
	}
	if(file_id == NULL) {
		return 1;
	}
	printf("Fileid: %s\n", file_id);
	char request[255];
	sprintf(request, "load_file %s\n", file_id);

	pthread_mutex_lock(&lock);
	socket_send_string(request, strlen(request));
	char* json;
	size_t len;
	socket_read_data(&json, &len);
	pthread_mutex_unlock(&lock);
	
	return json_parse_filelink(download, json);
}

tg_fd* tg_init_fd() {
	tg_fd* result = (tg_fd*)malloc(sizeof(tg_fd));
	result->path = NULL;
	result->path_hash = 0;
	result->next = NULL;
	result->fd = -1;
	result->file_type = TG_FILE_UNKNOWN;
	return result;
}

void tg_add_fd(tg_fd** head, tg_fd* item) {
	assert(item->file_type != TG_FILE_UNKNOWN);
	if(head) {
		item->next = *head;
		*head = item;
	} else {
		*head = item;
	}
}

void tg_free_fd(tg_fd* item) {
	free(item->real_path);
	free(item->path);
	free(item);
}

void tg_remove_fd(tg_fd **head, tg_fd* item) {
	if(item == NULL) {
		return;
	}
	if(*head == item) {
		*head = item->next;
		free(item);
	} else {
		tg_fd* t = *head;
		while(t->next) {
			if(t->next == item) {
				t->next = t->next->next;
				free(t->next);
				return;
			}
			t = t->next;
		}
	}
}

tg_fd* tg_search_fd(tg_fd* head, const char* path) {
	uint32_t hash = tg_string_hash(path);
	while(head) {
		if(head->path_hash == hash) {
			if(strcmp(path, head->path) == 0) {
				return head;
			}
		}
		head = head->next;
	}
	return NULL;
}
