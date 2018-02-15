// workflow in a pseudocode
// as if websocket.on("message",(data)=>{})
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
if(handle->audio_stream == NULL && handle->video_stream == NULL && handle->data_stream == NULL) {
janus_ice_trickle *early_trickle = janus_ice_trickle_new(handle, transaction_text, candidate ? candidate : candidates);
handle->pending_trickles = g_list_append(handle->pending_trickles, early_trickle);
goto trickledone;
}
if(janus_flags_is_set(&handle->webrtc_flags, JANUS_ICE_HANDLE_WEBRTC_PROCESSING_OFFER) ||
	!janus_flags_is_set(&handle->webrtc_flags, JANUS_ICE_HANDLE_WEBRTC_GOT_OFFER) ||
	!janus_flags_is_set(&handle->webrtc_flags, JANUS_ICE_HANDLE_WEBRTC_GOT_ANSWER)) {
janus_ice_trickle *early_trickle = janus_ice_trickle_new(handle, transaction_text, candidate ? candidate : candidates);
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
//ret = janus_process_error(request, session_id, transaction_text, JANUS_ERROR_INVALID_JSON_OBJECT, "Invalid jsep object");
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



int janus_process_incoming_request(janus_requet *request) {
	int ret = -1;
	int error_code = 0;
	char error_cause[100];
	json_t *root = request->message;
	/* Ok, let's start with the ids */
	guint64 session_id = 0, handle_id = 0;
	json_t *s = json_object_get(root, "session_id");
	if(s && json_is_integer(s))
		session_id = json_integer_value(s);
	json_t *h = json_object_get(root, "handle_id");
	if(h && json_is_integer(h))
		handle_id = json_integer_value(h);

	/* Get transaction and message request */
	JANUS_VALIDATE_JSON_OBJECT(root, incoming_request_parameters,
		error_code, error_cause, FALSE,
		JANUS_ERROR_MISSING_MANDATORY_ELEMENT, JANUS_ERROR_INVALID_ELEMENT_TYPE);
	if(error_code != 0) {
		ret = janus_process_error_string(request, session_id, NULL, error_code, error_cause);
		goto jsondone;
	}
	json_t *transaction = json_object_get(root, "transaction");
	const gchar *transaction_text = json_string_value(transaction);
	json_t *message = json_object_get(root, "janus");
	const gchar *message_text = json_string_value(message);

	if(session_id == 0 && handle_id == 0) {
		/* Can only be a 'Create new session', a 'Get info' or a 'Ping/Pong' request */
		
		
		/* Any secret/token to check? */
		
	
		session_id = 0;
		json_t *id = json_object_get(root, "id");
		if(id != NULL) {
			/* The application provided the session ID to use */
			session_id = json_integer_value(id);
			if(session_id > 0 && janus_session_find(session_id) != NULL) {
				/* Session ID already taken */
				ret = janus_process_error(request, session_id, transaction_text, JANUS_ERROR_SESSION_CONFLICT, "Session ID already in use");
				goto jsondone;
			}
		}
		/* Handle it */
		janus_session *session = janus_session_create(session_id);
		if(session == NULL) {
			ret = janus_process_error(request, session_id, transaction_text, JANUS_ERROR_UNKNOWN, "Memory error");
			goto jsondone;
		}
		session_id = session->session_id;
		/* Take note of the request source that originated this session (HTTP, WebSockets, RabbitMQ?) */
		session->source = janus_request_new(request->transport, request->instance, NULL, FALSE, NULL);
		/* Notify the source that a new session has been created */
		request->transport->session_created(request->instance, session->session_id);
	
		/* Prepare JSON reply */
		json_t *reply = json_object();
		json_object_set_new(reply, "janus", json_string("success"));
		json_object_set_new(reply, "transaction", json_string(transaction_text));
		json_t *data = json_object();
		json_object_set_new(data, "id", json_integer(session_id));
		json_object_set_new(reply, "data", data);
		/* Send the success reply */
		ret = janus_process_success(request, reply);
		goto jsondone;
	}
	if(session_id < 1) {
		JANUS_LOG(LOG_ERR, "Invalid session\n");
		ret = janus_process_error(request, session_id, transaction_text, JANUS_ERROR_SESSION_NOT_FOUND, NULL);
		goto jsondone;
	}
	if(h && handle_id < 1) {
		JANUS_LOG(LOG_ERR, "Invalid handle\n");
		ret = janus_process_error(request, session_id, transaction_text, JANUS_ERROR_SESSION_NOT_FOUND, NULL);
		goto jsondone;
	}

	/* Go on with the processing */
	gboolean secret_authorized = FALSE, token_authorized = FALSE;
	

	/* If we got here, make sure we have a session (and/or a handle) */
	janus_session *session = janus_session_find(session_id);
	if(!session) {
		JANUS_LOG(LOG_ERR, "Couldn't find any session %"SCNu64"...\n", session_id);
		ret = janus_process_error(request, session_id, transaction_text, JANUS_ERROR_SESSION_NOT_FOUND, "No such session %"SCNu64"", session_id);
		goto jsondone;
	}
	/* Update the last activity timer */
	session->last_activity = janus_get_monotonic_time();
	janus_ice_handle *handle = NULL;
	if(handle_id > 0) {
		janus_mutex_lock(&session->mutex);
		handle = janus_ice_handle_find(session, handle_id);
		janus_mutex_unlock(&session->mutex);
		if(!handle) {
			JANUS_LOG(LOG_ERR, "Couldn't find any handle %"SCNu64" in session %"SCNu64"...\n", handle_id, session_id);
			ret = janus_process_error(request, session_id, transaction_text, JANUS_ERROR_HANDLE_NOT_FOUND, "No such handle %"SCNu64" in session %"SCNu64"", handle_id, session_id);
			goto jsondone;
		}
	}

	/* What is this? */
	if(!strcasecmp(message_text, "keepalive")) {
	} 
	else if(!strcasecmp(message_text, "attach")) {
		if(handle != NULL) {
			/* Attach is a session-level command */
			ret = janus_process_error(request, 
					session_id, transaction_text, JANUS_ERROR_INVALID_REQUEST_PATH, "Unhandled request '%s' at this path", message_text);
			goto jsondone;
		}
		JANUS_VALIDATE_JSON_OBJECT(root, attach_parameters,
			error_code, error_cause, FALSE,
			JANUS_ERROR_MISSING_MANDATORY_ELEMENT, JANUS_ERROR_INVALID_ELEMENT_TYPE);
		if(error_code != 0) {
			ret = janus_process_error_string(request, session_id, NULL, error_code, error_cause);
			goto jsondone;
		}
		json_t *plugin = json_object_get(root, "plugin");
		gboolean force_bundle = json_is_true(json_object_get(root, "force-bundle"));
		gboolean force_rtcp_mux = json_is_true(json_object_get(root, "force-rtcp-mux"));
		const gchar *plugin_text = json_string_value(plugin);
		janus_plugin *plugin_t = janus_plugin_find(plugin_text);
		if(plugin_t == NULL) {
			ret = janus_process_error(request, session_id, transaction_text, JANUS_ERROR_PLUGIN_NOT_FOUND, "No such plugin '%s'", plugin_text);
			goto jsondone;
		}
		
		json_t *opaque = json_object_get(root, "opaque_id");
		const char *opaque_id = opaque ? json_string_value(opaque) : NULL;
		/* Create handle */
		janus_mutex_lock(&session->mutex);
		handle = janus_ice_handle_create(session, opaque_id);
		if(handle == NULL) {
			ret = janus_process_error(request, session_id, transaction_text, JANUS_ERROR_UNKNOWN, "Memory error");
			janus_mutex_unlock(&session->mutex);
			goto jsondone;
		}
		handle_id = handle->handle_id;
		handle->force_bundle = force_bundle;
		handle->force_rtcp_mux = force_rtcp_mux;
		/* Attach to the plugin */
		int error = 0;
		if((error = janus_ice_handle_attach_plugin(session, handle_id, plugin_t)) != 0) {
			/* TODO Make error struct to pass verbose information */
			janus_ice_handle_destroy(session, handle_id);
			g_hash_table_remove(session->ice_handles, &handle_id);
			janus_mutex_unlock(&session->mutex);
			JANUS_LOG(LOG_ERR, "Couldn't attach to plugin '%s', error '%d'\n", plugin_text, error);
			ret = janus_process_error(request, session_id, transaction_text, JANUS_ERROR_PLUGIN_ATTACH, "Couldn't attach to plugin: error '%d'", error);
			goto jsondone;
		}
		janus_mutex_unlock(&session->mutex);
		/* Prepare JSON reply */
		json_t *reply = json_object();
		json_object_set_new(reply, "janus", json_string("success"));
		json_object_set_new(reply, "session_id", json_integer(session_id));
		json_object_set_new(reply, "transaction", json_string(transaction_text));
		json_t *data = json_object();
		json_object_set_new(data, "id", json_integer(handle_id));
		json_object_set_new(reply, "data", data);
		/* Send the success reply */
		ret = janus_process_success(request, reply);
	} 
	else if(!strcasecmp(message_text, "destroy")) {
		if(handle != NULL) {
			/* Query is a session-level command */
			ret = janus_process_error(request, session_id, transaction_text, JANUS_ERROR_INVALID_REQUEST_PATH, "Unhandled request '%s' at this path", message_text);
			goto jsondone;
		}
		/* Mark the session as destroyed */
		janus_session_schedule_destruction(session, TRUE, TRUE, TRUE);

		/* Prepare JSON reply */
		json_t *reply = json_object();
		json_object_set_new(reply, "janus", json_string("success"));
		json_object_set_new(reply, "session_id", json_integer(session_id));
		json_object_set_new(reply, "transaction", json_string(transaction_text));
		/* Send the success reply */
		ret = janus_process_success(request, reply);
		/* Notify event handlers as well */
		if(janus_events_is_enabled())
			janus_events_notify_handlers(JANUS_EVENT_TYPE_SESSION, session_id, "destroyed", NULL);
	}
	else if(!strcasecmp(message_text, "detach")) {
		if(handle == NULL) {
			/* Query is an handle-level command */
			ret = janus_process_error(request, session_id, transaction_text, JANUS_ERROR_INVALID_REQUEST_PATH, "Unhandled request '%s' at this path", message_text);
			goto jsondone;
		}
		if(handle->app == NULL || handle->app_handle == NULL) {
			ret = janus_process_error(request, session_id, transaction_text, JANUS_ERROR_PLUGIN_DETACH, "No plugin to detach from");
			goto jsondone;
		}
		janus_mutex_lock(&session->mutex);
		int error = janus_ice_handle_destroy(session, handle_id);
		g_hash_table_remove(session->ice_handles, &handle_id);
		janus_mutex_unlock(&session->mutex);

		if(error != 0) {
			/* TODO Make error struct to pass verbose information */
			ret = janus_process_error(request, session_id, transaction_text, JANUS_ERROR_PLUGIN_DETACH, "Couldn't detach from plugin: error '%d'", error);
			/* TODO Delete handle instance */
			goto jsondone;
		}
		/* Prepare JSON reply */
		json_t *reply = json_object();
		json_object_set_new(reply, "janus", json_string("success"));
		json_object_set_new(reply, "session_id", json_integer(session_id));
		json_object_set_new(reply, "transaction", json_string(transaction_text));
		/* Send the success reply */
		ret = janus_process_success(request, reply);
	} 
	else if(!strcasecmp(message_text, "hangup")) {
		if(handle == NULL) {
			/* Query is an handle-level command */
			ret = janus_process_error(request, session_id, transaction_text, JANUS_ERROR_INVALID_REQUEST_PATH, "Unhandled request '%s' at this path", message_text);
			goto jsondone;
		}
		if(handle->app == NULL || handle->app_handle == NULL) {
			ret = janus_process_error(request, session_id, transaction_text, JANUS_ERROR_PLUGIN_DETACH, "No plugin attached");
			goto jsondone;
		}
		janus_ice_webrtc_hangup(handle, "Janus API");
		/* Prepare JSON reply */
		json_t *reply = json_object();
		json_object_set_new(reply, "janus", json_string("success"));
		json_object_set_new(reply, "session_id", json_integer(session_id));
		json_object_set_new(reply, "transaction", json_string(transaction_text));
		/* Send the success reply */
		ret = janus_process_success(request, reply);
	} 
	
	
	
	else if(!strcasecmp(message_text, "message")) {
		if(handle == NULL) {
			/* Query is an handle-level command */
			ret = janus_process_error(request, session_id, transaction_text, JANUS_ERROR_INVALID_REQUEST_PATH, "Unhandled request '%s' at this path", message_text);
			goto jsondone;
		}
		if(handle->app == NULL || handle->app_handle == NULL) {
			ret = janus_process_error(request, session_id, transaction_text, JANUS_ERROR_PLUGIN_MESSAGE, "No plugin to handle this message");
			goto jsondone;
		}
		
		janus_plugin *plugin_t = (janus_plugin *)handle->app;
		JANUS_LOG(LOG_VERB, "[%"SCNu64"] There's a message for %s\n", handle->handle_id, plugin_t->get_name());
		JANUS_VALIDATE_JSON_OBJECT(root, body_parameters,
			error_code, error_cause, FALSE,
			JANUS_ERROR_MISSING_MANDATORY_ELEMENT, JANUS_ERROR_INVALID_ELEMENT_TYPE);
		if(error_code != 0) {
			ret = janus_process_error_string(request, session_id, transaction_text, error_code, error_cause);
			goto jsondone;
		}
		json_t *body = json_object_get(root, "body");
		/* Is there an SDP attached? */
		json_t *jsep = json_object_get(root, "jsep");
		char *jsep_type = NULL;
		char *jsep_sdp = NULL, 
		*jsep_sdp_stripped = NULL;
if(jsep != NULL) {
if(!json_is_object(jsep)) {
				ret = janus_process_error(request, session_id, transaction_text, JANUS_ERROR_INVALID_JSON_OBJECT, "Invalid jsep object");
				goto jsondone;
			}
JANUS_VALIDATE_JSON_OBJECT_FORMAT("JSEP error: missing mandatory element (%s)",
"JSEP error: invalid element type (%s should be %s)",
jsep, jsep_parameters, error_code, error_cause, FALSE,
JANUS_ERROR_MISSING_MANDATORY_ELEMENT, JANUS_ERROR_INVALID_ELEMENT_TYPE);
			if(error_code != 0) {
				ret = janus_process_error_string(request, session_id, transaction_text, error_code, error_cause);
				goto jsondone;
			}
			json_t *type = json_object_get(jsep, "type");
			jsep_type = g_strdup(json_string_value(type));
			type = NULL;
			gboolean do_trickle = TRUE;
			json_t *jsep_trickle = json_object_get(jsep, "trickle");
			do_trickle = jsep_trickle ? json_is_true(jsep_trickle) : TRUE;
			/* Are we still cleaning up from a previous media session? */
if(janus_flags_is_set(&handle->webrtc_flags, JANUS_ICE_HANDLE_WEBRTC_CLEANING)) {
				JANUS_LOG(LOG_VERB, "[%"SCNu64"] Still cleaning up from a previous media session, let's wait a bit...\n", handle->handle_id);
				gint64 waited = 0;
				while(janus_flags_is_set(&handle->webrtc_flags, JANUS_ICE_HANDLE_WEBRTC_CLEANING)) {
					g_usleep(100000);
					waited += 100000;
					if(waited >= 3*G_USEC_PER_SEC) {
						JANUS_LOG(LOG_VERB, "[%"SCNu64"]   -- Waited 3 seconds, that's enough!\n", handle->handle_id);
						break;
					}
				}
			}
			/* Check the JSEP type */
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
				/* TODO Handle other message types as well */
ret = janus_process_error(request, session_id, transaction_text, JANUS_ERROR_JSEP_UNKNOWN_TYPE, 
						"JSEP error: unknown message type '%s'", jsep_type);
				g_free(jsep_type);
				janus_flags_clear(&handle->webrtc_flags, JANUS_ICE_HANDLE_WEBRTC_PROCESSING_OFFER);
				janus_mutex_unlock(&handle->mutex);
				goto jsondone;
			}
			json_t *sdp = json_object_get(jsep, "sdp");
			jsep_sdp = (char *)json_string_value(sdp);
			JANUS_LOG(LOG_VERB, "[%"SCNu64"] Remote SDP:\n%s", handle->handle_id, jsep_sdp);
			/* Is this valid SDP? */
			char error_str[512];
			int audio = 0, video = 0, data = 0, bundle = 0, rtcpmux = 0, trickle = 0;
			janus_sdp *parsed_sdp = janus_sdp_preparse(jsep_sdp, error_str, sizeof(error_str), &audio, &video, &data, &bundle, &rtcpmux, &trickle);
			trickle = trickle && do_trickle;
			if(parsed_sdp == NULL) {
				/* Invalid SDP */
				ret = janus_process_error_string(request, session_id, transaction_text, JANUS_ERROR_JSEP_INVALID_SDP, error_str);
				g_free(jsep_type);
				janus_flags_clear(&handle->webrtc_flags, JANUS_ICE_HANDLE_WEBRTC_PROCESSING_OFFER);
				janus_mutex_unlock(&handle->mutex);
				goto jsondone;
			}
			/* Notify event handlers */
			if(janus_events_is_enabled()) {
				janus_events_notify_handlers(JANUS_EVENT_TYPE_JSEP,
					session_id, handle_id, "remote", jsep_type, jsep_sdp);
			}
			/* FIXME We're only handling single audio/video lines for now... */
			JANUS_LOG(LOG_VERB, "[%"SCNu64"] Audio %s been negotiated, Video %s been negotiated, SCTP/DataChannels %s been negotiated\n",
			                    handle->handle_id,
			                    audio ? "has" : "has NOT",
			                    video ? "has" : "has NOT",
			                    data ? "have" : "have NOT");
			if(audio > 1) {
				JANUS_LOG(LOG_WARN, "[%"SCNu64"] More than one audio line? only going to negotiate one...\n", handle->handle_id);
			}
			if(video > 1) {
				JANUS_LOG(LOG_WARN, "[%"SCNu64"] More than one video line? only going to negotiate one...\n", handle->handle_id);
			}
			if(data > 1) {
				JANUS_LOG(LOG_WARN, "[%"SCNu64"] More than one data line? only going to negotiate one...\n", handle->handle_id);
			}
#ifndef HAVE_SCTP
			if(data) {
				JANUS_LOG(LOG_WARN, "[%"SCNu64"]   -- DataChannels have been negotiated, but support for them has not been compiled...\n", handle->handle_id);
			}
#endif
			JANUS_LOG(LOG_VERB, "[%"SCNu64"] The browser: %s BUNDLE, %s rtcp-mux, %s doing Trickle ICE\n", handle->handle_id,
			                    bundle  ? "supports" : "does NOT support",
			                    rtcpmux ? "supports" : "does NOT support",
			                    trickle ? "is"       : "is NOT");
			/* Check if it's a new session, or an update... */
if(!janus_flags_is_set(&handle->webrtc_flags, JANUS_ICE_HANDLE_WEBRTC_READY)|| janus_flags_is_set(&handle->webrtc_flags, 
																								  JANUS_ICE_HANDLE_WEBRTC_ALERT)) {
				/* New session */
				if(offer) {
					/* Setup ICE locally (we received an offer) */
					if(janus_ice_setup_local(handle, offer, audio, video, data, bundle, rtcpmux, trickle) < 0) {
						JANUS_LOG(LOG_ERR, "Error setting ICE locally\n");
						janus_sdp_free(parsed_sdp);
						g_free(jsep_type);
						janus_flags_clear(&handle->webrtc_flags, JANUS_ICE_HANDLE_WEBRTC_PROCESSING_OFFER);
						ret = janus_process_error(request, session_id, transaction_text, JANUS_ERROR_UNKNOWN, "Error setting ICE locally");
						janus_mutex_unlock(&handle->mutex);
						goto jsondone;
					}
				} 
				else {
					/* Make sure we're waiting for an ANSWER in the first place */
					if(!handle->agent) {
						JANUS_LOG(LOG_ERR, "Unexpected ANSWER (did we offer?)\n");
						janus_sdp_free(parsed_sdp);
						g_free(jsep_type);
						janus_flags_clear(&handle->webrtc_flags, JANUS_ICE_HANDLE_WEBRTC_PROCESSING_OFFER);
						ret = janus_process_error(request, session_id, transaction_text, JANUS_ERROR_UNEXPECTED_ANSWER, "Unexpected ANSWER (did we offer?)");
						janus_mutex_unlock(&handle->mutex);
						goto jsondone;
					}
				}
				if(janus_sdp_process(handle, parsed_sdp) < 0) {
					JANUS_LOG(LOG_ERR, "Error processing SDP\n");
					janus_sdp_free(parsed_sdp);
					g_free(jsep_type);
					janus_flags_clear(&handle->webrtc_flags, JANUS_ICE_HANDLE_WEBRTC_PROCESSING_OFFER);
					ret = janus_process_error(request, session_id, transaction_text, JANUS_ERROR_UNEXPECTED_ANSWER, "Error processing SDP");
					janus_mutex_unlock(&handle->mutex);
					goto jsondone;
				}
				if(!offer) {
					/* Set remote candidates now (we received an answer) */
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
JANUS_LOG(LOG_HUGE, "[%"SCNu64"]   -- bundle is supported by the browser, getting rid of one of the RTP/RTCP components, if any...\n", handle->handle_id);
if(audio) {
							/* Get rid of video and data, if present */
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
							/* Get rid of data, if present */
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
						JANUS_LOG(LOG_HUGE, "[%"SCNu64"]   -- rtcp-mux is supported by the browser, getting rid of RTCP components, if any...\n", handle->handle_id);
						if(handle->audio_stream && handle->audio_stream->components != NULL) {
							nice_agent_attach_recv(handle->agent, handle->audio_id, 2, g_main_loop_get_context (handle->iceloop), NULL, NULL);
							/* Free the component */
							janus_ice_component_free(handle->audio_stream->components, handle->audio_stream->rtcp_component);
							handle->audio_stream->rtcp_component = NULL;
							/* Create a dummy candidate and enforce it as the one to use for this now unneeded component */
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
								JANUS_LOG(LOG_ERR, "[%"SCNu64"] Error forcing dummy candidate on RTCP component of stream %d\n", handle->handle_id, handle->audio_stream->stream_id);
								nice_candidate_free(c);
							}
						}
						if(handle->video_stream && handle->video_stream->components != NULL) {
							nice_agent_attach_recv(handle->agent, handle->video_id, 2, g_main_loop_get_context (handle->iceloop), NULL, NULL);
							/* Free the component */
							janus_ice_component_free(handle->video_stream->components, handle->video_stream->rtcp_component);
							handle->video_stream->rtcp_component = NULL;
							/* Create a dummy candidate and enforce it as the one to use for this now unneeded component */
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
								JANUS_LOG(LOG_ERR, "[%"SCNu64"] Error forcing dummy candidate on RTCP component of stream %d\n", handle->handle_id, handle->video_stream->stream_id);
								nice_candidate_free(c);
							}
						}
					}
					/* FIXME Any disabled m-line? */
if(strstr(jsep_sdp, "m=audio 0")) {
						JANUS_LOG(LOG_VERB, "[%"SCNu64"] Audio disabled via SDP\n", handle->handle_id);
						if(!janus_flags_is_set(&handle->webrtc_flags, JANUS_ICE_HANDLE_WEBRTC_BUNDLE)
								|| (!video && !data)) {
							JANUS_LOG(LOG_HUGE, "  -- Marking audio stream as disabled\n");
							janus_ice_stream *stream = g_hash_table_lookup(handle->streams, GUINT_TO_POINTER(handle->audio_id));
							if(stream)
								stream->disabled = TRUE;
						}
					}

if(strstr(jsep_sdp, "m=video 0")) {
						JANUS_LOG(LOG_VERB, "[%"SCNu64"] Video disabled via SDP\n", handle->handle_id);
						if(!janus_flags_is_set(&handle->webrtc_flags, JANUS_ICE_HANDLE_WEBRTC_BUNDLE)
								|| (!audio && !data)) {
							JANUS_LOG(LOG_HUGE, "  -- Marking video stream as disabled\n");
							janus_ice_stream *stream = NULL;
							if(!janus_flags_is_set(&handle->webrtc_flags, JANUS_ICE_HANDLE_WEBRTC_BUNDLE)) {
								stream = g_hash_table_lookup(handle->streams, GUINT_TO_POINTER(handle->video_id));
							} else {
								gint id = handle->audio_id > 0 ? handle->audio_id : handle->video_id;
								stream = g_hash_table_lookup(handle->streams, GUINT_TO_POINTER(id));
							}
							if(stream)
								stream->disabled = TRUE;
						}
					}
					
if(strstr(jsep_sdp, "m=application 0 DTLS/SCTP")) {
						JANUS_LOG(LOG_VERB, "[%"SCNu64"] Data Channel disabled via SDP\n", handle->handle_id);
						if(!janus_flags_is_set(&handle->webrtc_flags, JANUS_ICE_HANDLE_WEBRTC_BUNDLE)
								|| (!audio && !video)) {
							JANUS_LOG(LOG_HUGE, "  -- Marking data channel stream as disabled\n");
							janus_ice_stream *stream = NULL;
							if(!janus_flags_is_set(&handle->webrtc_flags, JANUS_ICE_HANDLE_WEBRTC_BUNDLE)) {
								stream = g_hash_table_lookup(handle->streams, GUINT_TO_POINTER(handle->data_id));
							} else {
								gint id = handle->audio_id > 0 ? handle->audio_id : (handle->video_id > 0 ? handle->video_id : handle->data_id);
								stream = g_hash_table_lookup(handle->streams, GUINT_TO_POINTER(id));
							}
							if(stream)
								stream->disabled = TRUE;
						}
					}
					
					
					
					
					
					
					
					
					
					
					
					
					
					
					
					
					/* We got our answer */
					janus_flags_clear(&handle->webrtc_flags, JANUS_ICE_HANDLE_WEBRTC_PROCESSING_OFFER);
					/* Any pending trickles? */
					
					
					if(handle->pending_trickles) {
						JANUS_LOG(LOG_VERB, "[%"SCNu64"]   -- Processing %d pending trickle candidates\n", handle->handle_id, g_list_length(handle->pending_trickles));
						GList *temp = NULL;
						while(handle->pending_trickles) {
							temp = g_list_first(handle->pending_trickles);
							handle->pending_trickles = g_list_remove_link(handle->pending_trickles, temp);
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
								if((error = janus_ice_trickle_parse(handle, candidate, &error_string)) != 0) {
									/* FIXME We should report the error parsing the trickle candidate */
								}
							} else if(json_is_array(candidate)) {
								/* We got multiple candidates in an array */
								JANUS_LOG(LOG_VERB, "Got multiple candidates (%zu)\n", json_array_size(candidate));
								if(json_array_size(candidate) > 0) {
									/* Handle remote candidates */
									size_t i = 0;
									for(i=0; i<json_array_size(candidate); i++) {
										json_t *c = json_array_get(candidate, i);
										/* FIXME We don't care if any trickle fails to parse */
										janus_ice_trickle_parse(handle, c, NULL);
									}
								}
							}
							/* Done, free candidate */
							janus_ice_trickle_destroy(trickle);
						}
					}
					
					
					
					
					
					
					
					
					
					
					
					
					/* This was an answer, check if it's time to start ICE */
					if(janus_flags_is_set(&handle->webrtc_flags, JANUS_ICE_HANDLE_WEBRTC_TRICKLE) &&
							!janus_flags_is_set(&handle->webrtc_flags, JANUS_ICE_HANDLE_WEBRTC_ALL_TRICKLES)) {
						JANUS_LOG(LOG_VERB, "[%"SCNu64"]   -- ICE Trickling is supported by the browser, waiting for remote candidates...\n", handle->handle_id);
						janus_flags_set(&handle->webrtc_flags, JANUS_ICE_HANDLE_WEBRTC_START);
					} 
					
					else {
						JANUS_LOG(LOG_VERB, "[%"SCNu64"] Done! Sending connectivity checks...\n", handle->handle_id);
						if(handle->audio_id > 0) {
							janus_ice_setup_remote_candidates(handle, handle->audio_id, 1);
							if(!janus_flags_is_set(&handle->webrtc_flags, JANUS_ICE_HANDLE_WEBRTC_RTCPMUX))	/* http://tools.ietf.org/html/rfc5761#section-5.1.3 */
								janus_ice_setup_remote_candidates(handle, handle->audio_id, 2);
						}
						if(handle->video_id > 0) {
							janus_ice_setup_remote_candidates(handle, handle->video_id, 1);
							if(!janus_flags_is_set(&handle->webrtc_flags, JANUS_ICE_HANDLE_WEBRTC_RTCPMUX))	/* http://tools.ietf.org/html/rfc5761#section-5.1.3 */
								janus_ice_setup_remote_candidates(handle, handle->video_id, 2);
						}
						if(handle->data_id > 0) {
							janus_ice_setup_remote_candidates(handle, handle->data_id, 1);
						}
					}
					
					
					
					
					
					
					
					
				}
			}
