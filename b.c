
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
#include "cmdline.h"
#include "config.h"
#include "apierror.h"
#include "log.h"
#include "debug.h"
#include "ip-utils.h"
#include "rtcp.h"
#include "auth.h"
#include "record.h"
#include "events.h"
#include "transports/transport.h"


#define JANUS_NAME				"Janus WebRTC Gateway"
#define JANUS_AUTHOR			"Meetecho s.r.l."
#define JANUS_VERSION			25
#define JANUS_VERSION_STRING	"0.2.5"
#define JANUS_SERVER_NAME		"MyJanusInstance"

#ifdef __MACH__
#define SHLIB_EXT "0.dylib"
#else
#define SHLIB_EXT ".so"
#endif


static janus_config *config = NULL;
static char *config_file = NULL;
static char *configs_folder = NULL;

static GHashTable *transports = NULL;
static GHashTable *transports_so = NULL;

static GHashTable *eventhandlers = NULL;
static GHashTable *eventhandlers_so = NULL;

static GHashTable *plugins = NULL;
static GHashTable *plugins_so = NULL;


/* Daemonization */
static gboolean daemonize = FALSE;
static int pipefd[2];

static char *api_secret = NULL, *admin_api_secret = NULL;


/* IP addresses */
static gchar *local_ip = NULL;
gchar *janus_get_local_ip(void) {
	return local_ip;
}
static gchar *public_ip = NULL;
gchar *janus_get_public_ip(void) {
	/* Fallback to the local IP, if we have no public one */
	return public_ip ? public_ip : local_ip;
}
void janus_set_public_ip(const char *ip) {
	/* once set do not override */
	if(ip == NULL || public_ip != NULL)
		return;
	public_ip = g_strdup(ip);
}
static volatile gint stop = 0;
static gint stop_signal = 0;
gint janus_is_stopping(void) {
	return g_atomic_int_get(&stop);
}

static gchar *server_name = NULL;
// For what Lorenzo you have those polls for keep alive implemented??? When can one via socket all signals handle Дебила, блядь.
// Нахуй надо было так код усложнять, когда все современные браузеры вебсокет поддерживают? 
// Я блядь чуть не выебся , когда код в janus_process_incoming_request() читал.
// Блядь, чо за дебилы блядь под webrtc иосокеты всякие тащут и аяксы всякие. Под ie-6  блядь чтобы работало?
#define DEFAULT_SESSION_TIMEOUT		60
static uint session_timeout = DEFAULT_SESSION_TIMEOUT;
int janus_log_level = LOG_INFO;
gboolean janus_log_timestamps = FALSE;
gboolean janus_log_colors = FALSE;
int lock_debug = 0;


/*! \brief Signal handler (just used to intercept CTRL+C and SIGTERM) */
static void janus_handle_signal(int signum) {
	stop_signal = signum;
	switch(g_atomic_int_get(&stop)) {
		case 0:
			JANUS_PRINT("Stopping gateway, please wait...\n");
			break;
		case 1:
			JANUS_PRINT("In a hurry? I'm trying to free resources cleanly, here!\n");
			break;
		default:
			JANUS_PRINT("Ok, leaving immediately...\n");
			break;
	}
	g_atomic_int_inc(&stop);
	if(g_atomic_int_get(&stop) > 2)
		exit(1);
}

/*! \brief Termination handler (atexit) */
static void janus_termination_handler(void) {
	/* Free the instance name, if provided */
	g_free(server_name);
	/* Remove the PID file if we created it */
	janus_pidfile_remove();
	/* Close the logger */
	janus_log_destroy();
	/* If we're daemonizing, we send an error code to the parent */
	if(daemonize) {
		int code = 1;
		ssize_t res = 0;
		do {
			res = write(pipefd[1], &code, sizeof(int));
		} while(res == -1 && errno == EINTR);
	}
}


/** @name Transport plugin callback interface
 * These are the callbacks implemented by the gateway core, as part of
 * the janus_transport_callbacks interface. Everything the transport
 * plugins send the gateway is handled here.
 */
///@{
void janus_transport_incoming_request(janus_transport *plugin, void *transport, 
void *request_id, gboolean admin, json_t *message, json_error_t *error);
void janus_transport_gone(janus_transport *plugin, void *transport);
gboolean janus_transport_is_api_secret_needed(janus_transport *plugin);
gboolean janus_transport_is_api_secret_valid(janus_transport *plugin, const char *apisecret);
gboolean janus_transport_is_auth_token_needed(janus_transport *plugin);
gboolean janus_transport_is_auth_token_valid(janus_transport *plugin, const char *token);
void janus_transport_notify_event(janus_transport *plugin, void *transport, json_t *event);




static gint initialized = 0, stopping = 0;
static janus_transport_callbacks *gateway = NULL;
static gboolean ws_janus_api_enabled = FALSE;
static gboolean ws_admin_api_enabled = FALSE;
static gboolean notify_events = TRUE;

static size_t json_format = JSON_INDENT(3) | JSON_PRESERVE_ORDER;








//GThreadPool *tasks = NULL;
//void janus_transport_task(gpointer data, gpointer user_data);




/** @name Plugin callback interface
 * These are the callbacks implemented by the gateway core, as part of
 * the janus_callbacks interface. Everything the plugins send the
 * gateway is handled here.
 */
///@{
int janus_plugin_push_event(janus_plugin_session *plugin_session, janus_plugin *plugin, const char *transaction, json_t *message, json_t *jsep);
json_t *janus_plugin_handle_sdp(janus_plugin_session *plugin_session, janus_plugin *plugin, const char *sdp_type, const char *sdp);
void janus_plugin_relay_rtp(janus_plugin_session *plugin_session, int video, char *buf, int len);
void janus_plugin_relay_rtcp(janus_plugin_session *plugin_session, int video, char *buf, int len);
void janus_plugin_relay_data(janus_plugin_session *plugin_session, char *buf, int len);
void janus_plugin_close_pc(janus_plugin_session *plugin_session);
void janus_plugin_end_session(janus_plugin_session *plugin_session);
void janus_plugin_notify_event(janus_plugin *plugin, janus_plugin_session *plugin_session, json_t *event);
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




static janus_mutex sessions_mutex;
static GHashTable *sessions = NULL, *old_sessions = NULL;
static GMainContext *sessions_watchdog_context = NULL;


static gboolean janus_cleanup_session(gpointer user_data) {
	janus_session *session = (janus_session *) user_data;

	JANUS_LOG(LOG_INFO, "Cleaning up session %"SCNu64"...\n", session->session_id);
	janus_session_destroy(session->session_id);

	return G_SOURCE_REMOVE;
}

