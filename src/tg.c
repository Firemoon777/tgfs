#define _DEFAULT_SOURCE

#include "tg.h"
#include "auth.h"

#include <unistd.h>
#include <pthread.h>
#include <sqlite3.h>
#include <event2/event.h>
#include <event2/bufferevent.h>

extern struct tgl_update_callback upd_cb;
extern int ready;
struct tgl_state *TLS;

pthread_t t;
sqlite3 *tg_storage;

static void* _tg_tgl_init(void*);
void tg_tgl_init() {
	pthread_create(&t, NULL, _tg_tgl_init, NULL);	
}
void tg_tgl_destruct() {
	sqlite3_close(tg_storage);
}

static void _tg_storage_init() {
	int rc = sqlite3_open("/tmp/tg", &tg_storage);
	char* err_msg = NULL;
	if(rc != SQLITE_OK) {
		fprintf(stderr, "Unable to open database: %s\n", sqlite3_errmsg(tg_storage));
		sqlite3_close(tg_storage);
		exit(1);
	}
	char *tables = "create table peers(type int, id int primary key, access_hash bigint);" 
		"create table messages(peer_type bigint, peer_id bigint, id bigint primary key, access_hash bigint, media int, d int, caption text, size int);";
	rc = sqlite3_exec(tg_storage, tables, 0, 0, &err_msg);
	if(rc != SQLITE_OK) {
		fprintf(stderr, "Failed to create tables\nSQL Error: %s\n", err_msg);
		exit(1);
	}
}

void tg_storage_peer_add(tgl_peer_id_t p) {
	char* insert;
	char* err_msg = NULL;
	tasprintf(&insert, "insert into peers(type, id, access_hash) values(%i, %i, %lli);", p.peer_type, p.peer_id, p.access_hash);
	int rc = sqlite3_exec(tg_storage, insert, 0, 0, &err_msg);
	if(rc != SQLITE_OK) {
		fprintf(stderr, "Failed to insert peer: %s\n", err_msg);
	}
	tfree_str(insert);
}

static tgl_peer_id_t convert_id(sqlite3_stmt* res) {
	tgl_peer_id_t id;
	id.peer_type = sqlite3_column_int(res, 0);
	id.peer_id = sqlite3_column_int(res, 1);
	id.access_hash = sqlite3_column_int64(res, 2);
	return id;
}

void tg_storage_peer_enumerate(void *buf, fuse_fill_dir_t filler) {
	char* select = "select * from peers;";
	char* err_msg = NULL;
	sqlite3_stmt* res;
	int rc = sqlite3_prepare_v2(tg_storage, select, -1, &res, NULL);
	if(rc != SQLITE_OK) {
		fprintf(stderr, "Failed to enumerate peers: %s\n", err_msg);
		return;
	}
	int step = sqlite3_step(res);
	while(step == SQLITE_ROW) {
		tgl_peer_id_t id = convert_id(res);
		tgl_peer_t *peer = tgl_peer_get(TLS, id);
		if(peer->print_name) 
			filler(buf, peer->print_name, NULL, 0); 
		step = sqlite3_step(res);
	}
	sqlite3_finalize(res);
}

void tg_storage_msg_add(struct tgl_message m) {
	char* insert;
	char* err_msg = NULL;
	sqlite3_stmt* res;
	insert = "insert into messages(peer_type, peer_id, id, access_hash, media, d, caption, size) "
			"values(?, ?, ?, ?, ?, ?, replace(?, '/', '_'), ?);";
	int rc = sqlite3_prepare_v2(tg_storage, insert, -1, &res, 0);
	if(rc != SQLITE_OK) {
		fprintf(stderr, "Failed to insert msg: %s\n", err_msg);
		return;
	}
	sqlite3_bind_int64(res, 1, m.permanent_id.peer_type);
	sqlite3_bind_int64(res, 2, m.permanent_id.peer_id);
	sqlite3_bind_int64(res, 3, m.permanent_id.id);
	sqlite3_bind_int64(res, 4, m.permanent_id.access_hash);
	sqlite3_bind_int64(res, 6, m.date);
	char dateCaption[100];
	char* caption;
	int caption_len;
	if(m.media.type == tgl_message_media_document && m.media.document->caption != NULL) {
		caption = m.media.document->caption;	
	} else if(m.media.caption != NULL && strlen(m.media.caption) > 0) {
		caption = m.media.caption;
	} else {
		time_t stamp = m.date;
		struct tm *t = localtime(&stamp);
		strftime(dateCaption, sizeof(dateCaption)-1, "%Y-%m-%d %H:%M", t);
		caption = dateCaption;
	}
	fprintf(stderr, "caption: %s\n", caption);
	caption_len = strlen(caption);
	sqlite3_bind_text(res, 7, caption, caption_len, SQLITE_STATIC);
	switch(m.media.type) {
		case tgl_message_media_video:
			sqlite3_bind_int64(res, 5, TGFS_VIDEOS);
			break;
		case tgl_message_media_photo:
			sqlite3_bind_int64(res, 5, TGFS_PHOTOS);
			break;
		case tgl_message_media_document:
			sqlite3_bind_int64(res, 5, TGFS_MUSIC);
			sqlite3_bind_int(res, 8, m.media.document->size);
			break;
		default:
			sqlite3_bind_text(res, 7, m.message, sizeof(m.message), SQLITE_STATIC);
	}
	if(sqlite3_step(res) != SQLITE_DONE) {
		fprintf(stderr,  "Failed to insert message\n");
	}
	sqlite3_finalize(res);
}

