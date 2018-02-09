#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <glib.h>
#include "plugin.h"


static GMainContext *sess_watchdog_ctx=NULL;
j_plugin *plugin_create(void);
int plugin_init(j_cbs*cb);
void plugin_destroy(void);

j_plugin_res*j_plugin_res_new(j_plugin_res_type type,const char*text){
j_plugin_res *result=g_malloc(sizeof(j_plugin_res));
	result->type=type;
	result->text=text;
	return result;
}
void j_plugin_res_destroy(j_plugin_res *result){
result->text=NULL;
	g_free(result);
	g_print("j_plugin_res_destroy\n");
}

struct j_plugin_res *plugin_handle_message(char*transaction);

static j_plugin p_m=J_PLUGIN_INIT(
		.init=plugin_init,
		.destroy=plugin_destroy,
		.handle_message=plugin_handle_message,
		);
j_plugin *plugin_create(void){
printf("Created!\n");
return &p_m;
}
static volatile gint initialized=0,stopping=0;
static j_cbs *gw=NULL;
static GThread *handler_thread;
static void *plugin_handler(void*data);
typedef struct j_message{
	char*transaction;
} j_message;
static GAsyncQueue *messages=NULL;
static j_message exit_message;
static void plugin_message_free(j_message *msg){
	g_print("ENTERING plugin_message_free\n");
if(!msg || msg==&exit_message) return;
	g_free(msg->transaction);
	msg->transaction=NULL;
	g_free(msg);
	g_print("plugin_message_free\n");
}

int plugin_init(j_cbs *cbs){
if(g_atomic_int_get(&stopping)){return -1;}
	if(cbs==NULL){return -1;}
messages=g_async_queue_new_full((GDestroyNotify)plugin_message_free);
	gw=cbs;
	g_atomic_int_set(&initialized,1);
	GError *error=NULL;
	handler_thread=g_thread_try_new("echotest",plugin_handler,NULL,&error);
	if(error !=NULL){
	g_atomic_int_set(&initialized,0);
	printf("got error handler_thread: %d\n",error->code);
	return -1;
	}
	printf("Initialized!\n");
return 0;
}

void plugin_destroy(void){
if(!g_atomic_int_get(&initialized)) return;
	g_atomic_int_set(&stopping,1);
	g_async_queue_push(messages,&exit_message);
	if(handler_thread !=NULL){
	g_thread_join(handler_thread);
		handler_thread=NULL;
	}
	g_async_queue_unref(messages);
	messages=NULL;
	g_atomic_int_set(&initialized,0);
	g_atomic_int_set(&stopping,0);
	printf("Destroyd\n");
}

struct j_plugin_res *plugin_handle_message(char*transaction){
if(g_atomic_int_get(&stopping) || !g_atomic_int_get(&initialized))
	return j_plugin_res_new(J_PLUGIN_ERROR, g_atomic_int_get(&stopping) ? "shutting down" : "plugin not initialized");
	j_message*msg=g_malloc(sizeof(j_message));
	msg->transaction=transaction;
	g_async_queue_push(messages,msg);
	return j_plugin_res_new(J_PLUGIN_OK_WAIT,"i'm taking my time");
}
static void*plugin_handler(void*data){
j_message *msg=NULL;
	while(g_atomic_int_get(&initialized) && !g_atomic_int_get(&stopping)){
	msg=g_async_queue_pop(messages);
		if(msg==&exit_message) break;
		if(msg->transaction==NULL){
		plugin_message_free(msg);
			continue;
		}
		printf("got a message: %s\n",msg->transaction);
	//	int res=gw->push_event("Fucker");
	//	printf("res of gw->push_event(fucker): %d\n",res);
		continue;
	}
	
	g_print("leaving thread from PLUGIN_HANDLER\n");
	return NULL;
}

int j_plugin_push_event(j_plugin*plugin,const char*transaction);

static j_cbs j_handler_plugin={
.push_event=j_plugin_push_event,
};
int j_plugin_push_event(j_plugin *plugin,const char*transaction){
	if(!plugin) return -1;
printf("TRANSACTION: %s\n",transaction); // here must be "fucker" ...
return 0;
}

static gboolean j_check_sess(gpointer user_data){
	g_print("tick-tack\n");
return G_SOURCE_CONTINUE;
}
static volatile gint stop=0;
static gint stop_signal=0;
static gint is_stopping(void){return g_atomic_int_get(&stop);}

static void j_handle_signal(int signum) {
	stop_signal = signum;
	switch(g_atomic_int_get(&stop)) {
		case 0:
			g_print("Stopping gateway, please wait...\n");
			break;
		case 1:
			g_print("In a hurry? I'm trying to free resources cleanly, here!\n");
			break;
		default:
			g_print("Ok, leaving immediately...\n");
			break;
	}
	g_atomic_int_inc(&stop);
	if(g_atomic_int_get(&stop) > 2)
		exit(1);
}


static gpointer j_sess_watchdog(gpointer user_data){
GMainLoop *loop=(GMainLoop *)user_data;
	GMainContext *watchdog_ctx=g_main_loop_get_context(loop);
	GSource *timeout_source=g_timeout_source_new_seconds(2);
	g_source_set_callback(timeout_source,j_check_sess,watchdog_ctx,NULL);
	g_source_attach(timeout_source,watchdog_ctx);
	g_source_unref(timeout_source);
	printf("sess watchdog started\n");
	g_main_loop_run(loop);
	return NULL;
}

static void j_termination_handler(void) {}
int main(){
	signal(SIGINT, j_handle_signal);
	signal(SIGTERM, j_handle_signal);
	atexit(j_termination_handler);

	g_print(" Setup Glib \n");
	
	j_plugin *plugin=plugin_create();
	if(!plugin){g_print("create plugin failer\n");exit(1);}
	if(plugin->init(&j_handler_plugin) < 0){
	g_print("no plugin init\n");
		exit(1);
	}
	j_plugin_res *resu=plugin->handle_message("dudka");
	if(resu==NULL){g_print("resu is null\n");}
	if(resu->type==J_PLUGIN_OK){g_print("j_plugin_ok\n");}
	if(resu->type==J_PLUGIN_OK_WAIT){g_print("J_PLUGIN_OK_WAIT: %s\n",resu->text);}
	
	int res=gw->push_event(plugin,"Fucker");
	printf("res of gw->push_event(fucker): %d\n",res);
	
	j_plugin_res_destroy(resu);
	
	sess_watchdog_ctx=g_main_context_new();
	GMainLoop *watchdog_loop=g_main_loop_new(sess_watchdog_ctx,FALSE);
	GError *err=NULL;
	GThread *watchdog=g_thread_try_new("sess",&j_sess_watchdog,watchdog_loop,&err);
	if(err !=NULL){
	printf("fatal err trying to start sess watchdog\n");
	exit(1);
	}
	while(!g_atomic_int_get(&stop)){
	usleep(250000);
	}
	printf("ending watchdog loop\n");
	g_main_loop_quit(watchdog_loop);
	g_thread_join(watchdog);
	watchdog=NULL;
	g_main_loop_unref(watchdog_loop);
	g_main_context_unref(sess_watchdog_ctx);
	sess_watchdog_ctx=NULL;
	plugin->destroy();
	exit(0);
}
