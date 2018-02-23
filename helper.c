//#include <stdlib.h> //exit
#include "helper.h"

//static 
	void janus_handle_signal(int signum) {
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

//static
	void janus_termination_handler(void) {
	//Free the instance name, if provided 
	g_free(server_name);
	// Remove the PID file if we created it 
	janus_pidfile_remove();
	// Close the logger 
	janus_log_destroy();
	// If we're daemonizing, we send an error code to the parent 
	if(daemonize) {
		int code = 1;
		ssize_t res = 0;
		do {
			res = write(pipefd[1], &code, sizeof(int));
		} while(res == -1 && errno == EINTR);
	}
}

gint janus_is_stopping(void) {
	return g_atomic_int_get(&stop);
}

//static 
	gboolean janus_cleanup_session(gpointer user_data) {
	janus_session *session = (janus_session *) user_data;

	JANUS_LOG(LOG_INFO, "Cleaning up session %"SCNu64"...\n", session->session_id);
	janus_session_destroy(session->session_id);

	return G_SOURCE_REMOVE;
}

janus_session *janus_session_find_destroyed(guint64 session_id) {
	janus_mutex_lock(&sessions_mutex);
	janus_session *session = g_hash_table_lookup(old_sessions, &session_id);
	g_hash_table_remove(old_sessions, &session_id);
	janus_mutex_unlock(&sessions_mutex);
	return session;
}

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
		//janus_request_destroy(session->source);
		session->source = NULL;
	}
	janus_mutex_unlock(&session->mutex);
	g_free(session);
	session = NULL;
}

janus_session *janus_session_find(guint64 session_id) {
	janus_mutex_lock(&sessions_mutex);
	janus_session *session = g_hash_table_lookup(sessions, &session_id);
	janus_mutex_unlock(&sessions_mutex);
	return session;
}