static void janus_session_schedule_destruction(janus_session *session,
		gboolean lock_sessions, gboolean remove_key, gboolean notify_transport) {
	if(session == NULL || !g_atomic_int_compare_and_exchange(&session->destroy, 0, 1))
		return;
	if(lock_sessions)
		janus_mutex_lock(&sessions_mutex);
	/* Schedule the session for deletion */
	janus_mutex_lock(&session->mutex);
	/* Remove all handles */
	if(session->ice_handles != NULL && g_hash_table_size(session->ice_handles) > 0) {
		GHashTableIter iter;
		gpointer value;
		g_hash_table_iter_init(&iter, session->ice_handles);
		while(g_hash_table_iter_next(&iter, NULL, &value)) {
			janus_ice_handle *h = value;
			if(!h || g_atomic_int_get(&stop)) {
				continue;
			}
			janus_ice_handle_destroy(session, h->handle_id);
			g_hash_table_iter_remove(&iter);
		}
	}
	janus_mutex_unlock(&session->mutex);
	if(remove_key)
		g_hash_table_remove(sessions, &session->session_id);
	g_hash_table_replace(old_sessions, janus_uint64_dup(session->session_id), session);
	GSource *timeout_source = g_timeout_source_new_seconds(3);
	g_source_set_callback(timeout_source, janus_cleanup_session, session, NULL);
	g_source_attach(timeout_source, sessions_watchdog_context);
	g_source_unref(timeout_source);
	if(lock_sessions)
		janus_mutex_unlock(&sessions_mutex);
	/* Notify the source that the session has been destroyed */
	if(notify_transport && session->source && session->source->transport)
		session->source->transport->session_over(session->source->instance, session->session_id, FALSE);
}

static gboolean janus_check_sessions(gpointer user_data) {
	if(session_timeout < 1)		/* Session timeouts are disabled */
		return G_SOURCE_CONTINUE;
	janus_mutex_lock(&sessions_mutex);
	if(sessions && g_hash_table_size(sessions) > 0) {
		GHashTableIter iter;
		gpointer value;
		g_hash_table_iter_init(&iter, sessions);
		while (g_hash_table_iter_next(&iter, NULL, &value)) {
			janus_session *session = (janus_session *) value;
			if (!session || g_atomic_int_get(&session->destroy)) {
				continue;
			}
			gint64 now = janus_get_monotonic_time();
			if (now - session->last_activity >= (gint64)session_timeout * G_USEC_PER_SEC &&
					!g_atomic_int_compare_and_exchange(&session->timeout, 0, 1)) {
				JANUS_LOG(LOG_INFO, "Timeout expired for session %"SCNu64"...\n", session->session_id);
				/* Mark the session as over, we'll deal with it later */
				janus_session_schedule_destruction(session, FALSE, FALSE, FALSE);
				/* Notify the transport */
				if(session->source) {
					json_t *event = json_object();
					json_object_set_new(event, "janus", json_string("timeout"));
					json_object_set_new(event, "session_id", json_integer(session->session_id));
					/* Send this to the transport client */
					session->source->transport->send_message(session->source->instance, NULL, FALSE, event);
					/* Notify the transport plugin about the session timeout */
					session->source->transport->session_over(session->source->instance, session->session_id, TRUE);
				}
				/* Notify event handlers as well */
				if(janus_events_is_enabled())
					janus_events_notify_handlers(JANUS_EVENT_TYPE_SESSION, session->session_id, "timeout", NULL);

				/* FIXME Is this safe? apparently it causes hash table errors on the console */
				g_hash_table_iter_remove(&iter);
				g_hash_table_replace(old_sessions, janus_uint64_dup(session->session_id), session);
			}
		}
	}
	janus_mutex_unlock(&sessions_mutex);

	return G_SOURCE_CONTINUE;
}

static gpointer janus_sessions_watchdog(gpointer user_data) {
	GMainLoop *loop = (GMainLoop *) user_data;
	GMainContext *watchdog_context = g_main_loop_get_context(loop);
	GSource *timeout_source;

	timeout_source = g_timeout_source_new_seconds(2);
	g_source_set_callback(timeout_source, janus_check_sessions, watchdog_context, NULL);
	g_source_attach(timeout_source, watchdog_context);
	g_source_unref(timeout_source);

	JANUS_LOG(LOG_INFO, "Sessions watchdog started\n");

	g_main_loop_run(loop);

	return NULL;
}

janus_session *janus_session_create(guint64 session_id) {
	if(session_id == 0) {
		while(session_id == 0) {
			session_id = janus_random_uint64();
			if(janus_session_find(session_id) != NULL) {
				/* Session ID already taken, try another one */
				session_id = 0;
			}
		}
	}
	JANUS_LOG(LOG_INFO, "Creating new session: %"SCNu64"\n", session_id);
	janus_session *session = (janus_session *)g_malloc0(sizeof(janus_session));
	if(session == NULL) {
		JANUS_LOG(LOG_FATAL, "Memory error!\n");
		return NULL;
	}
	session->session_id = session_id;
	session->source = NULL;
	g_atomic_int_set(&session->destroy, 0);
	g_atomic_int_set(&session->timeout, 0);
	session->last_activity = janus_get_monotonic_time();
	janus_mutex_init(&session->mutex);
	janus_mutex_lock(&sessions_mutex);
	g_hash_table_insert(sessions, janus_uint64_dup(session->session_id), session);
	janus_mutex_unlock(&sessions_mutex);
	return session;
}

janus_session *janus_session_find(guint64 session_id) {
	janus_mutex_lock(&sessions_mutex);
	janus_session *session = g_hash_table_lookup(sessions, &session_id);
	janus_mutex_unlock(&sessions_mutex);
	return session;
}

janus_session *janus_session_find_destroyed(guint64 session_id) {
	janus_mutex_lock(&sessions_mutex);
	janus_session *session = g_hash_table_lookup(old_sessions, &session_id);
	g_hash_table_remove(old_sessions, &session_id);
	janus_mutex_unlock(&sessions_mutex);
	return session;
}

/*void janus_session_notify_event(janus_session *session, json_t *event) {
	if(session != NULL && !g_atomic_int_get(&session->destroy) && session->source != NULL && session->source->transport != NULL) {
		/* Send this to the transport client */
		JANUS_LOG(LOG_HUGE, "Sending event to %s (%p)\n", session->source->transport->get_package(), session->source->instance);
		session->source->transport->send_message(session->source->instance, NULL, FALSE, event);
	} else {
		/* No transport, free the event */
		json_decref(event);
	}
} */