//TODO: fix potential bug with same caption in different chats
int tg_storage_msg_stat(const char* path, struct stat *stbuf, int type) {
	char* select = "select size, d from messages where caption = ? and media = ?;";
	sqlite3_stmt* res;
	int rc = sqlite3_prepare_v2(tg_storage, select, -1, &res, NULL);
	if(rc != SQLITE_OK) {
		fprintf(stderr, "Failed to stat");
		return -EIO;
	}
	sqlite3_bind_text(res, 1, path, strlen(path), SQLITE_STATIC);
	sqlite3_bind_int64(res, 2, type);
	int result = -ENOENT;
	if(sqlite3_step(res) == SQLITE_ROW) {
		stbuf->st_mode = S_IFREG | 0600;
		stbuf->st_nlink = 1;
		stbuf->st_size = sqlite3_column_int(res, 0);
		stbuf->st_mtime = sqlite3_column_int(res, 1);
		stbuf->st_atime = stbuf->st_mtime;
		stbuf->st_ctime = stbuf->st_mtime;
		result = 0;
	}
	sqlite3_finalize(res);
	return result;
}

void tg_storage_msg_enumerate_name(tgl_peer_id_t peer, int type, void *buf, fuse_fill_dir_t filler) {
	char* select = "select caption from messages where peer_id = ? and peer_type = ? and media = ?;";
	char* err_msg = NULL;
	sqlite3_stmt* res;
	int rc = sqlite3_prepare_v2(tg_storage, select, -1, &res, NULL);
	if(rc != SQLITE_OK) {
		fprintf(stderr, "Failed to enumerate peers: %s\n", err_msg);
		return;
	}
	sqlite3_bind_int64(res, 1, peer.peer_id);
	sqlite3_bind_int64(res, 2, peer.peer_type);
	sqlite3_bind_int(res, 3, type);
	int step = sqlite3_step(res);
	while(step == SQLITE_ROW) {
		const char* name = (char*)sqlite3_column_text(res, 0);
		if(name == NULL) {
			fprintf(stderr, "Shit\n");
		}
		fprintf(stderr, "name: >%s< size: %ld\n", name, strlen(name));
		filler(buf, name, NULL, 0); 
		step = sqlite3_step(res);
	}
	sqlite3_finalize(res);
}

struct tgl_message *tg_storage_msg_by_name(tgl_peer_t *peer, int type, const char *filename) {
	char* select = "select peer_type, peer_id, id, access_hash from messages where caption = ? and media = ? and peer_id = ?;";
	sqlite3_stmt* res;
	int rc = sqlite3_prepare_v2(tg_storage, select, -1, &res, NULL);
	if(rc != SQLITE_OK) {
		fprintf(stderr, "Failed to get msg by name\n");
		return NULL;
	}
	sqlite3_bind_text(res, 1, filename, strlen(filename), SQLITE_STATIC);
	sqlite3_bind_int64(res, 2, type);
	sqlite3_bind_int64(res, 3, peer->id.peer_id);
	
	struct tgl_message *result = NULL;
	if(sqlite3_step(res) == SQLITE_ROW) {
		tgl_message_id_t id;
		id.peer_type = (unsigned)sqlite3_column_int64(res, 0);
		id.peer_id = (unsigned)sqlite3_column_int64(res, 1);
		id.id = sqlite3_column_int64(res, 2);
		id.access_hash = sqlite3_column_int64(res, 3);
		result = tgl_message_get(TLS, &id);
	}
	sqlite3_finalize(res);
	return result;

}

static void tg_download_attachments_callback(struct tgl_state *TLS, void *callback_extra, int success, int size, struct tgl_message *list[]) {
	for(int i = 0; i < size; i++) {
		struct tgl_message* msg = list[i];
		tg_storage_msg_add(*msg);
	}
	pthread_mutex_unlock(callback_extra);
}

