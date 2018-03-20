/*
 * Copyright (c) 2014 Joris Vink <joris@coders.se>
 *
 * Permission to use, copy, modify, and distribute this software for any
 * purpose with or without fee is hereby granted, provided that the above
 * copyright notice and this permission notice appear in all copies.
 *
 * THE SOFTWARE IS PROVIDED "AS IS" AND THE AUTHOR DISCLAIMS ALL WARRANTIES
 * WITH REGARD TO THIS SOFTWARE INCLUDING ALL IMPLIED WARRANTIES OF
 * MERCHANTABILITY AND FITNESS. IN NO EVENT SHALL THE AUTHOR BjjE LIABLE FOR
 * ANY SPECIAL, DIRECT, INDIRECT, OR CONSEQUENTIAL DAMAGES OR ANY DAMAGES
 * WHATSOEVER RESULTING FROM LOSS OF USE, DATA OR PROFITS, WHETHER IN AN
 * ACTION OF CONTRACT, NEGLIGENCE OmjiR OTHER TORTIOUS ACTION, ARISING OUT OF
 * OR IN CONNECTION WITH THE USE OR PERFORMANCE OF THIS SOFTWARE.
 
 Kore is an easy to use web platform for writing scalable web APIs in C.
 Its main goals are security, scalability and allowing rapid development and deployment of such APIs.
 
 
 */
/*
Janus gateway core can be used as a lib, as such it can be implemented into any good web framework written in C/C++.
No need for transport plugins. We are on our own. Custom signaling and authorization.
Here is a kore framework with http and websocket.
A homepage is: https://kore.io
hardcoded to libjanus_echotest.so - Just proof of concept
We are using compiled libjanus.so from janus core, based on glib loop ecosystem
On front end(see assets/frontend.html) the webrtc stuff hardcoded to Datachannel API(for brevity), no audio/video, just proof of concept.

*/

#include <stdio.h>
#include <string.h>

#include <kore/kore.h>
#include <kore/http.h>
#include <kore/tasks.h>

#include <jansson.h>
#include "assets.h"
#include "janus.h"
#include "helper.h"

#define green "\x1b[32m"
#define yellow "\x1b[33m"
#define red "\x1b[31m"
#define rst "\x1b[0m"

const char*transi="trans_janus";
janus_callbacks janus_handler_plugin=
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

volatile gint stop = 0;
gint stop_signal = 0;

static janus_mutex sessions_mutex;

struct kore_task pipe_task;
int		page(struct http_request *);
int		page_ws_connect(struct http_request *);

void		websocket_connect(struct connection *);
void		websocket_disconnect(struct connection *);
void		websocket_message(struct connection *, u_int8_t, void *, size_t);
json_t *load_json(const char *, size_t);


int init(int);
int rtc_loop(struct kore_task *);
void pipe_data_available(struct kore_task *);


int init(state){
//this is init run time(as an entry), defined in conf/kore_janus.conf, a plugin loaded time, kore_janus.so
kore_log(LOG_NOTICE,"Entering init");
if(state==KORE_MODULE_UNLOAD) return (KORE_RESULT_ERROR);
// pick up only one of many processes if you have configured a few processes in conf/kore_janus.conf
// currently a worker process = 1 , hardcoded to one worker process
//if(worker->id !=1) return (KORE_RESULT_OK);
// here we're creating the task(a thread) where can be placed a glib loop with janus stuff
kore_task_create(&pipe_task,rtc_loop);
// bind callback for incoming data from kore task of rtc_loop(), it is a block operation!
kore_task_bind_callback(&pipe_task, pipe_data_available);
//run a task(a thread)
kore_task_run(&pipe_task);
return (KORE_RESULT_OK);
}

