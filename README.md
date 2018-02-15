# webrtc
It's time to hack Lorenzo's code in Janus webrtc gateway server written in C

A standalone dedicated WebRTC gateway server is cool, but a standalone WebRTC library even better.

Сервер это хорошо, а библиотека еще лучше.
Ибо нет ничего лучше, чем свой сигнальный протокол под свою платформу и под свой вебсервер.

# Lorenzo's janus_process_incoming_request() in pseudocode

```
workflow:
raw:
1. if no sessions and handle_id, then just create them
2. find a session. if there is a handle_id, find it.
3. parse data.type: one of them: "keepalive"
									"attach"
									"destroy"
									"detach"
									"hungup"
									"message" =??? on offer, on answer ??? how to create a PeerConnection() ??
									"trickle" =??? on ice candidate ???
									"unknown type"
	
in details:
1. create new session. Create session->source, notify transport ->session_created(), and inform app about it.
2. find a session with given session_id, update session last activity,
   find a handle type janus_ice_handle with given session and hanlde_id.
3. 
"attach":
if(hundle !=NULL)return;
handle = janus_ice_handle_create(session, opaque_id);
handle_id = handle->handle_id;
handle->force_bundle = force_bundle; bool
handle->force_rtcp_mux = force_rtcp_mux; bool
err = janus_ice_handle_attach_plugin(session, handle_id, plugin_t)
ws.send({id:handle_id, session_id});

"destroy":
if(hundle !=NULL)return;
janus_session_schedule_destruction(session, TRUE, TRUE, TRUE);
ws.send(success)
	
"detach":
if(hundle ==NULL)return;
if(handle->app == NULL || handle->app_handle == NULL) return;
err=janus_ice_handle_destroy(session, handle_id);
g_hash_table_remove(session->ice_handles, &handle_id);
ws.send(success);

"hangup":
if(handle==NULL)return;
if(handle->app == NULL || handle->app_handle == NULL) return;
janus_ice_webrtc_hangup(handle, "Janus API");
ws.send(success);

"trickle":
if(handle==NULL)return;
if(handle->app == NULL || handle->app_handle == NULL
|| !janus_plugin_session_is_alive(handle->app_handle))return;
if(candidate == NULL && candidates == NULL)return;
if(candidate != NULL && candidates != NULL)return; 
"can't handle both at a time"
lock(&handle->mutex);
if(!janus_flags_is_set(&handle->webrtc_flags, JANUS_ICE_HANDLE_WEBRTC_TRICKLE)) {
janus_flags_set(&handle->webrtc_flags, JANUS_ICE_HANDLE_WEBRTC_TRICKLE);
}
if(handle->audio_stream == NULL && 
handle->video_stream == NULL && handle->data_stream == NULL) {
janus_ice_trickle *early_trickle = janus_ice_trickle_new(handle,
transaction_text, candidate ? candidate : candidates);
handle->pending_trickles = g_list_append(handle->pending_trickles, early_trickle);
goto trickledone;
}
if(janus_flags_is_set(&handle->webrtc_flags, JANUS_ICE_HANDLE_WEBRTC_PROCESSING_OFFER) ||
!janus_flags_is_set(&handle->webrtc_flags, JANUS_ICE_HANDLE_WEBRTC_GOT_OFFER) ||
!janus_flags_is_set(&handle->webrtc_flags, JANUS_ICE_HANDLE_WEBRTC_GOT_ANSWER)) {
janus_ice_trickle *early_trickle = janus_ice_trickle_new(handle,
transaction_text, candidate ? candidate : candidates);
handle->pending_trickles = g_list_append(handle->pending_trickles, early_trickle);
goto trickledone;
}
if(candidate != NULL) {
if(error = janus_ice_trickle_parse(handle, candidate, &error_string))
} else{

}
if(json_array_size(candidates) > 0) {
size_t i = 0;
for(i=0; i<json_array_size(candidates); i++) { 
json_t *c = json_array_get(candidates, i);
janus_ice_trickle_parse(handle, c, NULL);
}
}


"message":
else if(!strcasecmp(message_text, "message")) {
if(handle == NULL) {
process_error("Unhandled request '%s' at this path");
goto jsondone;
}
if(handle->app == NULL || handle->app_handle == NULL) {
process_error
goto jsondone;
		}
		
janus_plugin *plugin_t = (janus_plugin *)handle->app;
"There's a message for %s\n, handle->handle_id, plugin_t->get_name()"
if(error_code != 0) {
//janus_process_error_string(request, session_id, transaction_text, error_code, error_cause);
goto jsondone;
}
		json_t *body = json_object_get(root, "body");
		"Is there an SDP attached? "
		json_t *jsep = json_object_get(root, "jsep");
		char *jsep_type = NULL;
		char *jsep_sdp = NULL, 
		*jsep_sdp_stripped = NULL;
	
if(jsep != NULL) {
if(!json_is_object(jsep)) {
//ret = janus_process_error(request, session_id, 
//transaction_text, JANUS_ERROR_INVALID_JSON_OBJECT, "Invalid jsep object");
goto jsondone;
}

if(error_code != 0) {
//ret = janus_process_error_string(request, session_id, transaction_text, error_code, error_cause);
goto jsondone;
}
			json_t *type = json_object_get(jsep, "type");
			jsep_type = g_strdup(json_string_value(type));
			type = NULL;
			gboolean do_trickle = TRUE;
			json_t *jsep_trickle = json_object_get(jsep, "trickle");
			do_trickle = jsep_trickle ? json_is_true(jsep_trickle) : TRUE;
" Are we still cleaning up from a previous media session? "
if(janus_flags_is_set(&handle->webrtc_flags, JANUS_ICE_HANDLE_WEBRTC_CLEANING)) {
"Still cleaning up from a previous media session, let's wait a bit...\n, handle->handle_id"
gint64 waited = 0;
while(janus_flags_is_set(&handle->webrtc_flags, JANUS_ICE_HANDLE_WEBRTC_CLEANING)) {
g_usleep(100000);
waited += 100000;
if(waited >= 3*G_USEC_PER_SEC) {
" Waited 3 seconds, that's enough!\n, handle->handle_id"
break;
}
}
}
" Check the JSEP type "
janus_mutex_lock(&handle->mutex);
int offer = 0;
if(!strcasecmp(jsep_type, "offer")) {
offer = 1;
janus_flags_set(&handle->webrtc_flags, JANUS_ICE_HANDLE_WEBRTC_PROCESSING_OFFER);
janus_flags_set(&handle->webrtc_flags, JANUS_ICE_HANDLE_WEBRTC_GOT_OFFER);
janus_flags_clear(&handle->webrtc_flags, JANUS_ICE_HANDLE_WEBRTC_GOT_ANSWER);
} 
else if(!strcasecmp(jsep_type, "answer")) {
janus_flags_set(&handle->webrtc_flags, JANUS_ICE_HANDLE_WEBRTC_GOT_ANSWER);
offer = 0;
} 
else {
"TODO Handle other message types as well "
//ret = janus_process_error(request, session_id, transaction_text, JANUS_ERROR_JSEP_UNKNOWN_TYPE, 
//"JSEP error: unknown message type '%s'", jsep_type);
g_free(jsep_type);
janus_flags_clear(&handle->webrtc_flags, JANUS_ICE_HANDLE_WEBRTC_PROCESSING_OFFER);
janus_mutex_unlock(&handle->mutex);
goto jsondone;
}
json_t *sdp = json_object_get(jsep, "sdp");
jsep_sdp = (char *)json_string_value(sdp);
" Remote SDP:\n%s, handle->handle_id, jsep_sdp"
"Is this valid SDP? "
char error_str[512];
int audio = 0, video = 0, data = 0, bundle = 0, rtcpmux = 0, trickle = 0;
janus_sdp *parsed_sdp = janus_sdp_preparse(jsep_sdp, error_str, sizeof(error_str), &audio, &video, &data, &bundle, &rtcpmux, &trickle);
trickle = trickle && do_trickle;
if(parsed_sdp == NULL) {
" Invalid SDP "
//ret = janus_process_error_string(request, session_id, transaction_text, JANUS_ERROR_JSEP_INVALID_SDP, error_str);
g_free(jsep_type);
janus_flags_clear(&handle->webrtc_flags, JANUS_ICE_HANDLE_WEBRTC_PROCESSING_OFFER);
janus_mutex_unlock(&handle->mutex);
goto jsondone;
}
" FIXME We're only handling single audio/video lines for now... "
if(audio > 1) {
"More than one audio line? only going to negotiate one...\n, handle->handle_id"
}
if(video > 1) {
"More than one video line? only going to negotiate one...\n, handle->handle_id"
}
if(data > 1) {
"More than one data line? only going to negotiate one...\n, handle->handle_id"
}
#ifndef HAVE_SCTP
if(data) {
"DataChannels have been negotiated, but support for them has not been compiled...\n, handle->handle_id"
}
#endif
"[%"SCNu64"] The browser: %s BUNDLE, %s rtcp-mux, %s doing Trickle ICE\n",
handle->handle_id,
bundle  ? "supports" : "does NOT support",
rtcpmux ? "supports" : "does NOT support",
trickle ? "is"       : "is NOT");
"Check if it's a new session, or an update... "
if(!janus_flags_is_set(&handle->webrtc_flags, JANUS_ICE_HANDLE_WEBRTC_READY)|| janus_flags_is_set(&handle->webrtc_flags, 
																								  JANUS_ICE_HANDLE_WEBRTC_ALERT)) {
"New session "
if(offer) {
" Setup ICE locally (we received an offer) "
if(janus_ice_setup_local(handle, offer, audio, video, data, bundle, rtcpmux, trickle) < 0) {
"Error setting ICE locally\n"
janus_sdp_free(parsed_sdp);
g_free(jsep_type);
janus_flags_clear(&handle->webrtc_flags, JANUS_ICE_HANDLE_WEBRTC_PROCESSING_OFFER);
//ret = janus_process_error(request, session_id, transaction_text, JANUS_ERROR_UNKNOWN, "Error setting ICE locally");
janus_mutex_unlock(&handle->mutex);
goto jsondone;
}
} 
else {
"Make sure we're waiting for an ANSWER in the first place "
if(!handle->agent) {
"Unexpected ANSWER (did we offer?)\n"
janus_sdp_free(parsed_sdp);
g_free(jsep_type);
janus_flags_clear(&handle->webrtc_flags, JANUS_ICE_HANDLE_WEBRTC_PROCESSING_OFFER);
//ret = janus_process_error(request, session_id, transaction_text, JANUS_ERROR_UNEXPECTED_ANSWER, "Unexpected ANSWER (did we offer?)");
janus_mutex_unlock(&handle->mutex);
goto jsondone;
}
}
if(janus_sdp_process(handle, parsed_sdp) < 0) {
"Error processing SDP\n"
janus_sdp_free(parsed_sdp);
g_free(jsep_type);
janus_flags_clear(&handle->webrtc_flags, JANUS_ICE_HANDLE_WEBRTC_PROCESSING_OFFER);
//ret = janus_process_error(request, session_id, transaction_text, JANUS_ERROR_UNEXPECTED_ANSWER, "Error processing SDP");
janus_mutex_unlock(&handle->mutex);
goto jsondone;
}
if(!offer) {
"Set remote candidates now (we received an answer) "
if(bundle) {
janus_flags_set(&handle->webrtc_flags, JANUS_ICE_HANDLE_WEBRTC_BUNDLE);
}
else {
janus_flags_clear(&handle->webrtc_flags, JANUS_ICE_HANDLE_WEBRTC_BUNDLE);
}
if(rtcpmux) {
janus_flags_set(&handle->webrtc_flags, JANUS_ICE_HANDLE_WEBRTC_RTCPMUX);
}
else {
janus_flags_clear(&handle->webrtc_flags, JANUS_ICE_HANDLE_WEBRTC_RTCPMUX);
}
if(trickle) {
janus_flags_set(&handle->webrtc_flags, JANUS_ICE_HANDLE_WEBRTC_TRICKLE);
} 
else {
janus_flags_clear(&handle->webrtc_flags, JANUS_ICE_HANDLE_WEBRTC_TRICKLE);
}
					
					
if(janus_flags_is_set(&handle->webrtc_flags, JANUS_ICE_HANDLE_WEBRTC_BUNDLE)) {
" bundle is supported by the browser, getting rid of one of the RTP/RTCP components, if any...\n, handle->handle_id"
if(audio) {
"Get rid of video and data, if present "
if(handle->streams && handle->video_stream) {
if(handle->audio_stream->rtp_component && handle->video_stream->rtp_component)
handle->audio_stream->rtp_component->do_video_nacks = handle->video_stream->rtp_component->do_video_nacks;
handle->audio_stream->video_ssrc = handle->video_stream->video_ssrc;
handle->audio_stream->video_ssrc_peer = handle->video_stream->video_ssrc_peer;
handle->audio_stream->video_ssrc_peer_rtx = handle->video_stream->video_ssrc_peer_rtx;
handle->audio_stream->video_ssrc_peer_sim_1 = handle->video_stream->video_ssrc_peer_sim_1;
handle->audio_stream->video_ssrc_peer_sim_2 = handle->video_stream->video_ssrc_peer_sim_2;
nice_agent_attach_recv(handle->agent, handle->video_stream->stream_id, 1, g_main_loop_get_context (handle->iceloop), NULL, NULL);
if(!handle->force_rtcp_mux && !janus_ice_is_rtcpmux_forced())
nice_agent_attach_recv(handle->agent, handle->video_stream->stream_id, 2, g_main_loop_get_context (handle->iceloop), NULL, NULL);
nice_agent_remove_stream(handle->agent, handle->video_stream->stream_id);
janus_ice_stream_free(handle->streams, handle->video_stream);
}
handle->video_stream = NULL;
handle->video_id = 0;
if(handle->streams && handle->data_stream) {
nice_agent_attach_recv(handle->agent, handle->data_stream->stream_id, 1, g_main_loop_get_context (handle->iceloop), NULL, NULL);
nice_agent_remove_stream(handle->agent, handle->data_stream->stream_id);
janus_ice_stream_free(handle->streams, handle->data_stream);
}
handle->data_stream = NULL;
handle->data_id = 0;
if(!video) {
if(handle->audio_stream->rtp_component) handle->audio_stream->rtp_component->do_video_nacks = FALSE;
handle->audio_stream->video_ssrc = 0;
handle->audio_stream->video_ssrc_peer = 0;
g_free(handle->audio_stream->video_rtcp_ctx);
handle->audio_stream->video_rtcp_ctx = NULL;
}
} 
	
else if(video) {
" Get rid of data, if present "
if(handle->streams && handle->data_stream) {
nice_agent_attach_recv(handle->agent, handle->data_stream->stream_id, 1, g_main_loop_get_context (handle->iceloop), NULL, NULL);
nice_agent_remove_stream(handle->agent, handle->data_stream->stream_id);
janus_ice_stream_free(handle->streams, handle->data_stream);
}
handle->data_stream = NULL;
handle->data_id = 0;
}
}
					
if(janus_flags_is_set(&handle->webrtc_flags, JANUS_ICE_HANDLE_WEBRTC_RTCPMUX) && 
   !handle->force_rtcp_mux && !janus_ice_is_rtcpmux_forced()) {
"rtcp-mux is supported by the browser, getting rid of RTCP components, if any...\n, handle->handle_id"
if(handle->audio_stream && handle->audio_stream->components != NULL) {
nice_agent_attach_recv(handle->agent, handle->audio_id, 2, g_main_loop_get_context (handle->iceloop), NULL, NULL);
"Free the component "
janus_ice_component_free(handle->audio_stream->components, handle->audio_stream->rtcp_component);
handle->audio_stream->rtcp_component = NULL;
"Create a dummy candidate and enforce it as the one to use for this now unneeded component "
NiceCandidate *c = nice_candidate_new(NICE_CANDIDATE_TYPE_HOST);
c->component_id = 2;
c->stream_id = handle->audio_stream->stream_id;
#ifndef HAVE_LIBNICE_TCP
c->transport = NICE_CANDIDATE_TRANSPORT_UDP;
#endif
strncpy(c->foundation, "1", NICE_CANDIDATE_MAX_FOUNDATION);
c->priority = 1;
nice_address_set_from_string(&c->addr, "127.0.0.1");
nice_address_set_port(&c->addr, janus_ice_get_rtcpmux_blackhole_port());
c->username = g_strdup(handle->audio_stream->ruser);
c->password = g_strdup(handle->audio_stream->rpass);
if(!nice_agent_set_selected_remote_candidate(handle->agent, handle->audio_stream->stream_id, 2, c)) {
"[%"SCNu64"] Error forcing dummy candidate on RTCP component of stream %d\n, handle->handle_id, handle->audio_stream->stream_id"
nice_candidate_free(c);
}
}
if(handle->video_stream && handle->video_stream->components != NULL) {
nice_agent_attach_recv(handle->agent, handle->video_id, 2, g_main_loop_get_context (handle->iceloop), NULL, NULL);
" Free the component "
janus_ice_component_free(handle->video_stream->components, handle->video_stream->rtcp_component);
handle->video_stream->rtcp_component = NULL;
"Create a dummy candidate and enforce it as the one to use for this now unneeded component "
NiceCandidate *c = nice_candidate_new(NICE_CANDIDATE_TYPE_HOST);
c->component_id = 2;
c->stream_id = handle->video_stream->stream_id;
#ifndef HAVE_LIBNICE_TCP
c->transport = NICE_CANDIDATE_TRANSPORT_UDP;
#endif
strncpy(c->foundation, "1", NICE_CANDIDATE_MAX_FOUNDATION);
c->priority = 1;
nice_address_set_from_string(&c->addr, "127.0.0.1");
nice_address_set_port(&c->addr, janus_ice_get_rtcpmux_blackhole_port());
c->username = g_strdup(handle->video_stream->ruser);
c->password = g_strdup(handle->video_stream->rpass);
if(!nice_agent_set_selected_remote_candidate(handle->agent, handle->video_stream->stream_id, 2, c)) {
"[%"SCNu64"] Error forcing dummy candidate on RTCP component of stream %d, handle->handle_id, handle->video_stream->stream_id"
nice_candidate_free(c);
}
}
}
"FIXME Any disabled m-line?"
if(strstr(jsep_sdp, "m=audio 0")) {
"[%"SCNu64"] Audio disabled via SDP, handle->handle_id"
if(!janus_flags_is_set(&handle->webrtc_flags, JANUS_ICE_HANDLE_WEBRTC_BUNDLE)
|| (!video && !data)) {
" Marking audio stream as disabled\n"
janus_ice_stream *stream = g_hash_table_lookup(handle->streams, GUINT_TO_POINTER(handle->audio_id));
if(stream)stream->disabled = TRUE;
}
}

if(strstr(jsep_sdp, "m=video 0")) {
"[%"SCNu64"] Video disabled via SDP, handle->handle_id"
if(!janus_flags_is_set(&handle->webrtc_flags, JANUS_ICE_HANDLE_WEBRTC_BUNDLE)
	|| (!audio && !data)) {
"  -- Marking video stream as disabled\n"
janus_ice_stream *stream = NULL;
if(!janus_flags_is_set(&handle->webrtc_flags, JANUS_ICE_HANDLE_WEBRTC_BUNDLE)) {
stream = g_hash_table_lookup(handle->streams, GUINT_TO_POINTER(handle->video_id));
} else {
gint id = handle->audio_id > 0 ? handle->audio_id : handle->video_id;
stream = g_hash_table_lookup(handle->streams, GUINT_TO_POINTER(id));
}
if(stream)stream->disabled = TRUE;
}
}
					
if(strstr(jsep_sdp, "m=application 0 DTLS/SCTP")) {
"[%"SCNu64"] Data Channel disabled via SDP\n, handle->handle_id"
if(!janus_flags_is_set(&handle->webrtc_flags, JANUS_ICE_HANDLE_WEBRTC_BUNDLE)|| (!audio && !video)) {
"  -- Marking data channel stream as disabled\n"
janus_ice_stream *stream = NULL;
if(!janus_flags_is_set(&handle->webrtc_flags, JANUS_ICE_HANDLE_WEBRTC_BUNDLE)) {
stream = g_hash_table_lookup(handle->streams, GUINT_TO_POINTER(handle->data_id));
} else {
gint id = handle->audio_id > 0 ? handle->audio_id : (handle->video_id > 0 ? handle->video_id : handle->data_id);
stream = g_hash_table_lookup(handle->streams, GUINT_TO_POINTER(id));
}
if(stream)stream->disabled = TRUE;
}
}
					
"We got our answer "
janus_flags_clear(&handle->webrtc_flags, JANUS_ICE_HANDLE_WEBRTC_PROCESSING_OFFER);
" Any pending trickles? "
					
if(handle->pending_trickles) {
"[%"SCNu64"] Processing %d pending trickle candidates\n, handle->handle_id, g_list_length(handle->pending_trickles)"
GList *temp = NULL;
while(handle->pending_trickles) {
temp = g_list_first(handle->pending_trickles);
handle->pending_trickles = g_list_remove_link(handle->pending_trickles, temp);
janus_ice_trickle *trickle = (janus_ice_trickle *)temp->data;
g_list_free(temp);
if(trickle == NULL)continue;
if((janus_get_monotonic_time() - trickle->received) > 45*G_USEC_PER_SEC) {
"FIXME Candidate is too old, discard it "
janus_ice_trickle_destroy(trickle);
continue;
}
json_t *candidate = trickle->candidate;
if(candidate == NULL) {
janus_ice_trickle_destroy(trickle);
continue;
}
if(json_is_object(candidate)) {
" We got a single candidate "
int error = 0;
const char *error_string = NULL;
if((error = janus_ice_trickle_parse(handle, candidate, &error_string)) != 0) {
"FIXME We should report the error parsing the trickle candidate "
}
} else if(json_is_array(candidate)) {
" We got multiple candidates in an array "
"Got multiple candidates (%zu)\n, json_array_size(candidate)"
if(json_array_size(candidate) > 0) {
"Handle remote candidates "
size_t i = 0;
for(i=0; i<json_array_size(candidate); i++) {
json_t *c = json_array_get(candidate, i);
"FIXME We don't care if any trickle fails to parse "
janus_ice_trickle_parse(handle, c, NULL);
}
}
}
"Done, free candidate "
janus_ice_trickle_destroy(trickle);
}
}
					
"This was an answer, check if it's time to start ICE "
if(janus_flags_is_set(&handle->webrtc_flags, JANUS_ICE_HANDLE_WEBRTC_TRICKLE) &&
!janus_flags_is_set(&handle->webrtc_flags, JANUS_ICE_HANDLE_WEBRTC_ALL_TRICKLES)) {
"ICE Trickling is supported by the browser, waiting for remote candidates...\n, handle->handle_id"
janus_flags_set(&handle->webrtc_flags, JANUS_ICE_HANDLE_WEBRTC_START);
} 
else {
" Done! Sending connectivity checks...\n, handle->handle_id"
if(handle->audio_id > 0) {
janus_ice_setup_remote_candidates(handle, handle->audio_id, 1);
if(!janus_flags_is_set(&handle->webrtc_flags, JANUS_ICE_HANDLE_WEBRTC_RTCPMUX))	
"http://tools.ietf.org/html/rfc5761#section-5.1.3 "
janus_ice_setup_remote_candidates(handle, handle->audio_id, 2);
}
if(handle->video_id > 0) {
janus_ice_setup_remote_candidates(handle, handle->video_id, 1);
if(!janus_flags_is_set(&handle->webrtc_flags, JANUS_ICE_HANDLE_WEBRTC_RTCPMUX))	
"http://tools.ietf.org/html/rfc5761#section-5.1.3"
janus_ice_setup_remote_candidates(handle, handle->video_id, 2);
}
if(handle->data_id > 0) {
janus_ice_setup_remote_candidates(handle, handle->data_id, 1);
}
}
}
}
else {
"TODO Actually handle session updates: for now we ignore them, and just relay them to plugins "
"[%"SCNu64"] Ignoring negotiation update, we don't support them yet...\n, handle->handle_id"
}
handle->remote_sdp = g_strdup(jsep_sdp);
janus_mutex_unlock(&handle->mutex);
" Anonymize SDP "
if(janus_sdp_anonymize(parsed_sdp) < 0) {
"Invalid SDP"
//ret = janus_process_error(request, session_id, transaction_text, JANUS_ERROR_JSEP_INVALID_SDP, "JSEP error: invalid SDP");
janus_sdp_free(parsed_sdp);
g_free(jsep_type);
janus_flags_clear(&handle->webrtc_flags, JANUS_ICE_HANDLE_WEBRTC_PROCESSING_OFFER);
goto jsondone;
}
jsep_sdp_stripped = janus_sdp_write(parsed_sdp);
janus_sdp_free(parsed_sdp);
sdp = NULL;
janus_flags_clear(&handle->webrtc_flags, JANUS_ICE_HANDLE_WEBRTC_PROCESSING_OFFER);
}
//end jsep !=NULL
"Make sure the app handle is still valid"
if(handle->app == NULL || handle->app_handle == NULL || !janus_plugin_session_is_alive(handle->app_handle)) {
//ret = janus_process_error(request, session_id, transaction_text, JANUS_ERROR_PLUGIN_MESSAGE, "No plugin to handle this message");
g_free(jsep_type);
g_free(jsep_sdp_stripped);
janus_flags_clear(&handle->webrtc_flags, JANUS_ICE_HANDLE_WEBRTC_PROCESSING_OFFER);
goto jsondone;
}
"Send the message to the plugin (which must eventually free transaction_text and unref the two objects, body and jsep)"
json_incref(body);
json_t *body_jsep = NULL;
if(jsep_sdp_stripped) {
body_jsep = json_pack("{ssss}", "type", jsep_type, "sdp", jsep_sdp_stripped);
" Check if VP8 simulcasting is enabled "
if(janus_flags_is_set(&handle->webrtc_flags, JANUS_ICE_HANDLE_WEBRTC_HAS_VIDEO)) {
if(handle->video_stream && handle->video_stream->video_ssrc_peer_sim_1) {
json_t *simulcast = json_object();
json_object_set(simulcast, "ssrc-0", json_integer(handle->video_stream->video_ssrc_peer));
json_object_set(simulcast, "ssrc-1", json_integer(handle->video_stream->video_ssrc_peer_sim_1));
if(handle->video_stream->video_ssrc_peer_sim_2)
json_object_set(simulcast, "ssrc-2", json_integer(handle->video_stream->video_ssrc_peer_sim_2));
json_object_set(body_jsep, "simulcast", simulcast);
} else if(handle->audio_stream && handle->audio_stream->video_ssrc_peer_sim_1) {
json_t *simulcast = json_object();
json_object_set(simulcast, "ssrc-0", json_integer(handle->audio_stream->video_ssrc_peer));
json_object_set(simulcast, "ssrc-1", json_integer(handle->audio_stream->video_ssrc_peer_sim_1));
if(handle->audio_stream->video_ssrc_peer_sim_2)
json_object_set(simulcast, "ssrc-2", json_integer(handle->audio_stream->video_ssrc_peer_sim_2));
json_object_set(body_jsep, "simulcast", simulcast);
}
}
}
janus_plugin_result *result = plugin_t->handle_message(handle->app_handle,
g_strdup((char *)transaction_text), body, body_jsep);
g_free(jsep_type);
g_free(jsep_sdp_stripped);
if(result == NULL) {
" Something went horribly wrong!"
//ret = janus_process_error(request, session_id, transaction_text, JANUS_ERROR_PLUGIN_MESSAGE, "Plugin didn't give a result");
goto jsondone;
}
if(result->type == JANUS_PLUGIN_OK) {
"The plugin gave a result already (synchronous request/response) "
if(result->content == NULL || !json_is_object(result->content)) {
" Missing content, or not a JSON object "
//ret = janus_process_error(request, session_id, transaction_text, JANUS_ERROR_PLUGIN_MESSAGE,
//result->content == NULL ?
//"Plugin didn't provide any content for this synchronous response" :
//"Plugin returned an invalid JSON response");
janus_plugin_result_destroy(result);
goto jsondone;
}
" Reference the content, as destroying the result instance will decref it "
json_incref(result->content);
" Prepare JSON response "
json_t *reply = json_object();
json_object_set_new(reply, "janus", json_string("success"));
json_object_set_new(reply, "session_id", json_integer(session->session_id));
json_object_set_new(reply, "sender", json_integer(handle->handle_id));
json_object_set_new(reply, "transaction", json_string(transaction_text));
json_t *plugin_data = json_object();
json_object_set_new(plugin_data, "plugin", json_string(plugin_t->get_package()));
json_object_set_new(plugin_data, "data", result->content);
json_object_set_new(reply, "plugindata", plugin_data);
"Send the success reply "
//ret = janus_process_success(request, reply);
} 
else if(result->type == JANUS_PLUGIN_OK_WAIT) {
"The plugin received the request but didn't process it yet, send an ack (asynchronous notifications may follow) "
json_t *reply = json_object();
json_object_set_new(reply, "janus", json_string("ack"));
json_object_set_new(reply, "session_id", json_integer(session_id));
if(result->text)
json_object_set_new(reply, "hint", json_string(result->text));
json_object_set_new(reply, "transaction", json_string(transaction_text));
" Send the success reply "
//ret = janus_process_success(request, reply);
} 
else {
" Something went horribly wrong! "
//ret = janus_process_error_string(request, session_id, transaction_text, JANUS_ERROR_PLUGIN_MESSAGE,
//(char *)(result->text ? result->text : "Plugin returned a severe (unknown) error"));
janus_plugin_result_destroy(result);
goto jsondone;
}
janus_plugin_result_destroy(result);
}

```
