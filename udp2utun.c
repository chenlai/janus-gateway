#include <sys/time.h>
#include <errno.h>

#include "udp2utun.h"
#include "debug.h"
#include "rtp.h"

/*#define INADDR_ANY "0.0.0.0"*/
#define BUFLEN 1600

static gboolean recv_thread_active = FALSE;
static char buffer[BUFLEN];


static gpointer udp_recv_handle(gpointer data);

// init handle thread 
static gboolean janus_is_rtcp(gchar *buf) {
    rtp_header *header = (rtp_header *)buf;
    return ((header->type >= 64) && (header->type < 96));
}


udp_handle* init_udp_handle(const char *remote_ip, const unsigned int  remote_port,const unsigned int local_port){

    udp_handle *handle = (udp_handle *)g_malloc0(sizeof(udp_handle));

    if (handle == NULL){
        JANUS_LOG(LOG_ERR, "UDP handle create fail \n");
        return NULL;
    }

// Bind local port 
    JANUS_LOG(LOG_DBG, "[udp] begin bind local ip\n");
    struct sockaddr_in si_me, si_other;
    int s, slen = sizeof(si_other);
    
    //create a UDP socket
    if ((s=socket(AF_INET, SOCK_DGRAM, IPPROTO_UDP)) == -1)
    {
        JANUS_LOG(LOG_ERR, "Local UDP Socket create fail ....\n");
        return NULL;
    }

    si_me.sin_family = AF_INET;
    si_me.sin_port = htons(local_port);
    si_me.sin_addr.s_addr = htonl(INADDR_ANY);

        //bind socket to port
    if( bind(s , (struct sockaddr*)&si_me, sizeof(si_me) ) == -1)
    {
        JANUS_LOG(LOG_ERR, "UDP Handle bind fail ....\n");
        return NULL;
    }

    handle->local_socket_id = s;

    JANUS_LOG(LOG_DBG, "[udp] begin bind remote ip\n");
// Init Remote Socket 
    /*handle.local_addr = si_me;*/
 
    if ( (s=socket(AF_INET, SOCK_DGRAM, IPPROTO_UDP)) == -1)
    {
        JANUS_LOG(LOG_ERR, "Remote UDP Socket create fail ....\n");
        return NULL;
    }
 
    memset((char *) &si_other, 0, sizeof(si_other));

    si_other.sin_family = AF_INET;
    si_other.sin_port = htons(remote_port);
     
    JANUS_LOG(LOG_DBG, "[udp] begin bind remote ip :%s\n", remote_ip);

    if(inet_aton(remote_ip , &si_other.sin_addr) == 0) 
    {

        JANUS_LOG(LOG_ERR, "Remote UDP Socket create fail ....\n");
        return NULL;
    }

    handle->remote_addr = si_other;
    handle->remote_socket_id = s;
    handle->slen = slen;

   // char *recv_name = ;  // TODO: gather handle id.
    GError *error = NULL;

    // Thread pool + select.

//TODO : recv for all handle, poll. will do in Janus init.
     
    // thread begin first init. 

    JANUS_LOG(LOG_DBG, "[udp] create a recv thread");
    handle->recv_thread = g_thread_try_new ("UDP recv Thread", &udp_recv_handle, handle, &error);  

    if(error != NULL) {
        JANUS_LOG(LOG_FATAL, "Got error %d (%s) trying to start UDP RECV thread ...\n", error->code, error->message ? error->message : "??");
        exit(1);
    }

    recv_thread_active = TRUE;
    handle->plugin = NULL;
    handle->plugin_session = NULL;

    return handle;
}

int udp_close(udp_handle* handle){

    JANUS_LOG(LOG_INFO, "[handle id ] udp close ");

    recv_thread_active = FALSE;

    if (handle == NULL){
        return -1;
    }

    if (handle->remote_socket_id > 0)
        close(handle->remote_socket_id);

    if (handle->remote_socket_id > 0)
        close(handle->local_socket_id);

    return 0;

    // free handle outside;
}

void udp_free(udp_handle * handle)
{
    if (handle != NULL)
        free(handle);

}



int udp_send_handle(udp_handle *handle, char* buf, int len){


    JANUS_LOG(LOG_DBG, "[udp] send data ");

    if (handle == NULL){
        JANUS_LOG(LOG_ERR, "Send UDP, handle is NULL");
        return -1;
    }

    int s = handle->remote_socket_id;
           //now reply the client with the same data
    if (sendto(s, buf, len, 0, (struct sockaddr *) &(handle->remote_addr), handle->slen) < 0)
    {

        JANUS_LOG(LOG_ERR, "[handle id] UDP Sendto fail : %s", strerror(errno));
        return -1;
    } 

    return 0;
}



static gpointer udp_recv_handle(gpointer data){

    JANUS_LOG(LOG_DBG, "[udp] recv data ");

    if (data == NULL)
        return NULL;

    udp_handle* handle = (udp_handle*)data;

    fd_set read_fds;
    int max_id = -1;
    int sd = handle->remote_socket_id;

    struct timeval tv;
    int ret;
    int len;

    do {
//TODO: handle list will be set 
        max_id = sd;

        FD_ZERO(&read_fds);
        FD_SET(sd, &read_fds);

        tv.tv_sec = 0;
        tv.tv_usec = 100000;
//TODO : use poll for more 
        ret = select(max_id+1, &read_fds, NULL, NULL, &tv);
        if (ret < 0)
        {
            JANUS_LOG(LOG_ERR, "udp_recv_thread select error");
            /* code */
        }

        if (FD_ISSET(sd, &read_fds)){
            len =recvfrom(sd, buffer, BUFLEN, 0,NULL ,NULL );
            JANUS_LOG(LOG_DBG, "recv_data......%d\n", len);
            if (len < 0)
            {
                JANUS_LOG(LOG_ERR, "[handle id] UDP receve fail : %s", strerror(errno));
                continue; 
            }

            if (handle->plugin != NULL && handle->plugin_session != NULL){
                if(janus_is_rtcp(buffer))
                    handle->plugin->utun_incoming_rtcp(handle->plugin_session, 1, buffer, len);
                else{
                    handle->plugin->utun_incoming_rtp(handle->plugin_session, 1, buffer, len);
                }
            }
            else{
                JANUS_LOG(LOG_ERR, "janus plugin has ");
                break;
            }
        }
    }while(recv_thread_active);


    return NULL;

}