/* Destroys a session but does not remove it from the sessions hash table. */
gint janus_session_destroy(guint64 session_id) {
	janus_session *session = janus_session_find_destroyed(session_id);
	if(session == NULL) {
		JANUS_LOG(LOG_ERR, "Couldn't find session to destroy: %"SCNu64"\n", session_id);
		return -1;
	}
	JANUS_LOG(LOG_VERB, "Destroying session %"SCNu64"\n", session_id);

	/* FIXME Actually destroy session */
	janus_session_free(session);

	return 0;
}

void janus_session_free(janus_session *session) {
	if(session == NULL)
		return;
	janus_mutex_lock(&session->mutex);
	if(session->ice_handles != NULL) {
		g_hash_table_destroy(session->ice_handles);
		session->ice_handles = NULL;
	}
	if(session->source != NULL) {
		janus_request_destroy(session->source);
		session->source = NULL;
	}
	janus_mutex_unlock(&session->mutex);
	g_free(session);
	session = NULL;
}


/* 
janus_request *janus_request_new(janus_transport *transport, void *instance, void *request_id, gboolean admin, json_t *message) {
	janus_request *request = (janus_request *)g_malloc0(sizeof(janus_request));
	request->transport = transport;
	request->instance = instance;
	request->request_id = request_id;
	request->admin = admin;
	request->message = message;
	return request;
}
*/
/*
void janus_request_destroy(janus_request *request) {
	if(request == NULL)
		return;
	request->transport = NULL;
	request->instance = NULL;
	request->request_id = NULL;
	if(request->message)
		json_decref(request->message);
	request->message = NULL;
	g_free(request);
}
*/







void janus_plugin_close(gpointer key, gpointer value, gpointer user_data) {
	janus_plugin *plugin = (janus_plugin *)value;
	if(!plugin)
		return;
	plugin->destroy();
}



janus_plugin *janus_plugin_find(const gchar *package) {
	if(package != NULL && plugins != NULL)	/* FIXME Do we need to fix the key pointer? */
		return g_hash_table_lookup(plugins, package);
	return NULL;
}


/* Plugin callback interface */
int janus_plugin_push_event(janus_plugin_session *plugin_session, janus_plugin *plugin, const char *transaction, json_t *message, json_t *jsep) {
	if(!plugin || !message)
		return -1;
	if(!plugin_session || plugin_session < (janus_plugin_session *)0x1000 ||
			!janus_plugin_session_is_alive(plugin_session) || plugin_session->stopped)
		return -2;
	janus_ice_handle *ice_handle = (janus_ice_handle *)plugin_session->gateway_handle;
	if(!ice_handle || janus_flags_is_set(&ice_handle->webrtc_flags, JANUS_ICE_HANDLE_WEBRTC_STOP))
		return JANUS_ERROR_SESSION_NOT_FOUND;
	janus_session *session = ice_handle->session;
	if(!session || g_atomic_int_get(&session->destroy))
		return JANUS_ERROR_SESSION_NOT_FOUND;
	/* Make sure this is a JSON object */
	if(!json_is_object(message)) {
		JANUS_LOG(LOG_ERR, "[%"SCNu64"] Cannot push event (JSON error: not an object)\n", ice_handle->handle_id);
		return JANUS_ERROR_INVALID_JSON_OBJECT;
	}
	/* Attach JSEP if possible? */
	const char *sdp_type = json_string_value(json_object_get(jsep, "type"));
	const char *sdp = json_string_value(json_object_get(jsep, "sdp"));
	json_t *merged_jsep = NULL;
	if(sdp_type != NULL && sdp != NULL) {
		merged_jsep = janus_plugin_handle_sdp(plugin_session, plugin, sdp_type, sdp);
		if(merged_jsep == NULL) {
			if(ice_handle == NULL || janus_flags_is_set(&ice_handle->webrtc_flags, JANUS_ICE_HANDLE_WEBRTC_STOP)
					|| janus_flags_is_set(&ice_handle->webrtc_flags, JANUS_ICE_HANDLE_WEBRTC_ALERT)) {
				JANUS_LOG(LOG_ERR, "[%"SCNu64"] Cannot push event (handle not available anymore or negotiation stopped)\n", ice_handle->handle_id);
				return JANUS_ERROR_HANDLE_NOT_FOUND;
			} else {
				JANUS_LOG(LOG_ERR, "[%"SCNu64"] Cannot push event (JSON error: problem with the SDP)\n", ice_handle->handle_id);
				return JANUS_ERROR_JSEP_INVALID_SDP;
			}
		}
	}
	/* Reference the payload, as the plugin may still need it and will do a decref itself */
	json_incref(message);
	/* Prepare JSON event */
	json_t *event = json_object();
	json_object_set_new(event, "janus", json_string("event"));
	json_object_set_new(event, "session_id", json_integer(session->session_id));
	json_object_set_new(event, "sender", json_integer(ice_handle->handle_id));
	if(transaction != NULL)
		json_object_set_new(event, "transaction", json_string(transaction));
	json_t *plugin_data = json_object();
	json_object_set_new(plugin_data, "plugin", json_string(plugin->get_package()));
	json_object_set_new(plugin_data, "data", message);
	json_object_set_new(event, "plugindata", plugin_data);
	if(merged_jsep != NULL)
		json_object_set_new(event, "jsep", merged_jsep);
	/* Send the event */
	JANUS_LOG(LOG_VERB, "[%"SCNu64"] Sending event to transport...\n", ice_handle->handle_id);
	janus_session_notify_event(session, event);

	if(jsep != NULL && janus_events_is_enabled()) {
		/* Notify event handlers as well */
		janus_events_notify_handlers(JANUS_EVENT_TYPE_JSEP,
			session->session_id, ice_handle->handle_id, "local", sdp_type, sdp);
	}

	return JANUS_OK;
}

