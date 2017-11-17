#define _DEFAULT_SOURCE

#include "tg.h"
#include "auth.h"

#include <unistd.h>
#include <pthread.h>
#include <event2/event.h>
#include <event2/bufferevent.h>

extern struct tgl_update_callback upd_cb;
extern int ready;
struct tgl_state *TLS;

pthread_t t;

static void* _tg_tgl_init(void*);
void tg_tgl_init() {
	pthread_create(&t, NULL, _tg_tgl_init, NULL);	
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

	int init = tgl_init(TLS);
	assert(init >= 0);

	// Simulate empty auth file
    	bl_do_dc_option (TLS, 0, 1, "", 0, TG_SERVER_1, strlen (TG_SERVER_1), 443);
    	bl_do_dc_option (TLS, 0, 2, "", 0, TG_SERVER_2, strlen (TG_SERVER_2), 443);
    	bl_do_dc_option (TLS, 0, 3, "", 0, TG_SERVER_3, strlen (TG_SERVER_3), 443);
    	bl_do_dc_option (TLS, 0, 4, "", 0, TG_SERVER_4, strlen (TG_SERVER_4), 443);
    	bl_do_dc_option (TLS, 0, 5, "", 0, TG_SERVER_5, strlen (TG_SERVER_5), 443);
    	bl_do_set_working_dc (TLS, TG_SERVER_DEFAULT);

	tgl_set_rsa_key_direct (TLS, tglmp_get_default_e (), tglmp_get_default_key_len (), tglmp_get_default_key ());

	read_auth_file();

  	tgl_login (TLS);

	while (1) {
    		event_base_loop (TLS->ev_base, EVLOOP_ONCE);
    		usleep(1000);
	}
	return NULL;
}

pthread_t t;
