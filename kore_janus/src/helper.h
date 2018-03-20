
#include <stdlib.h> //exit
#include <dlfcn.h>
#include <dirent.h>
#include <unistd.h> //write

#include <errno.h> //eintr, errno
#include <glib.h>

#include <kore/kore.h>
#include <kore/tasks.h>

//#include "debug.h" //janus_print
#include "utils.h" //janus_pidfile_remove
#include "config.h"
#include "janus.h"
#include "version.h"
#include "events.h"
#include "apierror.h"
#include "log.h"
#include "ip-utils.h"

#ifdef __MACH__
#define SHLIB_EXT "0.dylib"
#else
#define SHLIB_EXT ".so"
#endif

#define JANUS_NAME				"Janus WebRTC Gateway"
#define JANUS_AUTHOR			"Meetecho s.r.l."
#define JANUS_VERSION			25
#define JANUS_VERSION_STRING	"0.2.5"
#define JANUS_SERVER_NAME		"MyJanusInstance"

#define DEFAULT_SESSION_TIMEOUT		3000

#define CONFDIR "/home/globik/webrtc/kore_janus/configs"

#define WEBSOCKET_PAYLOAD_SINGLE	125
#define WEBSOCKET_PAYLOAD_EXTEND_1	126
#define WEBSOCKET_PAYLOAD_EXTEND_2	127
//#define CONFDIR "/usr/local/etc/janus"
// for trucking of websocket connections
struct ex{
int id;
int b;
guint64 sender_id;
};

extern const char* janus_get_api_error(int);
extern  gint stop_signal;
extern  volatile gint stop;
gchar *server_name;
static janus_mutex sessions_mutex;
void janus_session_free(janus_session *);
janus_session *janus_session_find(guint64);
janus_session *janus_session_create(guint64);
void janus_plugin_notify_event(janus_plugin *, janus_plugin_session *, json_t *);
void janus_plugin_end_session(janus_plugin_session *);
void janus_plugin_close_pc(janus_plugin_session *);
void janus_plugin_relay_data(janus_plugin_session *, char *, int);
void janus_plugin_relay_rtp(janus_plugin_session *, int, char *, int);
void janus_plugin_relay_rtcp(janus_plugin_session *, int, char *, int);
int janus_plugin_push_event(janus_plugin_session *,janus_plugin *,const char *,json_t *, json_t *); 
json_t *janus_plugin_handle_sdp(janus_plugin_session *plugin_session, janus_plugin *plugin, const char *sdp_type, const char *sdp,gboolean);
janus_callbacks janus_handler_plugin;
void fuck_up(struct kore_task*);
void incoming_message(janus_ice_handle*,janus_session *,json_t*,guint64,struct connection*);
void do_trickle(janus_ice_handle*,janus_session*,json_t*,guint64,struct connection*);