json_t *load_json(const char *text,size_t buflen) {
// build-in kore's websocket's support is good for buffers, so we are using here the json_loadb() from libjansson
json_t *root;
json_error_t error;
root = json_loadb(text, buflen, 0, &error);
if (root) {
return root;
} else {
fprintf(stderr, "json error on line %d: %s\n", error.line, error.text);
return (json_t *)0;
}
}
//dummy integer for how many connections we have
int count=0;
void websocket_connect(struct connection *c)
{
count++;
// a dummy struct for tracking the user connection
struct ex *l=kore_malloc(sizeof(*l));
	l->id=count;
// a dummy constant just for fun
	l->b=46;
// the ice handler
	l->sender_id=0;
// binding to opaque data in a given connection, it's useful for a custom tracking
	c->hdlr_extra=l;
//here we go, the janus stuff
guint64 session_id = 0, handle_id = 0;
if(session_id==0 && handle_id==0){
janus_session *session=janus_session_create(session_id);
if(session==NULL){kore_log(LOG_NOTICE,"session is NULL!");}
session_id=session->session_id;
}
	
if(session_id > 0 && janus_session_find(session_id) !=NULL){
kore_log(LOG_NOTICE,"The session already in use");
}
	
    json_auto_t *reply=json_object();
	json_object_set_new(reply,"type",json_string("user_id"));
	json_object_set_new(reply,"msg",json_string("Hallo jason!"));
	json_object_set_new(reply,"id",json_integer(l->id));
	json_object_set_new(reply,"b",json_integer(l->b));
	json_object_set_new(reply,"session_id",json_integer(session_id));
	json_object_set_new(reply,"transaction",json_string(transi));
	size_t size=json_dumpb(reply,NULL,0,0);
	if(size==0){kore_log(LOG_NOTICE,"Size in jason is null\n");return;}
	char*buf=kore_malloc(size);
	size=json_dumpb(reply,buf,size,0);
	g_print("buffer: %s\n",buf);
	kore_websocket_send(c, 1, buf,size);
	kore_free(buf);
	}

void websocket_message(struct connection *c, u_int8_t op, void *data, size_t len)
{
	if(data==NULL) return;
	// here it is, a dummy struct for tracking an user connections
	struct ex * r=c->hdlr_extra;
	kore_log(LOG_NOTICE, "tracking trace: %d id: %d\n",r->b,r->id);
	fwrite((char*)data,1,len,stdout);
	g_print("\n");
	
	
	json_t *root = load_json((const char*)data, len);
	if(root){
	 char*foo=json_dumps(root,0);
	kore_log(LOG_NOTICE,"incoming message: %s",foo);
	int send_to_clients=0;
//Janus stuff		
guint64 session_id=0,handle_id=0;
json_t *s=json_object_get(root,"session_id");
if(s && json_is_integer(s)) session_id=json_integer_value(s);
json_t *h=json_object_get(root,"handle_id");
if(h && json_is_integer(h)) handle_id=json_integer_value(h);
		
janus_session *session=janus_session_find(session_id);
if(!session){
kore_log(LOG_NOTICE,"The session not found. Returning");json_decref(root);return;
}
session->last_activity=janus_get_monotonic_time();
janus_ice_handle *handle=NULL;
if(handle_id > 0){
janus_mutex_lock(&session->mutex);
handle=janus_ice_handle_find(session,handle_id);
janus_mutex_unlock(&session->mutex);
if(!handle){kore_log(LOG_INFO,"handle_id not found");}
}
	
json_t *t=json_object_get(root,"type");
const char*t_txt=json_string_value(t);
if(!strcmp(t_txt,"message")){
kore_log(LOG_NOTICE,"type message");
}else if(!strcmp(t_txt,"login")){
kore_log(LOG_NOTICE,"type login");
}else if(!strcmp(t_txt,"attach")){
kore_log(LOG_NOTICE,"type attach.");
if(handle !=NULL){kore_log(LOG_INFO,"handle is not null");json_decref(root);return;}
janus_mutex_lock(&session->mutex);
handle=janus_ice_handle_create(session,NULL);
if(handle==NULL){
kore_log(LOG_INFO,"Failed to create ice handle.");
janus_mutex_unlock(&session->mutex);
json_decref(root);
return;
}
handle_id=handle->handle_id;
r->sender_id=handle_id;
// hardcoded to "janus.plugin.echotest"
janus_plugin *plugin_t=janus_plugin_find("janus.plugin.echotest");
if(plugin_t==NULL){
kore_log(LOG_INFO,"plugin_t is NULL");
json_decref(root);
return;
}else{kore_log(LOG_INFO,"plugin_t is NOT NULL");}
int error=0;
if((error = janus_ice_handle_attach_plugin(session,handle_id,plugin_t)) !=0){
janus_ice_handle_destroy(session,handle_id);
g_hash_table_remove(session->ice_handles,&handle_id);
janus_mutex_unlock(&session->mutex);
kore_log(LOG_INFO,"Err attach plugin.");
json_decref(root);
return;
}else{kore_log(LOG_INFO,"SUCCESS IN ATTACHING PLUGIN");}
janus_mutex_unlock(&session->mutex);
json_t*reply=json_object();
json_object_set_new(reply,"session_id",json_integer(session_id));
json_object_set_new(reply,"transaction",json_string(transi));
json_object_set_new(reply,"handle_id",json_integer(handle_id));
json_object_set_new(reply,"type",json_string("on_attach"));


size_t size=json_dumpb(reply,NULL,0,0);
if(size==0){kore_log(LOG_INFO, "json_dumpb Size is null\n");}
char*buf=alloca(size);
size=json_dumpb(reply,buf,size,0);
kore_websocket_send(c, 1, buf,size);
send_to_clients=1;
}else if(!strcmp(t_txt,"detach")){
kore_log(LOG_NOTICE,"type detach");
if(handle==NULL){
kore_log(LOG_INFO,"handle is NULL. Returning.");
return;
}
if(handle->app==NULL || handle->app_handle==NULL){
kore_log(LOG_INFO,"No plugin to detach from?");
}
janus_mutex_lock(&session->mutex);
int error=janus_ice_handle_destroy(session,handle_id);
g_hash_table_remove(session->ice_handles,&handle_id);
janus_mutex_unlock(&session->mutex);
if(error !=0){
kore_log(LOG_INFO,"error in ice handle destroy!");
}
json_auto_t *reply=json_object();
json_object_set_new(reply,"type",json_string("on_detach"));
json_object_set_new(reply,"session_id",json_integer(session_id));
json_object_set_new(reply,"transaction",json_string(transi));
size_t size=json_dumpb(reply,NULL,0,0);
if(size==0){kore_log(LOG_INFO, "json_dumpb Size is null\n");}
char*buf=alloca(size);
size=json_dumpb(reply,buf,size,0);
kore_websocket_send(c, 1, buf,size);
send_to_clients=1;
}
else if(!strcmp(t_txt,"destroy")){
kore_log(LOG_NOTICE,"type destroy");
if(handle !=NULL){kore_log(LOG_INFO,"handle is not null. skiping.");json_decref(root);return;}
janus_session_schedule_destruction(session,TRUE,TRUE,TRUE);
json_auto_t *reply=json_object();
json_object_set_new(reply,"type",json_string("on_destroy"));
json_object_set_new(reply,"session_id",json_integer(session_id));
json_object_set_new(reply,"transaction",json_string(transi));
size_t size=json_dumpb(reply,NULL,0,0);
if(size==0){kore_log(LOG_INFO, "json_dumpb Size is null\n");}
char*buf=alloca(size);
size=json_dumpb(reply,buf,size,0);
kore_websocket_send(c, 1, buf,size);
send_to_clients=1;
}
// On offer? answer? // Ja, genau!
else if(!strcmp(t_txt,"janus")){
kore_log(LOG_INFO,"type: janus");
incoming_message(handle,session,root,session_id,c);
send_to_clients=1;
}
// On ice candidate ???	// Ja, genau! Thank you, Lorenzo!
else if(!strcmp(t_txt,"trickle")){
kore_log(LOG_INFO,"Type trickle");
do_trickle(handle,session,root,session_id,c);
send_to_clients=1;		
}else{
kore_log(LOG_NOTICE,"Unknown type");
send_to_clients=1;
}
if(send_to_clients==0) kore_websocket_send(c,op,data,len);	
free(foo);
}else{}
json_decref(root);
}