json_t *janus_plugin_handle_sdp(janus_plugin_session *plugin_session, janus_plugin *plugin, const char *sdp_type, const char *sdp) {
	if(!plugin_session || plugin_session < (janus_plugin_session *)0x1000 ||
			!janus_plugin_session_is_alive(plugin_session) || plugin_session->stopped ||
			plugin == NULL || sdp_type == NULL || sdp == NULL) {
		JANUS_LOG(LOG_ERR, "Invalid arguments\n");
		return NULL;
	}
	janus_ice_handle *ice_handle = (janus_ice_handle *)plugin_session->gateway_handle;
	//~ if(ice_handle == NULL || janus_flags_is_set(&ice_handle->webrtc_flags, JANUS_ICE_HANDLE_WEBRTC_READY)) {
	if(ice_handle == NULL) {
		JANUS_LOG(LOG_ERR, "Invalid ICE handle\n");
		return NULL;
	}
	int offer = 0;
	if(!strcasecmp(sdp_type, "offer")) {
		/* This is an offer from a plugin */
		offer = 1;
		janus_flags_set(&ice_handle->webrtc_flags, JANUS_ICE_HANDLE_WEBRTC_GOT_OFFER);
		janus_flags_clear(&ice_handle->webrtc_flags, JANUS_ICE_HANDLE_WEBRTC_GOT_ANSWER);
	} else if(!strcasecmp(sdp_type, "answer")) {
		/* This is an answer from a plugin */
		janus_flags_set(&ice_handle->webrtc_flags, JANUS_ICE_HANDLE_WEBRTC_GOT_ANSWER);
	} else {
		/* TODO Handle other messages */
		JANUS_LOG(LOG_ERR, "Unknown type '%s'\n", sdp_type);
		return NULL;
	}
	/* Is this valid SDP? */
	char error_str[512];
	int audio = 0, video = 0, data = 0, bundle = 0, rtcpmux = 0, trickle = 0;
	janus_sdp *parsed_sdp = janus_sdp_preparse(sdp, error_str, sizeof(error_str), &audio, &video, &data, &bundle, &rtcpmux, &trickle);
	if(parsed_sdp == NULL) {
		JANUS_LOG(LOG_ERR, "[%"SCNu64"] Couldn't parse SDP... %s\n", ice_handle->handle_id, error_str);
		return NULL;
	}
	gboolean updating = FALSE;
	if(offer) {
		/* We still don't have a local ICE setup */
		JANUS_LOG(LOG_VERB, "[%"SCNu64"] Audio %s been negotiated\n", ice_handle->handle_id, audio ? "has" : "has NOT");
		if(audio > 1) {
			JANUS_LOG(LOG_ERR, "[%"SCNu64"] More than one audio line? only going to negotiate one...\n", ice_handle->handle_id);
		}
		JANUS_LOG(LOG_VERB, "[%"SCNu64"] Video %s been negotiated\n", ice_handle->handle_id, video ? "has" : "has NOT");
		if(video > 1) {
			JANUS_LOG(LOG_ERR, "[%"SCNu64"] More than one video line? only going to negotiate one...\n", ice_handle->handle_id);
		}
		JANUS_LOG(LOG_VERB, "[%"SCNu64"] SCTP/DataChannels %s been negotiated\n", ice_handle->handle_id, data ? "have" : "have NOT");
		if(data > 1) {
			JANUS_LOG(LOG_ERR, "[%"SCNu64"] More than one data line? only going to negotiate one...\n", ice_handle->handle_id);
		}
#ifndef HAVE_SCTP
		if(data) {
			JANUS_LOG(LOG_WARN, "[%"SCNu64"]   -- DataChannels have been negotiated, but support for them has not been compiled...\n", ice_handle->handle_id);
		}
#endif
		/* Are we still cleaning up from a previous media session? */
		if(janus_flags_is_set(&ice_handle->webrtc_flags, JANUS_ICE_HANDLE_WEBRTC_CLEANING)) {
			JANUS_LOG(LOG_VERB, "[%"SCNu64"] Still cleaning up from a previous media session, let's wait a bit...\n", ice_handle->handle_id);
			gint64 waited = 0;
			while(janus_flags_is_set(&ice_handle->webrtc_flags, JANUS_ICE_HANDLE_WEBRTC_CLEANING)) {
				JANUS_LOG(LOG_VERB, "[%"SCNu64"] Still cleaning up from a previous media session, let's wait a bit...\n", ice_handle->handle_id);
				g_usleep(100000);
				waited += 100000;
				if(waited >= 3*G_USEC_PER_SEC) {
					JANUS_LOG(LOG_VERB, "[%"SCNu64"]   -- Waited 3 seconds, that's enough!\n", ice_handle->handle_id);
					break;
				}
			}
		}
		if(ice_handle->agent == NULL) {
			/* Process SDP in order to setup ICE locally (this is going to result in an answer from the browser) */
			if(janus_ice_setup_local(ice_handle, 0, audio, video, data, bundle, rtcpmux, trickle) < 0) {
				JANUS_LOG(LOG_ERR, "[%"SCNu64"] Error setting ICE locally\n", ice_handle->handle_id);
				janus_sdp_free(parsed_sdp);
				return NULL;
			}
		} else {
			updating = TRUE;
			JANUS_LOG(LOG_INFO, "[%"SCNu64"] Updating existing session\n", ice_handle->handle_id);
		}
	}
	if(!updating) {
		/* Wait for candidates-done callback */
		while(ice_handle->cdone < ice_handle->streams_num) {
			if(ice_handle == NULL || janus_flags_is_set(&ice_handle->webrtc_flags, JANUS_ICE_HANDLE_WEBRTC_STOP)
					|| janus_flags_is_set(&ice_handle->webrtc_flags, JANUS_ICE_HANDLE_WEBRTC_ALERT)) {
				JANUS_LOG(LOG_WARN, "[%"SCNu64"] Handle detached or PC closed, giving up...!\n", ice_handle ? ice_handle->handle_id : 0);
				janus_sdp_free(parsed_sdp);
				return NULL;
			}
			JANUS_LOG(LOG_VERB, "[%"SCNu64"] Waiting for candidates-done callback...\n", ice_handle->handle_id);
			g_usleep(100000);
			if(ice_handle->cdone < 0) {
				JANUS_LOG(LOG_ERR, "[%"SCNu64"] Error gathering candidates!\n", ice_handle->handle_id);
				janus_sdp_free(parsed_sdp);
				return NULL;
			}
		}
	}
	/* Anonymize SDP */
	if(janus_sdp_anonymize(parsed_sdp) < 0) {
		/* Invalid SDP */
		JANUS_LOG(LOG_ERR, "[%"SCNu64"] Invalid SDP\n", ice_handle->handle_id);
		janus_sdp_free(parsed_sdp);
		return NULL;
	}
	/* Add our details */
	char *sdp_merged = janus_sdp_merge(ice_handle, parsed_sdp);
	if(sdp_merged == NULL) {
		/* Couldn't merge SDP */
		JANUS_LOG(LOG_ERR, "[%"SCNu64"] Error merging SDP\n", ice_handle->handle_id);
		janus_sdp_free(parsed_sdp);
		return NULL;
	}
	janus_sdp_free(parsed_sdp);
	/* FIXME Any disabled m-line? */
	if(strstr(sdp_merged, "m=audio 0")) {
		JANUS_LOG(LOG_VERB, "[%"SCNu64"] Audio disabled via SDP\n", ice_handle->handle_id);
		if(!janus_flags_is_set(&ice_handle->webrtc_flags, JANUS_ICE_HANDLE_WEBRTC_BUNDLE)
				|| (!video && !data)) {
			JANUS_LOG(LOG_VERB, "[%"SCNu64"]   -- Marking audio stream as disabled\n", ice_handle->handle_id);
			janus_ice_stream *stream = g_hash_table_lookup(ice_handle->streams, GUINT_TO_POINTER(ice_handle->audio_id));
			if(stream)
				stream->disabled = TRUE;
		}
	}
	if(strstr(sdp_merged, "m=video 0")) {
		JANUS_LOG(LOG_VERB, "[%"SCNu64"] Video disabled via SDP\n", ice_handle->handle_id);
		if(!janus_flags_is_set(&ice_handle->webrtc_flags, JANUS_ICE_HANDLE_WEBRTC_BUNDLE)
				|| (!audio && !data)) {
			JANUS_LOG(LOG_VERB, "[%"SCNu64"]   -- Marking video stream as disabled\n", ice_handle->handle_id);
			janus_ice_stream *stream = NULL;
			if(!janus_flags_is_set(&ice_handle->webrtc_flags, JANUS_ICE_HANDLE_WEBRTC_BUNDLE)) {
				stream = g_hash_table_lookup(ice_handle->streams, GUINT_TO_POINTER(ice_handle->video_id));
			} else {
				gint id = ice_handle->audio_id > 0 ? ice_handle->audio_id : ice_handle->video_id;
				stream = g_hash_table_lookup(ice_handle->streams, GUINT_TO_POINTER(id));
			}
			if(stream)
				stream->disabled = TRUE;
		}
	}
	if(strstr(sdp_merged, "m=application 0 DTLS/SCTP")) {
		JANUS_LOG(LOG_VERB, "[%"SCNu64"] Data Channel disabled via SDP\n", ice_handle->handle_id);
		if(!janus_flags_is_set(&ice_handle->webrtc_flags, JANUS_ICE_HANDLE_WEBRTC_BUNDLE)
				|| (!audio && !video)) {
			JANUS_LOG(LOG_VERB, "[%"SCNu64"]   -- Marking data channel stream as disabled\n", ice_handle->handle_id);
			janus_ice_stream *stream = NULL;
			if(!janus_flags_is_set(&ice_handle->webrtc_flags, JANUS_ICE_HANDLE_WEBRTC_BUNDLE)) {
				stream = g_hash_table_lookup(ice_handle->streams, GUINT_TO_POINTER(ice_handle->data_id));
			} else {
				gint id = ice_handle->audio_id > 0 ? ice_handle->audio_id : (ice_handle->video_id > 0 ? ice_handle->video_id : ice_handle->data_id);
				stream = g_hash_table_lookup(ice_handle->streams, GUINT_TO_POINTER(id));
			}
			if(stream)
				stream->disabled = TRUE;
		}
	}

	if(!updating) {
		if(offer) {
			/* We set the flag to wait for an answer before handling trickle candidates */
			janus_flags_set(&ice_handle->webrtc_flags, JANUS_ICE_HANDLE_WEBRTC_PROCESSING_OFFER);
		} else {
			JANUS_LOG(LOG_VERB, "[%"SCNu64"] Done! Ready to setup remote candidates and send connectivity checks...\n", ice_handle->handle_id);
			if(janus_flags_is_set(&ice_handle->webrtc_flags, JANUS_ICE_HANDLE_WEBRTC_BUNDLE)) {
				JANUS_LOG(LOG_VERB, "[%"SCNu64"]   -- bundle is supported by the browser, getting rid of one of the RTP/RTCP components, if any...\n", ice_handle->handle_id);
				if(audio) {
					/* Get rid of video and data, if present */
					if(ice_handle->streams && ice_handle->video_stream) {
						if(ice_handle->audio_stream->rtp_component && ice_handle->video_stream->rtp_component)
							ice_handle->audio_stream->rtp_component->do_video_nacks = ice_handle->video_stream->rtp_component->do_video_nacks;
						ice_handle->audio_stream->video_ssrc = ice_handle->video_stream->video_ssrc;
						ice_handle->audio_stream->video_ssrc_peer = ice_handle->video_stream->video_ssrc_peer;
						ice_handle->audio_stream->video_ssrc_peer_rtx = ice_handle->video_stream->video_ssrc_peer_rtx;
						ice_handle->audio_stream->video_ssrc_peer_sim_1 = ice_handle->video_stream->video_ssrc_peer_sim_1;
						ice_handle->audio_stream->video_ssrc_peer_sim_2 = ice_handle->video_stream->video_ssrc_peer_sim_2;
						nice_agent_attach_recv(ice_handle->agent, ice_handle->video_stream->stream_id, 1, g_main_loop_get_context (ice_handle->iceloop), NULL, NULL);
						if(!ice_handle->force_rtcp_mux && !janus_ice_is_rtcpmux_forced())
							nice_agent_attach_recv(ice_handle->agent, ice_handle->video_stream->stream_id, 2, g_main_loop_get_context (ice_handle->iceloop), NULL, NULL);
						nice_agent_remove_stream(ice_handle->agent, ice_handle->video_stream->stream_id);
						janus_ice_stream_free(ice_handle->streams, ice_handle->video_stream);
					}
					ice_handle->video_stream = NULL;
					ice_handle->video_id = 0;
					if(ice_handle->streams && ice_handle->data_stream) {
						nice_agent_attach_recv(ice_handle->agent, ice_handle->data_stream->stream_id, 1, g_main_loop_get_context (ice_handle->iceloop), NULL, NULL);
						nice_agent_remove_stream(ice_handle->agent, ice_handle->data_stream->stream_id);
						janus_ice_stream_free(ice_handle->streams, ice_handle->data_stream);
					}
					ice_handle->data_stream = NULL;
					ice_handle->data_id = 0;
					if(!video) {
						if(ice_handle->audio_stream->rtp_component)
							ice_handle->audio_stream->rtp_component->do_video_nacks = FALSE;
						ice_handle->audio_stream->video_ssrc = 0;
						ice_handle->audio_stream->video_ssrc_peer = 0;
						g_free(ice_handle->audio_stream->video_rtcp_ctx);
						ice_handle->audio_stream->video_rtcp_ctx = NULL;
					}
				} else if(video) {
					/* Get rid of data, if present */
					if(ice_handle->streams && ice_handle->data_stream) {
						nice_agent_attach_recv(ice_handle->agent, ice_handle->data_stream->stream_id, 1, g_main_loop_get_context (ice_handle->iceloop), NULL, NULL);
						nice_agent_remove_stream(ice_handle->agent, ice_handle->data_stream->stream_id);
						janus_ice_stream_free(ice_handle->streams, ice_handle->data_stream);
					}
					ice_handle->data_stream = NULL;
					ice_handle->data_id = 0;
				}
			}
			if(janus_flags_is_set(&ice_handle->webrtc_flags, JANUS_ICE_HANDLE_WEBRTC_RTCPMUX) && !ice_handle->force_rtcp_mux && !janus_ice_is_rtcpmux_forced()) {
				JANUS_LOG(LOG_VERB, "[%"SCNu64"]   -- rtcp-mux is supported by the browser, getting rid of RTCP components, if any...\n", ice_handle->handle_id);
				if(ice_handle->audio_stream && ice_handle->audio_stream->rtcp_component && ice_handle->audio_stream->components != NULL) {
					nice_agent_attach_recv(ice_handle->agent, ice_handle->audio_id, 2, g_main_loop_get_context (ice_handle->iceloop), NULL, NULL);
					/* Free the component */
					janus_ice_component_free(ice_handle->audio_stream->components, ice_handle->audio_stream->rtcp_component);
					ice_handle->audio_stream->rtcp_component = NULL;
					/* Create a dummy candidate and enforce it as the one to use for this now unneeded component */
					NiceCandidate *c = nice_candidate_new(NICE_CANDIDATE_TYPE_HOST);
					c->component_id = 2;
					c->stream_id = ice_handle->audio_stream->stream_id;
#ifndef HAVE_LIBNICE_TCP
					c->transport = NICE_CANDIDATE_TRANSPORT_UDP;
#endif
					strncpy(c->foundation, "1", NICE_CANDIDATE_MAX_FOUNDATION);
					c->priority = 1;
					nice_address_set_from_string(&c->addr, "127.0.0.1");
					nice_address_set_port(&c->addr, janus_ice_get_rtcpmux_blackhole_port());
					c->username = g_strdup(ice_handle->audio_stream->ruser);
					c->password = g_strdup(ice_handle->audio_stream->rpass);
					if(!nice_agent_set_selected_remote_candidate(ice_handle->agent, ice_handle->audio_stream->stream_id, 2, c)) {
						JANUS_LOG(LOG_ERR, "[%"SCNu64"] Error forcing dummy candidate on RTCP component of stream %d\n", ice_handle->handle_id, ice_handle->audio_stream->stream_id);
						nice_candidate_free(c);
					}
				}
				if(ice_handle->video_stream && ice_handle->video_stream->rtcp_component && ice_handle->video_stream->components != NULL) {
					nice_agent_attach_recv(ice_handle->agent, ice_handle->video_id, 2, g_main_loop_get_context (ice_handle->iceloop), NULL, NULL);
					/* Free the component */
					janus_ice_component_free(ice_handle->video_stream->components, ice_handle->video_stream->rtcp_component);
					ice_handle->video_stream->rtcp_component = NULL;
					/* Create a dummy candidate and enforce it as the one to use for this now unneeded component */
					NiceCandidate *c = nice_candidate_new(NICE_CANDIDATE_TYPE_HOST);
					c->component_id = 2;
					c->stream_id = ice_handle->video_stream->stream_id;
#ifndef HAVE_LIBNICE_TCP
					c->transport = NICE_CANDIDATE_TRANSPORT_UDP;
#endif
					strncpy(c->foundation, "1", NICE_CANDIDATE_MAX_FOUNDATION);
					c->priority = 1;
					nice_address_set_from_string(&c->addr, "127.0.0.1");
					nice_address_set_port(&c->addr, janus_ice_get_rtcpmux_blackhole_port());
					c->username = g_strdup(ice_handle->video_stream->ruser);
					c->password = g_strdup(ice_handle->video_stream->rpass);
					if(!nice_agent_set_selected_remote_candidate(ice_handle->agent, ice_handle->video_stream->stream_id, 2, c)) {
						JANUS_LOG(LOG_ERR, "[%"SCNu64"] Error forcing dummy candidate on RTCP component of stream %d\n", ice_handle->handle_id, ice_handle->video_stream->stream_id);
						nice_candidate_free(c);
					}
				}
			}
			janus_mutex_lock(&ice_handle->mutex);
			/* We got our answer */
			janus_flags_clear(&ice_handle->webrtc_flags, JANUS_ICE_HANDLE_WEBRTC_PROCESSING_OFFER);
			/* Any pending trickles? */
			if(ice_handle->pending_trickles) {
				JANUS_LOG(LOG_VERB, "[%"SCNu64"]   -- Processing %d pending trickle candidates\n", ice_handle->handle_id, g_list_length(ice_handle->pending_trickles));
				GList *temp = NULL;
				while(ice_handle->pending_trickles) {
					temp = g_list_first(ice_handle->pending_trickles);
					ice_handle->pending_trickles = g_list_remove_link(ice_handle->pending_trickles, temp);
					janus_ice_trickle *trickle = (janus_ice_trickle *)temp->data;
					g_list_free(temp);
					if(trickle == NULL)
						continue;
					if((janus_get_monotonic_time() - trickle->received) > 45*G_USEC_PER_SEC) {
						/* FIXME Candidate is too old, discard it */
						janus_ice_trickle_destroy(trickle);
						/* FIXME We should report that */
						continue;
					}
					json_t *candidate = trickle->candidate;
					if(candidate == NULL) {
						janus_ice_trickle_destroy(trickle);
						continue;
					}
					if(json_is_object(candidate)) {
						/* We got a single candidate */
						int error = 0;
						const char *error_string = NULL;
						if((error = janus_ice_trickle_parse(ice_handle, candidate, &error_string)) != 0) {
							/* FIXME We should report the error parsing the trickle candidate */
						}
					} else if(json_is_array(candidate)) {
						/* We got multiple candidates in an array */
						JANUS_LOG(LOG_VERB, "[%"SCNu64"] Got multiple candidates (%zu)\n", ice_handle->handle_id, json_array_size(candidate));
						if(json_array_size(candidate) > 0) {
							/* Handle remote candidates */
							size_t i = 0;
							for(i=0; i<json_array_size(candidate); i++) {
								json_t *c = json_array_get(candidate, i);
								/* FIXME We don't care if any trickle fails to parse */
								janus_ice_trickle_parse(ice_handle, c, NULL);
							}
						}
					}
					/* Done, free candidate */
					janus_ice_trickle_destroy(trickle);
				}
			}
			/* This was an answer, check if it's time to start ICE */
			if(janus_flags_is_set(&ice_handle->webrtc_flags, JANUS_ICE_HANDLE_WEBRTC_TRICKLE) &&
					!janus_flags_is_set(&ice_handle->webrtc_flags, JANUS_ICE_HANDLE_WEBRTC_ALL_TRICKLES)) {
				JANUS_LOG(LOG_VERB, "[%"SCNu64"]   -- ICE Trickling is supported by the browser, waiting for remote candidates...\n", ice_handle->handle_id);
				janus_flags_set(&ice_handle->webrtc_flags, JANUS_ICE_HANDLE_WEBRTC_START);
			} else {
				JANUS_LOG(LOG_VERB, "[%"SCNu64"] Done! Sending connectivity checks...\n", ice_handle->handle_id);
				if(ice_handle->audio_id > 0) {
					janus_ice_setup_remote_candidates(ice_handle, ice_handle->audio_id, 1);
					if(!janus_flags_is_set(&ice_handle->webrtc_flags, JANUS_ICE_HANDLE_WEBRTC_RTCPMUX))	/* http://tools.ietf.org/html/rfc5761#section-5.1.3 */
						janus_ice_setup_remote_candidates(ice_handle, ice_handle->audio_id, 2);
				}
				if(ice_handle->video_id > 0) {
					janus_ice_setup_remote_candidates(ice_handle, ice_handle->video_id, 1);
					if(!janus_flags_is_set(&ice_handle->webrtc_flags, JANUS_ICE_HANDLE_WEBRTC_RTCPMUX))	/* http://tools.ietf.org/html/rfc5761#section-5.1.3 */
						janus_ice_setup_remote_candidates(ice_handle, ice_handle->video_id, 2);
				}
				if(ice_handle->data_id > 0) {
					janus_ice_setup_remote_candidates(ice_handle, ice_handle->data_id, 1);
				}
			}
			janus_mutex_unlock(&ice_handle->mutex);
		}
	}

	/* Prepare JSON event */
	json_t *jsep = json_object();
	json_object_set_new(jsep, "type", json_string(sdp_type));
	json_object_set_new(jsep, "sdp", json_string(sdp_merged));
	ice_handle->local_sdp = sdp_merged;
	return jsep;
}

