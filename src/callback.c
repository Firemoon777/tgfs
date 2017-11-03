#include <stdio.h>

#include "tgl/tgl.h"

void on_login (struct tgl_state *TLS) {
	printf("Login succeded\n");
}

void on_failed_login (struct tgl_state *TLS) {
	printf("Login failed\n");
}

void on_started (struct tgl_state *TLS) {
	printf("On started\n");
}

struct tgl_update_callback upd_cb = {
  .new_msg = 0,
  .marked_read = 0,
  .logprintf = 0,
  .get_values = 0,
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

