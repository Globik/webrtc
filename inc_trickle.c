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