void janus_plugin_relay_rtp(janus_plugin_session *plugin_session, int video, char *buf, int len) {
	if((plugin_session < (janus_plugin_session *)0x1000) || plugin_session->stopped || buf == NULL || len < 1)
		return;
	janus_ice_handle *handle = (janus_ice_handle *)plugin_session->gateway_handle;
	if(!handle || janus_flags_is_set(&handle->webrtc_flags, JANUS_ICE_HANDLE_WEBRTC_STOP)
			|| janus_flags_is_set(&handle->webrtc_flags, JANUS_ICE_HANDLE_WEBRTC_ALERT))
		return;
	janus_ice_relay_rtp(handle, video, buf, len);
}

void janus_plugin_relay_rtcp(janus_plugin_session *plugin_session, int video, char *buf, int len) {
	if((plugin_session < (janus_plugin_session *)0x1000) || plugin_session->stopped || buf == NULL || len < 1)
		return;
	janus_ice_handle *handle = (janus_ice_handle *)plugin_session->gateway_handle;
	if(!handle || janus_flags_is_set(&handle->webrtc_flags, JANUS_ICE_HANDLE_WEBRTC_STOP)
			|| janus_flags_is_set(&handle->webrtc_flags, JANUS_ICE_HANDLE_WEBRTC_ALERT))
		return;
	janus_ice_relay_rtcp(handle, video, buf, len);
}

