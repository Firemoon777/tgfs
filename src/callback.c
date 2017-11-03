#include <stdio.h>
#include <assert.h>
#include <malloc.h>
#include <unistd.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <fcntl.h>

#include <tgl/tgl.h>
#include <tgl/tgl-binlog.h>
#include <tgl/mtproto-client.h>
#include <tgl/tgl-queries.h>

#define DC_SERIALIZED_MAGIC 0x868aa81d

extern struct tgl_state *TLS;

const char* get_auth_key_filename () {
	return "/home/firemoon/.tgfs/auth";
}

void write_dc (struct tgl_dc *DC, void *extra) {
  int auth_file_fd = *(int *)extra;
  if (!DC) {
    int x = 0;
    assert (write (auth_file_fd, &x, 4) == 4);
    return;
  } else {
    int x = 1;
    assert (write (auth_file_fd, &x, 4) == 4);
  }

  assert (DC->flags & TGLDCF_LOGGED_IN);

  assert (write (auth_file_fd, &DC->options[0]->port, 4) == 4);
  int l = strlen (DC->options[0]->ip);
  assert (write (auth_file_fd, &l, 4) == 4);
  assert (write (auth_file_fd, DC->options[0]->ip, l) == l);
  assert (write (auth_file_fd, &DC->auth_key_id, 8) == 8);
  assert (write (auth_file_fd, DC->auth_key, 256) == 256);
}


void write_auth_file (void) {
  int auth_file_fd = open (get_auth_key_filename (), O_CREAT | O_RDWR, 0600);
  if(auth_file_fd < 0) {
	perror("auth_file");
  }
  assert (auth_file_fd >= 0); 
  int x = DC_SERIALIZED_MAGIC;
  assert (write (auth_file_fd, &x, 4) == 4); 
  assert (write (auth_file_fd, &TLS->max_dc_num, 4) == 4); 
  assert (write (auth_file_fd, &TLS->dc_working_num, 4) == 4); 

  tgl_dc_iterator_ex (TLS, write_dc, &auth_file_fd);

  assert (write (auth_file_fd, &TLS->our_id.peer_id, 4) == 4); 
  close (auth_file_fd);
}

void read_dc (int auth_file_fd, int id, unsigned ver) {
  int port = 0;
  assert (read (auth_file_fd, &port, 4) == 4);
  int l = 0;
  assert (read (auth_file_fd, &l, 4) == 4);
  assert (l >= 0 && l < 100);
  char ip[100];
  assert (read (auth_file_fd, ip, l) == l);
  ip[l] = 0;

  long long auth_key_id;
  static unsigned char auth_key[256];
  assert (read (auth_file_fd, &auth_key_id, 8) == 8);
  assert (read (auth_file_fd, auth_key, 256) == 256);

  //bl_do_add_dc (id, ip, l, port, auth_key_id, auth_key);
  bl_do_dc_option (TLS, 0, id, "DC", 2, ip, l, port);
  bl_do_set_auth_key (TLS, id, auth_key);
  bl_do_dc_signed (TLS, id);
}


void read_auth_file (void) {
  int auth_file_fd = open (get_auth_key_filename (), O_CREAT | O_RDWR, 0600);
  if (auth_file_fd < 0) {
    return;
  }
  assert (auth_file_fd >= 0);
  unsigned x;
  unsigned m;
  if (read (auth_file_fd, &m, 4) < 4 || (m != DC_SERIALIZED_MAGIC)) {
    close (auth_file_fd);
    return;
  }
  assert (read (auth_file_fd, &x, 4) == 4);
  assert (x > 0);
  int dc_working_num;
  assert (read (auth_file_fd, &dc_working_num, 4) == 4);

  int i;
  for (i = 0; i <= (int)x; i++) {
    int y;
    assert (read (auth_file_fd, &y, 4) == 4);
    if (y) {
      read_dc (auth_file_fd, i, m);
    }
  }
  bl_do_set_working_dc (TLS, dc_working_num);
  int our_id;
  int l = read (auth_file_fd, &our_id, 4);
  if (l < 4) {
    assert (!l);
  }
  if (our_id) {
    bl_do_set_our_id (TLS, TGL_MK_USER (our_id));
  }
  close (auth_file_fd);
}



void on_login (struct tgl_state *TLS) {
	printf("on_login start\n");
	write_auth_file();
	printf("on login end\n");
}

void on_failed_login (struct tgl_state *TLS) {
	printf("Login failed\n");
}

void dlist_cb (struct tgl_state *TLSR, void *callback_extra, int success, int size, tgl_peer_id_t peers[], tgl_message_id_t *last_msg_id[], int unread_count[])  {
	for(int i = 0; i < size; i++) {
		printf("peer[%i].id = %i\n", i, peers[i].peer_id);
	}
}
void on_started (struct tgl_state *TLS) {
	printf("On started\n");
	tgl_do_get_dialog_list(TLS, 10, 0, dlist_cb, 0);
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

