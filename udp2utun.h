



#ifndef UDP2UTUN_H__
#define UDP2UTUN_H__


#include <stdio.h> //printf
#include <string.h> //memset
#include <stdlib.h> //exit(0);
#include <arpa/inet.h>
#include <sys/socket.h>
#include "plugins/plugin.h"


#include <glib.h>



typedef struct udp_handle {

	struct sockaddr_in remote_addr;
    int slen;

	int local_socket_id;
	int remote_socket_id;
	janus_plugin *plugin;
	janus_plugin_session *plugin_session;

	GThread* recv_thread;

}udp_handle;


udp_handle* init_udp_handle(const char *remote_ip, const unsigned int  remote_port, const unsigned int local_port);
int udp_close(udp_handle* handle);
void udp_free(udp_handle * handle);
int udp_send_handle(udp_handle *handle, char* buf, int len);

#endif



