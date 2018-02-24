#include <dlfcn.h>
#include <dirent.h>
#include <net/if.h>
#include <netdb.h>
#include <signal.h>
#include <getopt.h>
#include <sys/resource.h>
#include <sys/stat.h>
#include <poll.h>

#include "janus.h"
//#include "version.h"
//#include "cmdline.h"
//#include "config.h"
//#include "apierror.h"
//#include "log.h"
//#include "debug.h"
//#include "ip-utils.h"
#include "rtcp.h"
#include "auth.h"
#include "record.h"
//#include "events.h"
//#include "transports/transport.h"

#include "helper.h"
janus_config *config = NULL;
static char *config_file = NULL;
char *configs_folder = NULL;
//static 
	GHashTable *transports = NULL;
//static
	GHashTable *transports_so = NULL;
//static
	GHashTable *eventhandlers = NULL;
//static
	GHashTable *eventhandlers_so = NULL;
GHashTable *plugins = NULL;
static GHashTable *plugins_so = NULL;
gboolean daemonize = FALSE;
int pipefd[2];
//static 
	char *api_secret = NULL, *admin_api_secret = NULL;
gchar *local_ip = NULL;
static gchar *public_ip = NULL;
volatile gint stop = 0;gint stop_signal = 0;
gchar *server_name = NULL;
static uint session_timeout = DEFAULT_SESSION_TIMEOUT;
int janus_log_level = LOG_INFO;
gboolean janus_log_timestamps = FALSE;
gboolean janus_log_colors = FALSE;
int lock_debug = 0;
static gint initialized = 0, stopping = 0;
static janus_transport_callbacks *gateway = NULL;
static gboolean ws_janus_api_enabled = FALSE;
static gboolean ws_admin_api_enabled = FALSE;
static gboolean notify_events = TRUE;
static size_t json_format = JSON_INDENT(3) | JSON_PRESERVE_ORDER;
static janus_mutex sessions_mutex;
static GHashTable *sessions = NULL, *old_sessions = NULL;
static GMainContext *sessions_watchdog_context = NULL;