void tg_donwload_attachments(tgl_peer_id_t peer_id, int type) {
	int type_code;
	switch(type) {
		case TGFS_VIDEOS:
			type_code = CODE_input_messages_filter_video;
			break;
		case TGFS_PHOTOS:
			type_code = CODE_input_messages_filter_photos;
			break;
		case TGFS_DOCUMENTS:
			type_code = CODE_input_messages_filter_document;
			break;
		case TGFS_MUSIC:
			type_code = CODE_input_messages_filter_audio_documents;
			break;
		default:
			type_code = -1;
	}


	pthread_mutex_t* m = (pthread_mutex_t*)malloc(sizeof(pthread_mutex_t));
	pthread_mutex_init(m, NULL);
	pthread_mutex_lock(m);
	tgl_do_msg_search(
			TLS, 
			peer_id, 
			type_code, 
			0, /* from */ 
			0, /* to */
			100, /* limit */
			0, /* offset */
			NULL, /* query */
			0, /* query len*/
			tg_download_attachments_callback,
			m /* callback extra */
	);	
	pthread_mutex_lock(m);
	pthread_mutex_destroy(m);
	free(m);
}

static void tg_read_file_callback(struct tgl_state *TLS, void *callback_extra, int success) {
	struct tgl_read_data *data = (struct tgl_read_data*)callback_extra;
	data->success = success;
	pthread_mutex_t *mutex = data->mutex;
	pthread_mutex_unlock(mutex);
	printf("callback success %i, read len = %i\n", success, data->len);
}

size_t tg_read_file(struct tgl_message *msg, void *buf, size_t size, off_t offset) {
	struct tgl_read_data *data = malloc(sizeof(struct tgl_read_data));
	data->mutex = malloc(sizeof(pthread_mutex_t));
	pthread_mutex_init(data->mutex, NULL);

	data->offset = offset;
	data->len = size;

	pthread_mutex_lock(data->mutex);
	tgl_do_read_audio(TLS, msg->media.document, tg_read_file_callback, data);
	pthread_mutex_lock(data->mutex);
	pthread_mutex_destroy(data->mutex);


	if(data->success == 1) {
		memcpy(buf, data->bytes, size);
		size = data->len > size ? size : data->len;

		free(data->bytes);
	} else {
		size = 0;
	}

	free(data);

	return size;
	
}


static void* _tg_tgl_init(void* arg) {
	(void)arg;
	TLS = tgl_state_alloc();
	tgl_set_rsa_key(TLS, "tg-server.pub");
	tgl_set_download_directory(TLS, "~/.tgfs");
	tgl_set_callback(TLS, &upd_cb);

	struct event_base *ev = event_base_new ();
	tgl_set_ev_base (TLS, ev);
	tgl_set_net_methods (TLS, &tgl_conn_methods);
	tgl_set_timer_methods (TLS, &tgl_libevent_timers);
	assert (TLS->timer_methods);


	tgl_register_app_id (TLS, TGFS_APP_ID, TGFS_APP_HASH); 
  	tgl_set_app_version (TLS, "tgfs " PACKAGE_VERSION);

	// TODO: fix no rsa key segfault
	int init = tgl_init(TLS);
	assert(init >= 0);

	// Simulate empty auth file
	// TODO: Change to prefered DC in runtime
    	bl_do_dc_option (TLS, 0, 1, "", 0, TG_SERVER_1, strlen (TG_SERVER_1), 443);
    	bl_do_dc_option (TLS, 0, 2, "", 0, TG_SERVER_2, strlen (TG_SERVER_2), 443);
    	bl_do_dc_option (TLS, 0, 3, "", 0, TG_SERVER_3, strlen (TG_SERVER_3), 443);
    	bl_do_dc_option (TLS, 0, 4, "", 0, TG_SERVER_4, strlen (TG_SERVER_4), 443);
    	bl_do_dc_option (TLS, 0, 5, "", 0, TG_SERVER_5, strlen (TG_SERVER_5), 443);
    	bl_do_set_working_dc (TLS, TG_SERVER_DEFAULT);

	tgl_set_rsa_key_direct (TLS, tglmp_get_default_e (), tglmp_get_default_key_len (), tglmp_get_default_key ());

	read_auth_file();

  	tgl_login (TLS);

	_tg_storage_init();

	while (1) {
    		event_base_loop (TLS->ev_base, EVLOOP_ONCE);
    		usleep(500);
	}
	return NULL;
}

pthread_t t;
