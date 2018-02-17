#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h> // usleep
#include <dlfcn.h>
#include <dirent.h>
#include <net/if.h>
#include <netdb.h>
#include <sys/resource.h>
#include <sys/stat.h>
#include <signal.h>
#include <glib.h>
#include "plugin.h"
#define SHLIB_EXT ".so"


static GMainContext *sess_watchdog_ctx=NULL;

/*
j_plugin *plugin_create(void);//p
int plugin_init(j_cbs*cb);//p
void plugin_destroy(void);//p
struct j_plugin_res *plugin_handle_message(char*transaction);//p


static j_plugin p_m=J_PLUGIN_INIT(
		.init=plugin_init,
		.destroy=plugin_destroy,
		.handle_message=plugin_handle_message,
		);//p
j_plugin *plugin_create(void){
printf("Created!\n");
return &p_m;
}//p
static volatile gint initialized=0,stopping=0;//p
static j_cbs *gw=NULL;//p
static GThread *handler_thread;//p
static void *plugin_handler(void*data);//p
typedef struct j_message{
char*transaction;
} j_message;//p
static GAsyncQueue *messages=NULL;//p
static j_message exit_message;//p


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
}//p

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
}//p

struct j_plugin_res *plugin_handle_message(char*transaction){
if(g_atomic_int_get(&stopping) || !g_atomic_int_get(&initialized))
	return j_plugin_res_new(J_PLUGIN_ERROR, g_atomic_int_get(&stopping) ? "shutting down" : "plugin not initialized");
	j_message*msg=g_malloc(sizeof(j_message));
	msg->transaction=transaction;
	g_async_queue_push(messages,msg);
	return j_plugin_res_new(J_PLUGIN_OK_WAIT,"i'm taking my time");
}//p
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
}//p


*/




int j_plugin_push_event(j_plugin*plugin,const char*transaction);
static gboolean j_check_sess(gpointer);

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
	j_plugin *janus_plugin =NULL;
	signal(SIGINT, j_handle_signal);
	signal(SIGTERM, j_handle_signal);
	atexit(j_termination_handler);

	g_print(" Setup Glib \n");
	
	sess_watchdog_ctx=g_main_context_new();
	GMainLoop *watchdog_loop=g_main_loop_new(sess_watchdog_ctx,FALSE);
	GError *err=NULL;
	GThread *watchdog=g_thread_try_new("sess",&j_sess_watchdog,watchdog_loop,&err);
	if(err !=NULL){
	printf("fatal err trying to start sess watchdog\n");
	exit(1);
	}
	
	struct dirent *pluginent = NULL;
	const char *path=NULL;
	DIR *dir=NULL;
	path="/home/globik/webrtc/thread1/plugin";
	
	g_print("Plugins folder: %s\n", path);
	dir = opendir(path);
	if(!dir) {
		g_print("\tCouldn't access plugins folder...\n");
		exit(1);
	}
	
	char pluginpath[1024];
	while((pluginent = readdir(dir))) {
		int len = strlen(pluginent->d_name);
		if (len < 4) {
			continue;
		}
		if (strcasecmp(pluginent->d_name+len-strlen(SHLIB_EXT), SHLIB_EXT)) {
			continue;
		}
		
		g_print("LOADING PLUGIN %s\n",pluginent->d_name);
		
		memset(pluginpath, 0, 1024);
		g_snprintf(pluginpath, 1024, "%s/%s", path, pluginent->d_name);
		void *plugin = dlopen(pluginpath, RTLD_NOW | RTLD_GLOBAL);
		if (!plugin) {
			g_print("\tCouldn't load plugin '%s': %s\n", pluginent->d_name, dlerror());
		} else {
			create_p *create = (create_p*) dlsym(plugin, "plugin_create");
			const char *dlsym_error = dlerror();
			if (dlsym_error) {
				g_print( "\tCouldn't load symbol 'plugin_create': %s\n", dlsym_error);
				continue;
			}
			
			//j_plugin *
				janus_plugin = create();
			if(!janus_plugin) {
				g_print("\tCouldn't use function 'plugin_create'...\n");
				continue;
			}
			/* Are all the mandatory methods and callbacks implemented? */
			if(!janus_plugin->init || !janus_plugin->destroy ||!janus_plugin->handle_message ) {
				g_print("\tMissing some mandatory methods/callbacks, skipping this plugin...\n");
				continue;
			}
		
			if(janus_plugin->init(&j_handler_plugin) < 0) {
				g_print( "The  plugin could not be initialized\n");
				dlclose(plugin);
				continue;
			}
			/*
			if(plugins == NULL)
				plugins = g_hash_table_new(g_str_hash, g_str_equal);
			g_hash_table_insert(plugins, (gpointer)janus_plugin->get_package(), janus_plugin);
			if(plugins_so == NULL)
				plugins_so = g_hash_table_new(g_str_hash, g_str_equal);
			g_hash_table_insert(plugins_so, (gpointer)janus_plugin->get_package(), plugin);
			*/
		}
	}
	
	
	closedir(dir);
	
	
	
	
	
	
	// PLUGIN!!!!!!!!!!!!!!
	if(janus_plugin !=NULL) {
		/*
	j_plugin *plugin=plugin_create();
	if(!plugin){g_print("create plugin failer\n");exit(1);}
	if(plugin->init(&j_handler_plugin) < 0){
	g_print("no plugin init\n");
		exit(1);
	}
	*/
	j_plugin_res *resu=janus_plugin->handle_message("dudka_DUDKA");
	if(resu==NULL){g_print("resu is null\n");}
	if(resu->type==J_PLUGIN_OK){g_print("j_plugin_ok\n");}
	if(resu->type==J_PLUGIN_OK_WAIT){g_print("J_PLUGIN_OK_WAIT: %s\n",resu->text);}
	
	//int res=gw->push_event(plugin,"Fucker");
	//printf("res of gw->push_event(fucker): %d\n",res);
	
	j_plugin_res_destroy(resu);
	}
	
	while(!g_atomic_int_get(&stop)){
	usleep(250000);
	}
	
	
	g_print("ending watchdog loop\n");
	g_main_loop_quit(watchdog_loop);
	g_thread_join(watchdog);
	watchdog=NULL;
	g_main_loop_unref(watchdog_loop);
	g_main_context_unref(sess_watchdog_ctx);
	sess_watchdog_ctx=NULL;
	if(janus_plugin !=NULL) janus_plugin->destroy();
	g_print("Bye!\n");
	exit(0);
}