void janus_plugin_relay_data(janus_plugin_session *plugin_session, char *buf, int len) {
	if((plugin_session < (janus_plugin_session *)0x1000) || plugin_session->stopped || buf == NULL || len < 1)
		return;
	janus_ice_handle *handle = (janus_ice_handle *)plugin_session->gateway_handle;
	if(!handle || janus_flags_is_set(&handle->webrtc_flags, JANUS_ICE_HANDLE_WEBRTC_STOP)
			|| janus_flags_is_set(&handle->webrtc_flags, JANUS_ICE_HANDLE_WEBRTC_ALERT))
		return;
#ifdef HAVE_SCTP
	janus_ice_relay_data(handle, buf, len);
#else
	JANUS_LOG(LOG_WARN, "Asked to relay data, but Data Channels support has not been compiled...\n");
#endif
}

void janus_plugin_close_pc(janus_plugin_session *plugin_session) {
	/* A plugin asked to get rid of a PeerConnection */
	if((plugin_session < (janus_plugin_session *)0x1000) || !janus_plugin_session_is_alive(plugin_session) || plugin_session->stopped)
		return;
	janus_ice_handle *ice_handle = (janus_ice_handle *)plugin_session->gateway_handle;
	if(!ice_handle)
		return;
	if(janus_flags_is_set(&ice_handle->webrtc_flags, JANUS_ICE_HANDLE_WEBRTC_STOP)
			|| janus_flags_is_set(&ice_handle->webrtc_flags, JANUS_ICE_HANDLE_WEBRTC_ALERT))
		return;
	janus_session *session = (janus_session *)ice_handle->session;
	if(!session)
		return;

	JANUS_LOG(LOG_VERB, "[%"SCNu64"] Plugin asked to hangup PeerConnection: sending alert\n", ice_handle->handle_id);
	/* Send an alert on all the DTLS connections */
	janus_ice_webrtc_hangup(ice_handle, "Close PC");
}