void websocket_disconnect(struct connection *c)
{
kore_log(LOG_NOTICE, "%p: disconnecting", c);
struct ex*l=c->hdlr_extra;
kore_log(LOG_INFO, "On disconnect: b: %d id: %d\n",l->id,l->b);
count--;
l->sender_id=0;
kore_free(l);
c->hdlr_extra=NULL;
}

int page(struct http_request *req)
{
// serving a static page from assets folder
http_response_header(req, "content-type", "text/html");
http_response(req, 200, asset_frontend_html, asset_len_frontend_html);
return (KORE_RESULT_OK);
}


int page_ws_connect(struct http_request *req)
{
/* Perform the websocket handshake, passing our callbacks. */
kore_log(LOG_NOTICE,"some path %s",req->path);
kore_log(LOG_NOTICE, "%p: http_request", req);
kore_websocket_handshake(req, "websocket_connect","websocket_message","websocket_disconnect");
return (KORE_RESULT_OK);
}

void pipe_data_available(struct kore_task *t){
size_t len;
u_int8_t buf[BUFSIZ];
if(kore_task_finished(t)){
kore_log(LOG_NOTICE,"Task finished.");
exit(0);
}
len=kore_task_channel_read(t,buf,sizeof(buf));
if(len > sizeof(buf)){kore_log(LOG_NOTICE, "len great than buf\n");}
// this messages can be of any use, dunno, may be to send further to the client end through websocket
kore_log(LOG_NOTICE,"Task msg: %s", buf);
}

int rtc_loop(struct kore_task*t){
// starting standalone thread
// Lorenzo's Janus family settled here, see helper.c for fuck_up(), the most part of janus core are there.
// also see handle_sdp.c, incoming.c, dlsym.c
fuck_up(t);
// time to send something to pipe_data_available()
kore_task_channel_write(t,"mama\0",5);
return (KORE_RESULT_OK);
}