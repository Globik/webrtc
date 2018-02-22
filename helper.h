//#include <limits.h>
#include <stdlib.h> //exit
//#include <net/if.h>
//#include <netdb.h>
#include <dlfcn.h>
#include <dirent.h>
#include <unistd.h> //write
//#include <poll.h>
#include <errno.h> //eintr, errno
#include <glib.h>
#include "debug.h" //janus_print
#include "utils.h" //janus_pidfile_remove
#include "config.h"
#include "janus.h"
#include "events.h"
#include "apierror.h"
#include "log.h"
#include "ip-utils.h"

#ifdef __MACH__
#define SHLIB_EXT "0.dylib"
#else
#define SHLIB_EXT ".so"
#endif

//#include "log.h"
extern  gint stop_signal;
extern  volatile gint stop;
extern gboolean daemonize;
extern int pipefd[2];
gchar *server_name;
static janus_mutex sessions_mutex;
static GHashTable *old_sessions, *sessions;
static uint session_timeout;
static GMainContext *sessions_watchdog_context;
//static
	gchar *local_ip;
static gchar *public_ip;

//static
	GHashTable *plugins;
static GHashTable *plugins_so;

const char *path;
static janus_config *config;
janus_config_item*item;
DIR *dir;
struct dirent *pluginent;
gchar **disabled_plugins;
static char *config_file;
static char *configs_folder;
//static janus_callbacks janus_handler_plugin;

int janus_log_level;
gboolean janus_log_timestamps;
gboolean janus_log_colors;
int lock_debug;
static gboolean notify_events;
janus_plugin *fucker;

void janus_handle_signal(int);
void janus_termination_handler(void);
gint janus_is_stopping(void);
gboolean janus_cleanup_session(gpointer);
void janus_session_free(janus_session *);
janus_session *janus_session_find(guint64);
janus_session *janus_session_create(guint64);
gpointer janus_sessions_watchdog(gpointer);
static gboolean janus_check_sessions(gpointer);
static void janus_session_schedule_destruction(janus_session *,gboolean, gboolean, gboolean);
gchar *janus_get_local_ip(void);
gchar *janus_get_public_ip(void);
void janus_set_public_ip(const char *ip);
void janus_plugin_close(gpointer, gpointer, gpointer);
janus_plugin *janus_plugin_find(const gchar *);
void janus_session_notify_event(janus_session *, json_t *);
void janus_plugin_notify_event(janus_plugin *, janus_plugin_session *, json_t *);
void janus_pluginso_close(gpointer, gpointer, gpointer);
void janus_plugin_end_session(janus_plugin_session *);
void janus_plugin_close_pc(janus_plugin_session *);
void janus_plugin_relay_data(janus_plugin_session *, char *, int);
void janus_plugin_relay_rtp(janus_plugin_session *, int, char *, int);
void janus_plugin_relay_rtcp(janus_plugin_session *, int, char *, int);
int janus_plugin_push_event(janus_plugin_session *,janus_plugin *,const char *,json_t *, json_t *); 
json_t *janus_plugin_handle_sdp(janus_plugin_session *plugin_session, janus_plugin *plugin, const char *sdp_type, const char *sdp);
void plug_fucker(GHashTable *,janus_plugin*);
void load_plugin(const char*);

static janus_callbacks janus_handler_plugin =
	{
		.push_event = janus_plugin_push_event,
		.relay_rtp = janus_plugin_relay_rtp,
		.relay_rtcp = janus_plugin_relay_rtcp,
		.relay_data = janus_plugin_relay_data,
		.close_pc = janus_plugin_close_pc,
		.end_session = janus_plugin_end_session,
		.events_is_enabled = janus_events_is_enabled,
		.notify_event = janus_plugin_notify_event,
	}; 
void select_local_ip(gchar*);