janus_session *janus_session_create(guint64 session_id) {
	if(session_id == 0) {
		while(session_id == 0) {
			session_id = janus_random_uint64();
			if(janus_session_find(session_id) != NULL) {
				// Session ID already taken, try another one 
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

//static 
	gpointer janus_sessions_watchdog(gpointer user_data) {
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


static gboolean janus_check_sessions(gpointer user_data) {
	if(session_timeout < 1)		//Session timeouts are disabled 
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
				// Mark the session as over, we'll deal with it later *
				janus_session_schedule_destruction(session, FALSE, FALSE, FALSE);
				// Notify the transport 
				if(session->source) {
					json_t *event = json_object();
					json_object_set_new(event, "janus", json_string("timeout"));
					json_object_set_new(event, "session_id", json_integer(session->session_id));
					// Send this to the transport client 
					session->source->transport->send_message(session->source->instance, NULL, FALSE, event);
					// Notify the transport plugin about the session timeout 
					session->source->transport->session_over(session->source->instance, session->session_id, TRUE);
				}
				// Notify event handlers as well 
				if(janus_events_is_enabled())
					janus_events_notify_handlers(JANUS_EVENT_TYPE_SESSION, session->session_id, "timeout", NULL);

				//FIXME Is this safe? apparently it causes hash table errors on the console 
				g_hash_table_iter_remove(&iter);
				g_hash_table_replace(old_sessions, janus_uint64_dup(session->session_id), session);
			}
		}
	}
	janus_mutex_unlock(&sessions_mutex);

	return G_SOURCE_CONTINUE;
}


static void janus_session_schedule_destruction(janus_session *session,
		gboolean lock_sessions, gboolean remove_key, gboolean notify_transport) {
	if(session == NULL || !g_atomic_int_compare_and_exchange(&session->destroy, 0, 1))
		return;
	if(lock_sessions)
		janus_mutex_lock(&sessions_mutex);
	// Schedule the session for deletion 
	janus_mutex_lock(&session->mutex);
	// Remove all handles 
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
	// Notify the source that the session has been destroyed 
	if(notify_transport && session->source && session->source->transport)
		session->source->transport->session_over(session->source->instance, session->session_id, FALSE);
}

gchar *janus_get_local_ip(void) {
	return local_ip;
}

gchar *janus_get_public_ip(void) {
	// Fallback to the local IP, if we have no public one 
	return public_ip ? public_ip : local_ip;
}
void janus_set_public_ip(const char *ip) {
	// once set do not override 
	if(ip == NULL || public_ip != NULL)
		return;
	public_ip = g_strdup(ip);
}

void janus_plugin_close(gpointer key, gpointer value, gpointer user_data) {
	janus_plugin *plugin = (janus_plugin *)value;
	if(!plugin)return;
	plugin->destroy();
}

janus_plugin *janus_plugin_find(const gchar *package) {
	if(package != NULL && plugins != NULL)	// FIXME Do we need to fix the key pointer? 
		return g_hash_table_lookup(plugins, package);
	return NULL;
}

void janus_session_notify_event(janus_session *session, json_t *event) {
// ws.send(json event); to browser
	if(session != NULL && !g_atomic_int_get(&session->destroy) && session->source != NULL && session->source->transport != NULL) {
		// Send this to the transport client 
		JANUS_LOG(LOG_HUGE, "Sending event to %s (%p)\n", session->source->transport->get_package(), session->source->instance);
		session->source->transport->send_message(session->source->instance, NULL, FALSE, event);
	} else {
		// No transport, free the event 
		json_decref(event);
	}
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

void janus_pluginso_close(gpointer key, gpointer value, gpointer user_data) {
	void *plugin = value;
	if(!plugin) return;
	// FIXME We don't dlclose plugins to be sure we can detect leaks 
	//~ dlclose(plugin);
}

void janus_plugin_end_session(janus_plugin_session *plugin_session) {
	// A plugin asked to get rid of a handle 
	if((plugin_session < (janus_plugin_session *)0x1000) || !janus_plugin_session_is_alive(plugin_session) || plugin_session->stopped)
		return;
	janus_ice_handle *ice_handle = (janus_ice_handle *)plugin_session->gateway_handle;
	if(!ice_handle)
		return;
	janus_session *session = (janus_session *)ice_handle->session;
	if(!session)
		return;
	// Destroy the handle 
	janus_mutex_lock(&session->mutex);
	janus_ice_handle_destroy(session, ice_handle->handle_id);
	g_hash_table_remove(session->ice_handles, &ice_handle->handle_id);
	janus_mutex_unlock(&session->mutex);
}

void janus_plugin_close_pc(janus_plugin_session *plugin_session) {
	// A plugin asked to get rid of a PeerConnection 
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
	//Send an alert on all the DTLS connections 
	janus_ice_webrtc_hangup(ice_handle, "Close PC");
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

// Plugin callback interface   from plugin to browser sending json event
int janus_plugin_push_event(janus_plugin_session *plugin_session,
							janus_plugin *plugin,
							const char *transaction,
							json_t *message, 
							json_t *jsep) {
	//to browser from plugin
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
	// Attach JSEP if possible? */
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
	// Reference the payload, as the plugin may still need it and will do a decref itself 
	json_incref(message);
	// Prepare JSON event 
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
	// Send the event 
	JANUS_LOG(LOG_VERB, "[%"SCNu64"] Sending event to transport...\n", ice_handle->handle_id);
	janus_session_notify_event(session, event);

	if(jsep != NULL && janus_events_is_enabled()) {
		// Notify event handlers as well 
		janus_events_notify_handlers(JANUS_EVENT_TYPE_JSEP,
			session->session_id, ice_handle->handle_id, "local", sdp_type, sdp);
	}

	return JANUS_OK;
}


void plug_fucker(GHashTable *pl,janus_plugin*janus_plugin){
if(pl == NULL){
pl = g_hash_table_new(g_str_hash, g_str_equal);
if(pl==NULL){g_print("FUCKER IS NULL\n");}else{
	g_print("PLUGINS IS NOT NULL\n");
	plugins=pl;
g_print("5sss %s\n",janus_plugin->get_package());}
g_hash_table_insert(pl, (gpointer)janus_plugin->get_package(), janus_plugin);
			}
}
			
gchar * select_local_ip(){
	
	if(local_ip == NULL) {
		local_ip = janus_network_detect_local_ip_as_string(janus_network_query_options_any_ip);
		
		if(local_ip == NULL) {
			g_print("Couldn't find any address! using 127.0.0.1 as the local IP... (which is NOT going to work out of your machine)\n");
			local_ip = g_strdup("127.0.0.1");
			//local_ip=loc_ip;
			return local_ip;
		}else{return local_ip;}
	}
}










