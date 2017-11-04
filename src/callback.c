#include <stdio.h>
#include <assert.h>
#include <malloc.h>
#include <pthread.h>

#include <tgl/tgl.h>
#include <tgl/tgl-binlog.h>
#include <tgl/mtproto-client.h>
#include <tgl/tgl-queries.h>

#include "auth.h"

extern struct tgl_state *TLS;
extern int ready;

extern tgl_peer_id_t *peers;
extern int peers_length;

void on_login (struct tgl_state *TLS) {
	write_auth_file();
}

void on_failed_login (struct tgl_state *TLS) {
	printf("Login failed\n");
}

static void parse_dialog_list (struct tgl_state *TLSR, void *callback_extra, int success, int size, tgl_peer_id_t p[], tgl_message_id_t *last_msg_id[], int unread_count[])  {
	peers = (tgl_peer_id_t*)malloc(size*sizeof(tgl_peer_id_t));
	memcpy(peers, p, size*sizeof(tgl_peer_id_t));
	peers_length = size;
	ready = 1;
}

void on_started (struct tgl_state *TLS) {
	tgl_do_get_dialog_list(TLS, 1000, 0, parse_dialog_list, 0);
}

void do_get_values (struct tgl_state *TLS, enum tgl_value_type type, const char *prompt, int num_values,
		void (*callback)(struct tgl_state *TLS, const char *string[], void *arg), void *arg) {
	switch(type) {
		case tgl_phone_number:
			printf("phone number: ");
			char *result[10];
			result[0] = malloc(50*sizeof(char));
			scanf("%s", result[0]);
			callback(TLS, (void*)result, arg); 
			break;
		case tgl_code:
			printf("code: ");
			scanf("%s", result[0]);
			callback(TLS, (void*)result, arg);
			break;
		default:
			assert(0);
	}
}

struct tgl_update_callback upd_cb = {
  .new_msg = 0,
  .marked_read = 0,
  .logprintf = 0,
  .get_values = do_get_values,
  .logged_in = on_login,
  .started = on_started,
  .type_notification = 0,
  .type_in_chat_notification = 0,
  .type_in_secret_chat_notification = 0, 
  .status_notification = 0, 
  .user_registered = 0, 
  .user_activated = 0, 
  .new_authorization = 0, 
  .user_update = 0,
  .chat_update = 0,
  .secret_chat_update = 0,
  .channel_update = 0,
  .msg_receive = 0,
  .our_id = 0,
  .user_status_update = 0,
  .on_failed_login = on_failed_login
};

