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
#include "version.h"
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


#define JANUS_NAME				"Janus WebRTC Gateway"
#define JANUS_AUTHOR			"Meetecho s.r.l."
#define JANUS_VERSION			25
#define JANUS_VERSION_STRING	"0.2.5"
#define JANUS_SERVER_NAME		"MyJanusInstance"

#define DEFAULT_SESSION_TIMEOUT		60


static janus_config *config = NULL;
static char *config_file = NULL;
static char *configs_folder = NULL;

static GHashTable *transports = NULL;
static GHashTable *transports_so = NULL;

static GHashTable *eventhandlers = NULL;
static GHashTable *eventhandlers_so = NULL;


	GHashTable *plugins = NULL;
static GHashTable *plugins_so = NULL;
gboolean daemonize = FALSE;
int pipefd[2];
static char *api_secret = NULL, *admin_api_secret = NULL;

//static
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
gint main(int argc, char *argv[])
{
	janus_log_level=7;
	// Core dumps may be disallowed by parent of this process; change that 
	struct rlimit core_limits;
	core_limits.rlim_cur = core_limits.rlim_max = RLIM_INFINITY;
	setrlimit(RLIMIT_CORE, &core_limits);

	g_print("Janus commit: %s\n", janus_build_git_sha);
	g_print("Compiled on:  %s\n\n", janus_build_git_time);
gboolean use_stdout = TRUE;
	g_print("after use_std_out\n");
	if(janus_log_init(0,use_stdout,"logfile")<0){g_print("no logfile error\n");exit(1);}
	JANUS_PRINT("---------------------------------------------------\n");
	g_print("  Starting Meetecho Janus (WebRTC Gateway) v%s\n", JANUS_VERSION_STRING);
	JANUS_PRINT("---------------------------------------------------\n\n");

	signal(SIGINT, janus_handle_signal);
	signal(SIGTERM, janus_handle_signal);
	atexit(janus_termination_handler);

	g_print(" Setup Glib \n");
	
#if !GLIB_CHECK_VERSION(2, 36, 0)
	g_type_init();
#endif
	janus_log_timestamps = TRUE;
	janus_log_colors = TRUE;
	
	// What is the local IP? 
	g_print("Selecting local IP address...\n");
	select_local_ip(local_ip);
	/*
	if(local_ip == NULL) {
		local_ip = janus_network_detect_local_ip_as_string(janus_network_query_options_any_ip);
		if(local_ip == NULL) {
			g_print("Couldn't find any address! using 127.0.0.1 as the local IP... (which is NOT going to work out of your machine)\n");
			local_ip = g_strdup("127.0.0.1");
		}
	}
	
	*/
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
	//item = janus_config_get_item_drilldown(config, "media", "ipv6");
	ipv6 = /*(item && item->value) ? janus_is_true(item->value) : */FALSE;
	janus_config_item*item = janus_config_get_item_drilldown(config, "media", "rtp_port_range");
	
	g_printf(" Check if we need to enable the ICE Lite mode \n");
	//item = janus_config_get_item_drilldown(config, "nat", "ice_lite");
	ice_lite =/* (item && item->value) ? janus_is_true(item->value) : */FALSE;
	//Check if we need to enable ICE-TCP support (warning: still broken, for debugging only) 
item = janus_config_get_item_drilldown(config, "nat", "ice_tcp");
	ice_tcp = (item && item->value) ? janus_is_true(item->value) : FALSE;
	// Any STUN server to use in Janus? 
	item = janus_config_get_item_drilldown(config, "nat", "stun_server");
	if(item && item->value)
		stun_server = (char *)item->value;
	item = janus_config_get_item_drilldown(config, "nat", "stun_port");
	if(item && item->value)
		stun_port = atoi(item->value);
	//Any 1:1 NAT mapping to take into account? 
	item = janus_config_get_item_drilldown(config, "nat", "nat_1_1_mapping");
	if(item && item->value) {
		g_print("Using nat_1_1_mapping for public ip - %s\n", item->value);
		if(!janus_network_string_is_valid_address(janus_network_query_options_any_ip, item->value)) {
			g_print("Invalid nat_1_1_mapping address %s, disabling...\n", item->value);
		} else {
			nat_1_1_mapping = item->value;
			janus_set_public_ip(item->value);
			janus_ice_enable_nat_1_1();
		}
	}
	// Any TURN server to use in Janus? 
	item = janus_config_get_item_drilldown(config, "nat", "turn_server");
	if(item && item->value)
		turn_server = (char *)item->value;
	item = janus_config_get_item_drilldown(config, "nat", "turn_port");
	if(item && item->value)
		turn_port = atoi(item->value);
	item = janus_config_get_item_drilldown(config, "nat", "turn_type");
	if(item && item->value)
		turn_type = (char *)item->value;
	item = janus_config_get_item_drilldown(config, "nat", "turn_user");
	if(item && item->value)
		turn_user = (char *)item->value;
	item = janus_config_get_item_drilldown(config, "nat", "turn_pwd");
	if(item && item->value)
		turn_pwd = (char *)item->value;
	/* Check if there's any TURN REST API backend to use */
	item = janus_config_get_item_drilldown(config, "nat", "turn_rest_api");
	if(item && item->value)
		turn_rest_api = (char *)item->value;
	item = janus_config_get_item_drilldown(config, "nat", "turn_rest_api_key");
	if(item && item->value)
		turn_rest_api_key = (char *)item->value;
#ifdef HAVE_LIBCURL
	item = janus_config_get_item_drilldown(config, "nat", "turn_rest_api_method");
	if(item && item->value)
		turn_rest_api_method = (char *)item->value;
#endif
	// Initialize the ICE stack now 
	
	/*
	const char *nat_1_1_mapping = NULL;
	uint16_t rtp_min_port = 0, rtp_max_port = 0;
	gboolean ice_lite = FALSE, ice_tcp = FALSE, ipv6 = FALSE;
	*/
	g_print("nat 1 1 mapping %s\n",nat_1_1_mapping);
	g_print("ice_tcp: %d\n",ice_tcp);
	g_print("ice_lite %d\n",ice_lite);
	g_print("ipv6 %d\n",ipv6);
	janus_ice_init(ice_lite, ice_tcp, ipv6, rtp_min_port, rtp_max_port);
	if(janus_ice_set_stun_server(stun_server, stun_port) < 0) {
		g_print("Invalid STUN address %s:%u\n", stun_server, stun_port);
		exit(1);
	}
	if(janus_ice_set_turn_server(turn_server, turn_port, turn_type, turn_user, turn_pwd) < 0) {
		g_print("Invalid TURN address %s:%u\n", turn_server, turn_port);
		exit(1);
	}
#ifndef HAVE_LIBCURL
	if(turn_rest_api != NULL || turn_rest_api_key != NULL) {
		g_print("A TURN REST API backend specified in the settings, but libcurl support has not been built\n");
	}
#else
	if(janus_ice_set_turn_rest_api(turn_rest_api, turn_rest_api_key, turn_rest_api_method) < 0) {
		g_print("Invalid TURN REST API configuration: %s (%s, %s)\n", turn_rest_api, turn_rest_api_key, turn_rest_api_method);
		exit(1);
	}
#endif
	item = janus_config_get_item_drilldown(config, "nat", "nice_debug");
	//if(item && item->value && janus_is_true(item->value)) {
		g_print("Enable libnice debugging\n");
		janus_ice_debugging_enable();
	//}
	if(stun_server == NULL && turn_server == NULL) {
		//No STUN and TURN server provided for Janus: make sure it isn't on a private address 
		gboolean private_address = FALSE;
		const char *test_ip = nat_1_1_mapping ? nat_1_1_mapping : local_ip;
		janus_network_address addr;
		if(janus_network_string_to_address(janus_network_query_options_any_ip, test_ip, &addr) != 0) {
			JANUS_LOG(LOG_ERR, "Invalid address %s..?\n", test_ip);
		} else {
			if(addr.family == AF_INET) {
				unsigned short int ip[4];
				sscanf(test_ip, "%hu.%hu.%hu.%hu", &ip[0], &ip[1], &ip[2], &ip[3]);
				if(ip[0] == 10) {
					// Class A private address 
					private_address = TRUE;
				} else if(ip[0] == 172 && (ip[1] >= 16 && ip[1] <= 31)) {
					// Class B private address 
					private_address = TRUE;
				} else if(ip[0] == 192 && ip[1] == 168) {
					// Class C private address 
					private_address = TRUE;
				}
			} else {
				// TODO Similar check for IPv6... 
			}
		}
		if(private_address) {
			JANUS_LOG(LOG_WARN, "Janus is deployed on a private address (%s) but you didn't specify any STUN server!"
			                    " Expect trouble if this is supposed to work over the internet and not just in a LAN...\n", test_ip);
		}
	}
	// Are we going to force BUNDLE and/or rtcp-mux? 
	gboolean force_bundle = FALSE, force_rtcpmux = FALSE;
	item = janus_config_get_item_drilldown(config, "media", "force-bundle");
	force_bundle = (item && item->value) ? janus_is_true(item->value) : FALSE;
	janus_ice_force_bundle(force_bundle);
	item = janus_config_get_item_drilldown(config, "media", "force-rtcp-mux");
	force_rtcpmux = (item && item->value) ? janus_is_true(item->value) : FALSE;
	janus_ice_force_rtcpmux(force_rtcpmux);
	/* NACK related stuff */
	item = janus_config_get_item_drilldown(config, "media", "max_nack_queue");
	if(item && item->value) {
		int mnq = atoi(item->value);
		if(mnq < 0) {
			JANUS_LOG(LOG_WARN, "Ignoring max_nack_queue value as it's not a positive integer\n");
		} else if(mnq > 0 && mnq < 200) {
			JANUS_LOG(LOG_WARN, "Ignoring max_nack_queue value as it's less than 200\n");
		} else {
			janus_set_max_nack_queue(mnq);
		}
	}
	//no-media timer 
	item = janus_config_get_item_drilldown(config, "media", "no_media_timer");
	if(item && item->value) {
		int nmt = atoi(item->value);
		if(nmt < 0) {
			JANUS_LOG(LOG_WARN, "Ignoring no_media_timer value as it's not a positive integer\n");
		} else {
			janus_set_no_media_timer(nmt);
		}
	}

	printf("Setup OpenSSL stuff\n");
	const char* server_pem;
	item = janus_config_get_item_drilldown(config, "certificates", "cert_pem");
	if(!item || !item->value) {
		server_pem = NULL;
	} else {
		server_pem = item->value;
	}

	const char* server_key;
	item = janus_config_get_item_drilldown(config, "certificates", "cert_key");
	if(!item || !item->value) {
		server_key = NULL;
	} else {
		server_key = item->value;
	}
	JANUS_LOG(LOG_VERB, "Using certificates:\n\t%s\n\t%s\n", server_pem, server_key);

	SSL_library_init();
	SSL_load_error_strings();
	OpenSSL_add_all_algorithms();
	// ... and DTLS-SRTP in particular 
	if(janus_dtls_srtp_init(server_pem, server_key) < 0) {
		exit(1);
	}
	// Check if there's any custom value for the starting MTU to use in the BIO filter 
	item = janus_config_get_item_drilldown(config, "media", "dtls_mtu");
	if(item && item->value)
		janus_dtls_bio_filter_set_mtu(atoi(item->value));

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
exit(0);}