void janus_plugin_end_session(janus_plugin_session *plugin_session) {
	/* A plugin asked to get rid of a handle */
	if((plugin_session < (janus_plugin_session *)0x1000) || !janus_plugin_session_is_alive(plugin_session) || plugin_session->stopped)
		return;
	janus_ice_handle *ice_handle = (janus_ice_handle *)plugin_session->gateway_handle;
	if(!ice_handle)
		return;
	janus_session *session = (janus_session *)ice_handle->session;
	if(!session)
		return;
	/* Destroy the handle */
	janus_mutex_lock(&session->mutex);
	janus_ice_handle_destroy(session, ice_handle->handle_id);
	g_hash_table_remove(session->ice_handles, &ice_handle->handle_id);
	janus_mutex_unlock(&session->mutex);
}

void janus_plugin_notify_event(janus_plugin *plugin, janus_plugin_session *plugin_session, json_t *event) {

	if(!plugin || !event || !json_is_object(event))
		return;
	guint64 session_id = 0, handle_id = 0;
	if(plugin_session != NULL) {
		if((plugin_session < (janus_plugin_session *)0x1000) || !janus_plugin_session_is_alive(plugin_session) || plugin_session->stopped) {
			json_decref(event);
			return;
		}
		janus_ice_handle *ice_handle = (janus_ice_handle *)plugin_session->gateway_handle;
		if(!ice_handle) {
			json_decref(event);
			return;
		}
		handle_id = ice_handle->handle_id;
		janus_session *session = (janus_session *)ice_handle->session;
		if(!session) {
			json_decref(event);
			return;
		}
		session_id = session->session_id;
	}

	if(janus_events_is_enabled()) {
		janus_events_notify_handlers(JANUS_EVENT_TYPE_PLUGIN,
			session_id, handle_id, plugin->get_package(), event);
	} else {
		json_decref(event);
	}
}