gint main(int argc, char *argv[]){
fuck_up();
/*
	
	janus_log_level=7;
	gboolean use_stdout = TRUE;
	// Core dumps may be disallowed by parent of this process; change that 
	struct rlimit core_limits;
	core_limits.rlim_cur = core_limits.rlim_max = RLIM_INFINITY;
	setrlimit(RLIMIT_CORE, &core_limits);
puzomerka(use_stdout);
stop_signal_hook();
configs_folder = g_strdup (CONFDIR);
	g_print("configs_folder %s\n",configs_folder);
set_conf_file(configs_folder);
conf_ice_enforce_list();
	conf_ice_ignore_list();
conf_interface();
	#if !GLIB_CHECK_VERSION(2, 36, 0)
	g_type_init();
#endif
	janus_log_timestamps = TRUE;
	janus_log_colors = TRUE;
// What is the local IP? 
g_print("Selecting local IP address...\n");
	if(local_ip==NULL)local_ip=select_local_ip();
conf_session_timeout();
g_print("Using %s as local IP...\n", local_ip);
// Is there any API secret to consider? 
	api_secret = NULL;
// Setup ICE stuff (e.g., checking if the provided STUN server is correct) 
	char *stun_server = NULL, *turn_server = NULL;
	uint16_t stun_port = 0, turn_port = 0;
	char *turn_type = NULL, *turn_user = NULL, *turn_pwd = NULL;
	char *turn_rest_api = NULL, *turn_rest_api_key = NULL;
#ifdef HAVE_LIBCURL
	char *turn_rest_api_method = NULL;
#endif
	const char *nat_1_1_mapping = NULL;
	uint16_t rtp_min_port = 0, rtp_max_port = 0;
	gboolean ice_lite = FALSE, ice_tcp = FALSE, ipv6 = FALSE;
item = janus_config_get_item_drilldown(config, "media", "ipv6");
ipv6 = (item && item->value) ? janus_is_true(item->value) : FALSE;
conf_turn();
conf_nice_debug();
test_private_address();
// Are we going to force BUNDLE and/or rtcp-mux? nn
gboolean force_bundle = FALSE, force_rtcpmux = FALSE;
conf_force_bundle_or_and_rtcp_mux();
// NACK related stuff 
conf_max_nack_queue();
//no-media timer 
conf_no_media_timer();
g_print("Setup OpenSSL stuff\n");
const char* server_pem=conf_cert_pem();
const char* server_key=conf_cert_key();
JANUS_LOG(LOG_VERB, "Using certificates:\n\t%s\n\t%s\n", server_pem, server_key);
SSL_library_init();
SSL_load_error_strings();
OpenSSL_add_all_algorithms();
	// ... and DTLS-SRTP in particular 
if(janus_dtls_srtp_init(server_pem, server_key) < 0) {
exit(1);
}
	// Check if there's any custom value for the starting MTU to use in the BIO filter 
conf_dtls_mtu();

#ifdef HAVE_SCTP
	// Initialize SCTP for DataChannels 
	if(janus_sctp_init() < 0) {exit(1);}else{g_print("janus_sctp_init - OK\n");}
#else
	JANUS_LOG(LOG_WARN, "Data Channels support not compiled\n");
#endif
// Sessions 
	sessions = g_hash_table_new_full(g_int64_hash, g_int64_equal, (GDestroyNotify)g_free, NULL);
	old_sessions = g_hash_table_new_full(g_int64_hash, g_int64_equal, (GDestroyNotify)g_free, NULL);
	janus_mutex_init(&sessions_mutex);
	// Start the sessions watchdog 
	sessions_watchdog_context = g_main_context_new();
	GMainLoop *watchdog_loop = g_main_loop_new(sessions_watchdog_context, FALSE);
	GError *error = NULL;
	GThread *watchdog = g_thread_try_new("sessions watchdog", &janus_sessions_watchdog, watchdog_loop, &error);
	if(error != NULL) {
		JANUS_LOG(LOG_FATAL, "Got error %d (%s) trying to start sessions watchdog...\n", error->code, error->message ? error->message : "??");
		exit(1);
	}

gchar **disabled_plugins = NULL;
struct dirent *pluginent = NULL;
disabled_plugins = NULL;
const char *path=NULL;
DIR *dir=NULL;
// Load plugins 
//	path = PLUGINDIR;
//path="/usr/local/lib/janus/plugins";
//path="/home/globik/webrtc/plugins";
path="/home/globik/webrtc";
load_plugin(path);
if(disabled_plugins != NULL)g_strfreev(disabled_plugins);
disabled_plugins = NULL;
gboolean janus_api_enabled = FALSE, admin_api_enabled = FALSE;
	
while(!g_atomic_int_get(&stop)) {
		usleep(250000); 
		// A signal will cancel usleep() but not g_usleep() 
	}
// Done 
	JANUS_LOG(LOG_INFO, "Ending watchdog mainloop...\n");
	g_main_loop_quit(watchdog_loop);
	g_thread_join(watchdog);
	watchdog = NULL;
	g_main_loop_unref(watchdog_loop);
	g_main_context_unref(sessions_watchdog_context);
	sessions_watchdog_context = NULL;

	if(config)
		janus_config_destroy(config);

	JANUS_LOG(LOG_INFO, "Closing transport plugins:\n");
	if(transports != NULL) {
		g_hash_table_foreach(transports, janus_transport_close, NULL);
		g_hash_table_destroy(transports);
	}
	if(transports_so != NULL) {
		g_hash_table_foreach(transports_so, janus_transportso_close, NULL);
		g_hash_table_destroy(transports_so);
	}

	JANUS_LOG(LOG_INFO, "Destroying sessions...\n");
	g_clear_pointer(&sessions, g_hash_table_destroy);
	g_clear_pointer(&old_sessions, g_hash_table_destroy);
	JANUS_LOG(LOG_INFO, "Freeing crypto resources...\n");
	janus_dtls_srtp_cleanup();
	EVP_cleanup();
	ERR_free_strings();
#ifdef HAVE_SCTP
	JANUS_LOG(LOG_INFO, "De-initializing SCTP...\n");
	janus_sctp_deinit();
#endif
	janus_ice_deinit();
	janus_auth_deinit();

	JANUS_LOG(LOG_INFO, "Closing plugins.\n");
	if(plugins != NULL) {
		JANUS_LOG(LOG_WARN, "PLUGINS NOT NULL, DELETENG NOW!!!!!!!!!!!!!!!!\n");
		g_hash_table_foreach(plugins, janus_plugin_close, NULL);
		g_hash_table_destroy(plugins);
	}else{JANUS_LOG(LOG_INFO, "Closing plugins: PLUGINS IS NULL\n");}
	if(plugins_so != NULL) {
		JANUS_LOG(LOG_INFO, "PLUGINS_SO:\n");
		g_hash_table_foreach(plugins_so, janus_pluginso_close, NULL);
		g_hash_table_destroy(plugins_so);
	}else{JANUS_LOG(LOG_INFO, "Closing plugins: PLUGINS_SO IS NULL\n");}
	JANUS_LOG(LOG_INFO, "Closing event handlers:\n");
	janus_events_deinit();
	if(eventhandlers != NULL) {
		g_hash_table_foreach(eventhandlers, janus_eventhandler_close, NULL);
		g_hash_table_destroy(eventhandlers);
	}
	if(eventhandlers_so != NULL) {
		g_hash_table_foreach(eventhandlers_so, janus_eventhandlerso_close, NULL);
		g_hash_table_destroy(eventhandlers_so);
	}
g_free(local_ip);
JANUS_PRINT("Bye!\n");
exit(0);
*/
}