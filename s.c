#include <stdio.h>
#include <stdlib.h>
#include <string.h>
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
int janus_log_level=3;
gboolean janus_log_timestamps=FALSE;
gboolean janus_log_colors=TRUE;
int lock_debug=0;
//include "sdp.h" uhuh me what
void janus_session_notify_event(janus_session *session, json_t *event){}
void janus_set_public_ip(const char *ip) {}
gint janus_is_stopping(void) {}
gchar *janus_get_public_ip(void) {}
int main(){
const char*sdp="v=0\r\n"\
"o=- 8390502800549818343 2 IN IP4 127.0.0.1\r\n"\
"s=-\r\n"\
"t=0 0\r\n"\
"a=group:BUNDLE data\r\n"\
"a=msid-semantic: WMS\r\n"\
"m=application 9 DTLS/SCTP 5000\r\n"\
"c=IN IP4 0.0.0.0\r\n"\
"b=AS:30\r\n"\
"a=ice-ufrag:DnTf\r\n"\
"a=ice-pwd:NlPxfbTtKe9O4U9iXWyNAZpz\r\n"\
"a=ice-options:trickle\r\n"\
"a=fingerprint:sha-256 E2:2E:0E:FA:29:9F:0F:06:D8:21:92:FA:CC:12:1A:A9:EB:F7:64:D9:55:9E:56:76:60:0B:41:A2:0D:4C:07:33\r\n"\
"a=setup:active\r\n"\
"a=mid:data\r\n"\
"a=sctpmap:5000 webrtc-datachannel 1024\r\n";
char error_str[512];
	int audio = 0, video = 0, data = 0, bundle = 0, rtcpmux = 0, trickle = 0;
janus_sdp *parsed_sdp = janus_sdp_preparse(sdp, error_str, sizeof(error_str), &audio, &video, &data, &bundle, &rtcpmux, &trickle);
	if(parsed_sdp==NULL){printf("err %s\n",error_str);}
	printf("audio: %d\n",audio);
	printf("video: %d\n",video);
	printf("data: %d\n",data);
	printf("bundle: %d\n",bundle);
	printf("rtcpmux: %d\n",rtcpmux);
	printf("trickle: %d\n",trickle);
return 0;
}