/* Main */
gint main(int argc, char *argv[])
{
	/* Core dumps may be disallowed by parent of this process; change that */
	struct rlimit core_limits;
	core_limits.rlim_cur = core_limits.rlim_max = RLIM_INFINITY;
	setrlimit(RLIMIT_CORE, &core_limits);

	g_print("Janus commit: %s\n", janus_build_git_sha);
	g_print("Compiled on:  %s\n\n", janus_build_git_time);

	struct gengetopt_args_info args_info;
	

	gboolean use_stdout = TRUE;
	g_print("after use_std_out\n");
	if(janus_log_init(0,use_stdout,"logfile")<0){g_print("no logfile error\n");exit(1);}
	JANUS_PRINT("---------------------------------------------------\n");
	g_print("  Starting Meetecho Janus (WebRTC Gateway) v%s\n", JANUS_VERSION_STRING);
	JANUS_PRINT("---------------------------------------------------\n\n");

	/* Handle SIGINT (CTRL-C), SIGTERM (from service managers) */
	signal(SIGINT, janus_handle_signal);
	signal(SIGTERM, janus_handle_signal);
	atexit(janus_termination_handler);

	g_print(" Setup Glib \n");
	
#if !GLIB_CHECK_VERSION(2, 36, 0)
	g_type_init();
#endif

	g_print(" Logging level: default is info and no timestamps \n");
	janus_log_level = 7;//LOG_INFO;
	janus_log_timestamps = TRUE;
	janus_log_colors = TRUE;
	if(args_info.debug_level_given) {
		if(args_info.debug_level_arg < LOG_NONE)
			args_info.debug_level_arg = 0;
		else if(args_info.debug_level_arg > LOG_MAX)
			args_info.debug_level_arg = LOG_MAX;
		janus_log_level = args_info.debug_level_arg;
	}

	
	

	
	/* What is the local IP? */
	g_print("Selecting local IP address...\n");
	
	if(local_ip == NULL) {
		local_ip = janus_network_detect_local_ip_as_string(janus_network_query_options_any_ip);
		if(local_ip == NULL) {
			g_print("Couldn't find any address! using 127.0.0.1 as the local IP... (which is NOT going to work out of your machine)\n");
			local_ip = g_strdup("127.0.0.1");
		}
	}
	g_print("Using %s as local IP...\n", local_ip);

	/* Is there any API secret to consider? */
	api_secret = NULL;

	/* Setup ICE stuff (e.g., checking if the provided STUN server is correct) */
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
	/* Check if we need to enable ICE-TCP support (warning: still broken, for debugging only) */
item = janus_config_get_item_drilldown(config, "nat", "ice_tcp");
	ice_tcp = (item && item->value) ? janus_is_true(item->value) : FALSE;
	/* Any STUN server to use in Janus? */
	item = janus_config_get_item_drilldown(config, "nat", "stun_server");
	if(item && item->value)
		stun_server = (char *)item->value;
	item = janus_config_get_item_drilldown(config, "nat", "stun_port");
	if(item && item->value)
		stun_port = atoi(item->value);
	/* Any 1:1 NAT mapping to take into account? */
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
	/* Any TURN server to use in Janus? */
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
	/* Initialize the ICE stack now */
	
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
		/* No STUN and TURN server provided for Janus: make sure it isn't on a private address */
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
					/* Class A private address */
					private_address = TRUE;
				} else if(ip[0] == 172 && (ip[1] >= 16 && ip[1] <= 31)) {
					/* Class B private address */
					private_address = TRUE;
				} else if(ip[0] == 192 && ip[1] == 168) {
					/* Class C private address */
					private_address = TRUE;
				}
			} else {
				/* TODO Similar check for IPv6... */
			}
		}
		if(private_address) {
			JANUS_LOG(LOG_WARN, "Janus is deployed on a private address (%s) but you didn't specify any STUN server!"
			                    " Expect trouble if this is supposed to work over the internet and not just in a LAN...\n", test_ip);
		}
	}
	/* Are we going to force BUNDLE and/or rtcp-mux? */
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
	/* no-media timer */
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
	/* ... and DTLS-SRTP in particular */
	if(janus_dtls_srtp_init(server_pem, server_key) < 0) {
		exit(1);
	}
	/* Check if there's any custom value for the starting MTU to use in the BIO filter */
	item = janus_config_get_item_drilldown(config, "media", "dtls_mtu");
	if(item && item->value)
		janus_dtls_bio_filter_set_mtu(atoi(item->value));

#ifdef HAVE_SCTP
	/* Initialize SCTP for DataChannels */
	if(janus_sctp_init() < 0) {
		exit(1);
	}else{g_print("janus_sctp_init+++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++ OK\n");}
#else
	JANUS_LOG(LOG_WARN, "Data Channels support not compiled\n");
#endif

	/* Sessions */
	sessions = g_hash_table_new_full(g_int64_hash, g_int64_equal, (GDestroyNotify)g_free, NULL);
	old_sessions = g_hash_table_new_full(g_int64_hash, g_int64_equal, (GDestroyNotify)g_free, NULL);
	janus_mutex_init(&sessions_mutex);
	/* Start the sessions watchdog */
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




	gboolean janus_api_enabled = FALSE, admin_api_enabled = FALSE;
	

	
	
//	janus_transport_incoming_request(janus_transport, NULL, 200, 0, NULL, NULL) ;
	json_t *repl=json_object();
	json_object_set_new(repl,"type",json_string("offer"));
	json_object_set_new(repl,"janus",json_string("janus"));
	json_object_set_new(repl,"jsep",json_string("58"));
	janus_process_incoming_request2(repl);
	json_decref(repl);
	/*
	"transaction", JSON_STRING, JANUS_JSON_PARAM_REQUIRED},
	{"janus", JSON_STRING, JANUS_JSON_PARAM_REQUIRED},
	{"id"
	*/
	
	
	
	
	
	
	
	
	
	
	
	
	
	
	
	
	
	
	
	
	

	while(!g_atomic_int_get(&stop)) {
	//	printf("Loop until we have to stop\n");
		usleep(250000); /* A signal will cancel usleep() but not g_usleep() */
	}

	
	/* Done */
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
	g_thread_pool_free(tasks, FALSE, FALSE);

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

	JANUS_LOG(LOG_INFO, "Closing plugins:\n");
	if(plugins != NULL) {
		g_hash_table_foreach(plugins, janus_plugin_close, NULL);
		g_hash_table_destroy(plugins);
	}
	if(plugins_so != NULL) {
		g_hash_table_foreach(plugins_so, janus_pluginso_close, NULL);
		g_hash_table_destroy(plugins_so);
	}

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
}