else {
				/* TODO Actually handle session updates: for now we ignore them, and just relay them to plugins */
				JANUS_LOG(LOG_WARN, "[%"SCNu64"] Ignoring negotiation update, we don't support them yet...\n", handle->handle_id);
			}
	
	
	
	
			handle->remote_sdp = g_strdup(jsep_sdp);
			janus_mutex_unlock(&handle->mutex);
			/* Anonymize SDP */
			if(janus_sdp_anonymize(parsed_sdp) < 0) {
				/* Invalid SDP */
				ret = janus_process_error(request, session_id, transaction_text, JANUS_ERROR_JSEP_INVALID_SDP, "JSEP error: invalid SDP");
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
		
		
		
		
		/* Make sure the app handle is still valid */
		if(handle->app == NULL || handle->app_handle == NULL || !janus_plugin_session_is_alive(handle->app_handle)) {
			ret = janus_process_error(request, session_id, transaction_text, JANUS_ERROR_PLUGIN_MESSAGE, "No plugin to handle this message");
			g_free(jsep_type);
			g_free(jsep_sdp_stripped);
			janus_flags_clear(&handle->webrtc_flags, JANUS_ICE_HANDLE_WEBRTC_PROCESSING_OFFER);
			goto jsondone;
		}

		/* Send the message to the plugin (which must eventually free transaction_text and unref the two objects, body and jsep) */
		json_incref(body);
		json_t *body_jsep = NULL;
		if(jsep_sdp_stripped) {
			body_jsep = json_pack("{ssss}", "type", jsep_type, "sdp", jsep_sdp_stripped);
			/* Check if VP8 simulcasting is enabled */
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
		};
		janus_plugin_result *result = plugin_t->handle_message(handle->app_handle,
			g_strdup((char *)transaction_text), body, body_jsep);
		g_free(jsep_type);
		g_free(jsep_sdp_stripped);
		if(result == NULL) {
			/* Something went horribly wrong! */
			ret = janus_process_error(request, session_id, transaction_text, JANUS_ERROR_PLUGIN_MESSAGE, "Plugin didn't give a result");
			goto jsondone;
		}
		if(result->type == JANUS_PLUGIN_OK) {
			/* The plugin gave a result already (synchronous request/response) */
			if(result->content == NULL || !json_is_object(result->content)) {
				/* Missing content, or not a JSON object */
				ret = janus_process_error(request, session_id, transaction_text, JANUS_ERROR_PLUGIN_MESSAGE,
					result->content == NULL ?
						"Plugin didn't provide any content for this synchronous response" :
						"Plugin returned an invalid JSON response");
				janus_plugin_result_destroy(result);
				goto jsondone;
			}
			/* Reference the content, as destroying the result instance will decref it */
			json_incref(result->content);
			/* Prepare JSON response */
			json_t *reply = json_object();
			json_object_set_new(reply, "janus", json_string("success"));
			json_object_set_new(reply, "session_id", json_integer(session->session_id));
			json_object_set_new(reply, "sender", json_integer(handle->handle_id));
			json_object_set_new(reply, "transaction", json_string(transaction_text));
			json_t *plugin_data = json_object();
			json_object_set_new(plugin_data, "plugin", json_string(plugin_t->get_package()));
			json_object_set_new(plugin_data, "data", result->content);
			json_object_set_new(reply, "plugindata", plugin_data);
			/* Send the success reply */
			ret = janus_process_success(request, reply);
		} 
		else if(result->type == JANUS_PLUGIN_OK_WAIT) {
			/* The plugin received the request but didn't process it yet, send an ack (asynchronous notifications may follow) */
			json_t *reply = json_object();
			json_object_set_new(reply, "janus", json_string("ack"));
			json_object_set_new(reply, "session_id", json_integer(session_id));
			if(result->text)
				json_object_set_new(reply, "hint", json_string(result->text));
			json_object_set_new(reply, "transaction", json_string(transaction_text));
			/* Send the success reply */
			ret = janus_process_success(request, reply);
		} 
		else {
			/* Something went horribly wrong! */
			ret = janus_process_error_string(request, session_id, transaction_text, JANUS_ERROR_PLUGIN_MESSAGE,
				(char *)(result->text ? result->text : "Plugin returned a severe (unknown) error"));
			janus_plugin_result_destroy(result);
			goto jsondone;
		}
		janus_plugin_result_destroy(result);
		
		
		
		
		
		
		
		
	}
	
	
	else if(!strcasecmp(message_text, "trickle")) {
		if(handle == NULL) {
			/* Trickle is an handle-level command */
			ret = janus_process_error(request, session_id, transaction_text, JANUS_ERROR_INVALID_REQUEST_PATH, "Unhandled request '%s' at this path", message_text);
			goto jsondone;
		}
		if(handle->app == NULL || handle->app_handle == NULL || !janus_plugin_session_is_alive(handle->app_handle)) {
			ret = janus_process_error(request, session_id, transaction_text, JANUS_ERROR_PLUGIN_MESSAGE, "No plugin to handle this trickle candidate");
			goto jsondone;
		}
		json_t *candidate = json_object_get(root, "candidate");
		json_t *candidates = json_object_get(root, "candidates");
		if(candidate == NULL && candidates == NULL) {
			ret = janus_process_error(request, session_id, transaction_text, JANUS_ERROR_MISSING_MANDATORY_ELEMENT,
									  "Missing mandatory element (candidate|candidates)");
			goto jsondone;
		}
		if(candidate != NULL && candidates != NULL) {
			ret = janus_process_error(request, session_id, transaction_text, JANUS_ERROR_INVALID_JSON, "Can't have both candidate and candidates");
			goto jsondone;
		}
		janus_mutex_lock(&handle->mutex);
		if(!janus_flags_is_set(&handle->webrtc_flags, JANUS_ICE_HANDLE_WEBRTC_TRICKLE)) {
			/* It looks like this peer supports Trickle, after all */
			JANUS_LOG(LOG_VERB, "Handle %"SCNu64" supports trickle even if it didn't negotiate it...\n", handle->handle_id);
			janus_flags_set(&handle->webrtc_flags, JANUS_ICE_HANDLE_WEBRTC_TRICKLE);
		}
		/* Is there any stream ready? this trickle may get here before the SDP it relates to */
		if(handle->audio_stream == NULL && handle->video_stream == NULL && handle->data_stream == NULL) {
			JANUS_LOG(LOG_WARN, "[%"SCNu64"] No stream, queueing this trickle as it got here before the SDP...\n", handle->handle_id);
			/* Enqueue this trickle candidate(s), we'll process this later */
			janus_ice_trickle *early_trickle = janus_ice_trickle_new(handle, transaction_text, candidate ? candidate : candidates);
			handle->pending_trickles = g_list_append(handle->pending_trickles, early_trickle);
			/* Send the ack right away, an event will tell the application if the candidate(s) failed */
			goto trickledone;
		}
		/* Is the ICE stack ready already? */
		if(janus_flags_is_set(&handle->webrtc_flags, JANUS_ICE_HANDLE_WEBRTC_PROCESSING_OFFER) ||
				!janus_flags_is_set(&handle->webrtc_flags, JANUS_ICE_HANDLE_WEBRTC_GOT_OFFER) ||
				!janus_flags_is_set(&handle->webrtc_flags, JANUS_ICE_HANDLE_WEBRTC_GOT_ANSWER)) {
			const char *cause = NULL;
			if(janus_flags_is_set(&handle->webrtc_flags, JANUS_ICE_HANDLE_WEBRTC_PROCESSING_OFFER))
				cause = "processing the offer";
			else if(!janus_flags_is_set(&handle->webrtc_flags, JANUS_ICE_HANDLE_WEBRTC_GOT_ANSWER))
				cause = "waiting for the answer";
			else if(!janus_flags_is_set(&handle->webrtc_flags, JANUS_ICE_HANDLE_WEBRTC_GOT_OFFER))
				cause = "waiting for the offer";
			JANUS_LOG(LOG_VERB, "[%"SCNu64"] Still %s, queueing this trickle to wait until we're done there...\n",
				handle->handle_id, cause);
			/* Enqueue this trickle candidate(s), we'll process this later */
			janus_ice_trickle *early_trickle = janus_ice_trickle_new(handle, transaction_text, candidate ? candidate : candidates);
			handle->pending_trickles = g_list_append(handle->pending_trickles, early_trickle);
			/* Send the ack right away, an event will tell the application if the candidate(s) failed */
			goto trickledone;
		}
		if(candidate != NULL) {
			/* We got a single candidate */
			int error = 0;
			const char *error_string = NULL;
			if((error = janus_ice_trickle_parse(handle, candidate, &error_string)) != 0) {
				ret = janus_process_error(request, session_id, transaction_text, error, "%s", error_string);
				janus_mutex_unlock(&handle->mutex);
				goto jsondone;
			}
		} 
		else {
			/* We got multiple candidates in an array */
			if(!json_is_array(candidates)) {
				ret = janus_process_error(request, session_id, transaction_text, JANUS_ERROR_INVALID_ELEMENT_TYPE, "candidates is not an array");
				janus_mutex_unlock(&handle->mutex);
				goto jsondone;
			}
			JANUS_LOG(LOG_VERB, "Got multiple candidates (%zu)\n", json_array_size(candidates));
			if(json_array_size(candidates) > 0) {
				/* Handle remote candidates */
				size_t i = 0;
				for(i=0; i<json_array_size(candidates); i++) {
					json_t *c = json_array_get(candidates, i);
					/* FIXME We don't care if any trickle fails to parse */
					janus_ice_trickle_parse(handle, c, NULL);
				}
			}
		}

trickledone:
		janus_mutex_unlock(&handle->mutex);
		/* We reply right away, not to block the web server... */
		json_t *reply = json_object();
		json_object_set_new(reply, "janus", json_string("ack"));
		json_object_set_new(reply, "session_id", json_integer(session_id));
		json_object_set_new(reply, "transaction", json_string(transaction_text));
		/* Send the success reply */
		ret = janus_process_success(request, reply);
	} 
	else {
	ret = janus_process_error(request, session_id, transaction_text, JANUS_ERROR_UNKNOWN_REQUEST, "Unknown request '%s'", message_text);
	}

jsondone:
	return ret;
}