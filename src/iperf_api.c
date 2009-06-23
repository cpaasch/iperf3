#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <getopt.h>
#include <errno.h>
#include <signal.h>
#include <unistd.h>
#include <assert.h>
#include<fcntl.h>
#include <sys/socket.h>
#include <sys/types.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <netdb.h>
#include <pthread.h> 
#include <stdint.h>
#include <netinet/tcp.h>
#include <sys/time.h>

#include "iperf_api.h"
#include "timer.h"
#include "net.h"
#include "units.h"
#include "tcp_window_size.h"

static struct option longopts[] =
{
{ "client",         required_argument,      NULL,   'c' },
{ "server",         no_argument,            NULL,   's' },
{ "time",           required_argument,      NULL,   't' },
{ "port",           required_argument,      NULL,   'p' },
{ "parallel",       required_argument,      NULL,   'P' },
{ "udp",            no_argument,            NULL,   'u' },
{ "bandwidth",      required_argument,      NULL,   'b' },
{ "length",         required_argument,      NULL,   'l' },
{ "window",         required_argument,      NULL,   'w' },
{"interval",        required_argument,        NULL,    'i'},
{ NULL,             0,                      NULL,   0   }    
};


int getsock_tcp_mss( int inSock )
{
    int theMSS = 0;
 
    int rc;
    socklen_t len;
    assert( inSock >= 0 );
    
    /* query for MSS */
    len = sizeof( theMSS );
    rc = getsockopt( inSock, IPPROTO_TCP, TCP_MAXSEG, (char*) &theMSS, &len );   
  
    return theMSS;
}  

int set_socket_options(struct iperf_stream *sp, struct iperf_test *tp)
{
    
    socklen_t len;
    
    // -N
    if(tp->mNodelay == 1)
    {
        int nodelay = 1;
        len = sizeof(nodelay);
        int rc = setsockopt( sp->socket, IPPROTO_TCP, TCP_NODELAY,
                            (char*) &nodelay, len);
        if(rc == -1)
        {
            perror("TCP_NODELAY");
            return -1;
        }
    }
    
        
    //-M
    if(tp->default_settings->MSS > 0)
    {
        int rc;
        int newMSS;
        len = sizeof(newMSS);
        
        assert( sp->socket != -1);
        
            /* set */
            newMSS = tp->default_settings->MSS;
            len = sizeof( newMSS );
            rc = setsockopt( sp->socket, IPPROTO_TCP, TCP_MAXSEG, (char*) &newMSS, len);
            if ( rc == -1) {
                perror("setsockopt");
                return -1;
            }
            
            /* verify results */
            rc = getsockopt( sp->socket, IPPROTO_TCP, TCP_MAXSEG, (char*) &newMSS, &len);
              if ( newMSS != tp->default_settings->MSS ) 
                perror("mismatch");           
    }        
    return 0;
}

void connect_msg(struct iperf_stream *sp)
{
    char ipl[512], ipr[512];
    
    inet_ntop(AF_INET, (void *) (&((struct sockaddr_in *) &sp->local_addr)->sin_addr), (void *) ipl, sizeof(ipl));
    inet_ntop(AF_INET, (void *) (&((struct sockaddr_in *) &sp->remote_addr)->sin_addr), (void *) ipr, sizeof(ipr));
            
    printf("[%3d] local %s port %d connected with %s port %d\n",
           sp->socket,
           ipl, ntohs(((struct sockaddr_in *) &sp->local_addr)->sin_port),
           ipr, ntohs(((struct sockaddr_in *) &sp->remote_addr)->sin_port) );
}

void Display(struct iperf_test *test)
{
    struct iperf_stream *n;
    n= test->streams;
    int count=1;
    
    printf("===============DISPLAY==================\n");
    
    while(1)
    {    
        if(n)
        {
            if(test->role == 'c')
                printf("position-%d\tsp=%d\tsocket=%d\tbytes sent=%llu\n",count++, (int)n, n->socket, n->result->bytes_sent);
            else
                printf("position-%d\tsp=%d\tsocket=%d\tbytes received=%llu\n",count++, (int)n, n->socket, n->result->bytes_received);
            
            if(n->next==NULL)
            {
                printf("=================END====================\n");
                fflush(stdout);
                break;
            }
            n=n->next;
        }
    }
}

int iperf_tcp_recv(struct iperf_stream *sp)
{
    int result;
    int size = sp->settings->blksize;
    char *buf = (char *) malloc(size);
    if(!buf)
    {
        perror("malloc: unable to allocate receive buffer");
    }
            
    do{
        result = recv(sp->socket, buf, size, 0);    
        
        
    } while (result == -1 && errno == EINTR);
    
    sp->result->bytes_received+= result;    
    return result;
}

int iperf_udp_recv(struct iperf_stream *sp)
{
    
    int result;
    int size = sp->settings->blksize;
    char *buf = (char *) malloc(size);

    if(!buf)
    {
        perror("malloc: unable to allocate receive buffer");
    }
    
    do{
        result = recv(sp->socket, buf, size, 0);
        
    } while (result == -1 && errno == EINTR);
        
    sp->result->bytes_received+= result;
    
    
    return result;    
}

int iperf_tcp_send(struct iperf_stream *sp)
{
    int result,i;
    int size = sp->settings->blksize;
    
    char *buf = (char *) malloc(size);
    if(!buf)
    {
        perror("malloc: unable to allocate transmit buffer");
    }
    for(i=0; i < size; i++)
        buf[i] = i % 37;
    
    result = send(sp->socket, buf, size , 0);    
    sp->result->bytes_sent+= size;
    
    return result;    
}

int iperf_udp_send(struct iperf_stream *sp)
{ 
    int result,i;
    struct timeval before, after;
     int64_t delayus, adjustus, dtargus; 
    int size = sp->settings->blksize;    
    char *buf = (char *) malloc(size);
    if(!buf)
    {
        perror("malloc: unable to allocate transmit buffer");
    }    
    
    dtargus = (int64_t)(sp->settings->blksize) * SEC_TO_US * 8;
    dtargus /= sp->settings->rate;
     
    assert(dtargus != 0);
    
    for(i=0; i < size; i++)
        buf[i] = i % 37;
        
    if(((struct timer *) sp->data)->expired((struct timer *) sp->data))
    {  
        
        if(gettimeofday(&before, 0) < 0)
            perror("gettimeofday");
        
        result = send(sp->socket, buf, size, 0);    
        sp->result->bytes_sent+= result;
        
        if(gettimeofday(&after, 0) < 0)
            perror("gettimeofday");
        
        adjustus = dtargus;
        adjustus += (before.tv_sec - after.tv_sec) * SEC_TO_US ;
        adjustus += (before.tv_usec - after.tv_usec);
        
        printf(" the adjust time = %lld \n",dtargus- adjustus);        
       // if( adjustus > 0) {
           dtargus = adjustus;
        //}
        memcpy(&before, &after, sizeof before);
        
        // RESET THE TIMER
        sp->data = new_timer(0, dtargus);
        printf(" new timer is %lld usec\n", dtargus);
        
    } // timer_expired_micro 
    
    
    return result;
}

struct iperf_test *iperf_new_test()
{
    struct iperf_test *testp;
    
    testp = (struct iperf_test *) malloc(sizeof(struct iperf_test));
    if(!testp)
    {
        perror("malloc");
        return(NULL);
    }
    
    // initialise everything to zero
    memset(testp, 0, sizeof(struct iperf_test));

    testp->default_settings = (struct iperf_settings *) malloc(sizeof(struct iperf_settings));
    memset(testp->default_settings, 0, sizeof(struct iperf_settings));
    
    // return an empty iperf_test* with memory alloted.
    return testp;
}

void iperf_defaults(struct iperf_test *testp)
{
    testp->protocol = Ptcp;
    testp->role = 's';
    testp->duration = 10;
    testp->server_port = 5001;
    testp->state = TEST_START;
    
    testp->mFormat = 'a';    
    
    testp->stats_interval = testp->duration;
    testp->reporter_interval = testp->duration;        
    testp->num_streams = 1;    
    testp->default_settings->socket_bufsize = 1024*1024;
    testp->default_settings->blksize = DEFAULT_TCP_BLKSIZE;
    testp->default_settings->rate = RATE;
}
    
void iperf_init_test(struct iperf_test *test)
{
    char ubuf[UNIT_LEN];
    struct iperf_stream *sp;
    int i, s=0;
    
    if(test->role == 's')
    {                
        test->listener_sock = netannounce(test->protocol, NULL, test->server_port);
        if( test->listener_sock < 0)
            exit(0);
                
        
        if(set_tcp_windowsize( test->listener_sock, test->default_settings->socket_bufsize, SO_RCVBUF) < 0) 
        {
            perror("unable to set window");
        }
        
        printf("-----------------------------------------------------------\n");
        printf("Server listening on %d\n", test->server_port);
        int x;
        if((x = getsock_tcp_windowsize( test->listener_sock, SO_RCVBUF)) < 0) 
            perror("SO_RCVBUF");    
    
        
        unit_snprintf(ubuf, UNIT_LEN, (double) x, 'A');
        printf("%s: %s\n",
               test->protocol == Ptcp ? "TCP window size" : "UDP buffer size", ubuf);
        printf("-----------------------------------------------------------\n");
        
    }
    
    else if( test->role == 'c')
    {
        FD_ZERO(&test->write_set);
        FD_SET(s, &test->write_set);
        
        for(i = 0; i < test->num_streams; i++)
        {
            s = netdial(test->protocol, test->server_hostname, test->server_port);
            
            if(s < 0) 
            {
                fprintf(stderr, "netdial failed\n");
                exit(0);                
            }
            
            FD_SET(s, &test->write_set);
            test->max_fd = (test->max_fd < s) ? s : test->max_fd;
            
            set_tcp_windowsize(sp->socket, test->default_settings->socket_bufsize, SO_SNDBUF);
            
            if(s < 0)
                exit(0);
            
            //setting noblock causes error in byte count -kprabhu
            //setnonblocking(s);
            
            sp = test->new_stream(test);
            sp->socket = s;
            iperf_init_stream(sp, test);
            iperf_add_stream(test, sp);
            connect_msg(sp);        
        }
    }                        
}

void iperf_free_test(struct iperf_test *test)
{
    
    free(test->default_settings);    
    free(test);
}

void *iperf_stats_callback(struct iperf_test *test)
{    
    struct iperf_stream *sp = test->streams;
        
    while(sp)
    {    
        if(test->role == 'c')
            sp->result->interval_results->bytes_transferred = sp->result->bytes_sent  - sp->result->interval_results->bytes_transferred;
        else
            sp->result->interval_results->bytes_transferred = sp->result->bytes_received  - sp->result->interval_results->bytes_transferred;
        
        sp->result->interval_results->interval_duration+= test->stats_interval;
                        
        sp = sp->next;
    }
    
    return 0;
}

char *iperf_reporter_callback(struct iperf_test *test)
{
    char ubuf[UNIT_LEN];
    struct iperf_stream *sp = test->streams;
    iperf_size_t bytes=0;
   // char *messege = (char *) malloc((sizeof("[%d]\t %llu bytes received %s per sec \n") * test->num_streams + 1) + 20 );
    char *messege = (char *) malloc(200);
    char *messege_final = (char *) malloc(test->num_streams * 50 + 200);
    
    if(test->state == TEST_RUNNING) 
    {
    
        printf("\n----------------INTERVAL [%d to %d]----------------\n", sp->result->interval_results->interval_duration - test->stats_interval,
           sp->result->interval_results->interval_duration);
    
        while(sp)
        {    
            if(test->role == 'c')
            {
                bytes+= sp->result->interval_results->bytes_transferred;
                unit_snprintf(ubuf, UNIT_LEN, (double) ( sp->result->interval_results->bytes_transferred / test->stats_interval), test->mFormat);
                printf("[%d]\t %llu bytes sent \t %s per sec \n",sp->socket, sp->result->interval_results->bytes_transferred , ubuf);
            }
            
            else if(test->role == 's')
            {            
                bytes+= sp->result->interval_results->bytes_transferred;
                unit_snprintf(ubuf, UNIT_LEN, (double) ( sp->result->interval_results->bytes_transferred / test->stats_interval), test->mFormat);
                printf("[%d]\t %llu bytes received \t %s per sec \n",sp->socket, sp->result->interval_results->bytes_transferred , ubuf);            
            }
        
            sp = sp->next;                        
        }
       
         printf("---------------------------------------------------\n");
         unit_snprintf(ubuf, UNIT_LEN, (double) ( bytes / test->stats_interval), test->mFormat);
         printf("SUM\t %llu bytes COUNT \t %s per sec \n", bytes , ubuf);
         printf("---------------------------------------------------\n");
       
    }
    // PRINT TOTAL
    
    if(test->state == TEST_END)    
    {
        bytes =0;
        sp= test->streams;        
        printf("\n-----------------------TOTAL-----------------------\n");
        sprintf(messege, "\n-----------------------TOTAL-----------------------\n");
        strcat(messege_final, messege);
        
        while(sp)
        {    
            if(test->role == 'c')
            {
                bytes+= sp->result->bytes_sent;
                
                gettimeofday( &sp->result->end_time, NULL);                
                                
                unit_snprintf(ubuf, UNIT_LEN, (double) (sp->result->bytes_sent /(sp->result->end_time.tv_sec - sp->result->start_time.tv_sec)), test->mFormat);
                printf("[%d]\t %llu bytes sent     %s per sec \n",sp->socket, sp->result->bytes_sent , ubuf);
                
            }
            else if(test->role == 's')
            {
                bytes+= sp->result->bytes_received;
                gettimeofday( &sp->result->end_time, NULL);                
                               
                 unit_snprintf(ubuf, UNIT_LEN, (double) sp->result->bytes_received /(sp->result->end_time.tv_sec - sp->result->start_time.tv_sec), test->mFormat);
                printf("[%d]\t %llu bytes received %s per sec \n",sp->socket, sp->result->bytes_received, ubuf);
                // IF SERVER IS DEAMON, ONLY THIS IS NEEDED
                sprintf(messege,"[%d]\t %llu bytes received %s per sec\n", sp->socket, sp->result->bytes_received, ubuf);
                strcat(messege_final, messege);
                
            }            
            sp = sp->next;        
        }
    
        sp = test->streams;
        
    printf("---------------------------------------------------\n");    
    unit_snprintf(ubuf, UNIT_LEN, (double) bytes /(sp->result->end_time.tv_sec - sp->result->start_time.tv_sec), test->mFormat);
                printf("SUM\t %llu bytes TOTAL    %s per sec \n", bytes , ubuf);
    printf("---------------------------------------------------\n\n");
        
    //IF SERVER IS DEAMON, ONLY THIS IS NEEDED   
        sprintf(messege, "---------------------------------------------------\n");
        strcat(messege_final, messege);
        sprintf(messege, "SUM\t %llu bytes TOTAL    %s per sec \n", bytes , ubuf);
        strcat(messege_final, messege);
        sprintf(messege, "---------------------------------------------------\n\n");
        strcat(messege_final, messege);
                
        // -m option        
        if((test->mPrintMSS != 0)  && (test->role == 'c'))
        {
            printf(" the TCP maximum segment size MSS = %d \n", getsock_tcp_mss(sp->socket));
        }
    
    }   
            
    return messege_final;
    
}

void iperf_free_stream(struct iperf_test *test, struct iperf_stream *sp)
{
    
    struct iperf_stream *prev,*start;
    prev = test->streams;
    start = test->streams;
    
    if(test->streams->socket == sp->socket)
    {        
        test->streams = test->streams->next;        
    }    
    else
    {    
        start= test->streams->next;
        
        while(1)
        {
            if(start->socket == sp->socket){
                
                prev->next = sp->next;                
                break;
            }
            
            if(start->next!=NULL){
                
                start=start->next;
                prev=prev->next;
            }
        }
    }
    sp->settings = NULL;
    free(sp->result);
    free(sp);
       
}

struct iperf_stream *iperf_new_stream(struct iperf_test *testp)
{
    
    struct iperf_stream *sp;
    
    sp = (struct iperf_stream *) malloc(sizeof(struct iperf_stream));
    if(!sp)
    {
        perror("malloc");
        return(NULL);
    }
    
    
    memset(sp, 0, sizeof(struct iperf_stream));
    
    sp->settings = (struct iperf_settings *) malloc(sizeof(struct iperf_settings));
    memcpy(sp->settings, testp->default_settings, sizeof(struct iperf_settings));
    
    sp->result = (struct iperf_stream_result *) malloc(sizeof(struct iperf_stream_result));
    //memset(&sp->result, 0, sizeof(struct iperf_stream_result));
    
    sp->result->interval_results = (struct iperf_interval_results *) malloc(sizeof(struct iperf_interval_results));
    
    sp->socket = -1;
    
    sp->result->bytes_received = 0;
    sp->result->bytes_sent = 0;
    gettimeofday(&sp->result->start_time, NULL);
    
    sp->result->interval_results->bytes_transferred = 0;
    sp->result->interval_results->interval_duration = 0;    
    return sp;
}

struct iperf_stream *iperf_new_tcp_stream(struct iperf_test *testp)
{    
    struct iperf_stream *sp;
        
    sp = (struct iperf_stream *) iperf_new_stream(testp);
    if(!sp) {
        perror("malloc");
        return(NULL);
    }

    sp->rcv = iperf_tcp_recv;
    sp->snd = iperf_tcp_send;
    //sp->update_stats = iperf_tcp_update_stats;
    
    return sp;        
}

struct iperf_stream * iperf_new_udp_stream(struct iperf_test *testp)
{    
    struct iperf_stream *sp;
        
    sp = (struct iperf_stream *) iperf_new_stream(testp);
    if(!sp) {
        perror("malloc");
        return(NULL);
    }
        
    sp->rcv = iperf_udp_recv;
    sp->snd = iperf_udp_send;
    //sp->update_stats = iperf_udp_update_stats;    
   
    return sp;
}

int iperf_udp_accept(struct iperf_test *test)
{
        
    struct iperf_stream *sp;
    struct sockaddr_in sa_peer;    
    char *buf;
    socklen_t len;
    int sz;
                
    buf = (char *) malloc(test->default_settings->blksize);    
    len = sizeof sa_peer;
    
    sz = recvfrom(test->listener_sock, buf, test->default_settings->blksize, 0, (struct sockaddr *) &sa_peer, &len);
        
    if(!sz)
        return -1;
        
    if(connect(test->listener_sock, (struct sockaddr *) &sa_peer, len) < 0)
    {
        perror("connect");
        return -1;
    }
    
    sp = test->new_stream(test);
    sp->socket = test->listener_sock;
    
    iperf_init_stream(sp, test);
    sp->result->bytes_received+= sz;
    iperf_add_stream(test, sp);    
        
    test->listener_sock = netannounce(test->protocol, NULL, test->server_port);
    if(test->listener_sock < 0) 
        return -1;    
    
    
    FD_SET(test->listener_sock, &test->read_set);
    test->max_fd = (test->max_fd < test->listener_sock) ? test->listener_sock : test->max_fd;
    
    connect_msg(sp);
    free(buf);
    
    return 0;
    
}    

int iperf_tcp_accept(struct iperf_test *test)
{
    socklen_t len;
    struct sockaddr_in addr;    
    int peersock;
    struct iperf_stream *sp;
    
    len = sizeof(addr);
    peersock = accept(test->listener_sock, (struct sockaddr *) &addr, &len);
    if (peersock < 0) 
    {
        printf("Error in accept(): %s\n", strerror(errno));
        return -1;
    }
    else 
    {    
        sp = test->new_stream(test);
        //setnonblocking(peersock);
        
        FD_SET(peersock, &test->read_set);
        test->max_fd = (test->max_fd < peersock) ? peersock : test->max_fd;
        
        sp->socket = peersock;        
        iperf_init_stream(sp, test);        
        iperf_add_stream(test, sp);                    
        connect_msg(sp);
        
        return 0;
    }        
}

void iperf_init_stream(struct iperf_stream *sp, struct iperf_test *testp)
{
    socklen_t len;
    int x;
    len = sizeof(struct sockaddr_in);    
    
    sp->protocol = testp->protocol;
    
    if(getsockname(sp->socket, (struct sockaddr *) &sp->local_addr, &len) < 0) 
    {
        perror("getsockname");        
    }
    
        
    //stores the socket id.
    if(getpeername(sp->socket, (struct sockaddr *) &sp->remote_addr, &len) < 0)
    {
        perror("getpeername");
        free(sp);        
    }
    
    
    //set socket options
    
    if(set_tcp_windowsize(sp->socket, testp->default_settings->socket_bufsize, 
                          testp->role == 's' ? SO_RCVBUF : SO_SNDBUF) < 0)
        fprintf(stderr, "unable to set window size\n");
    
    
    x = getsock_tcp_windowsize(sp->socket, SO_RCVBUF);
    if(x < 0)
        perror("SO_RCVBUF");
    // printf("RCV: %d\n", x);
    
    x = getsock_tcp_windowsize(sp->socket, SO_SNDBUF);
    if(x < 0)
        perror("SO_SNDBUF");
    // printf("SND: %d\n", x);    
    
    set_socket_options(sp, testp);
    
}

int iperf_add_stream(struct iperf_test *test, struct iperf_stream *sp)
{
    struct iperf_stream *n;
    
    if(!test->streams){
        test->streams = sp;
        return 1;
    }
    else {
        n = test->streams;
        while(n->next)
            n = n->next;
        n->next = sp;
        return 1;
    }
    
    return 0;
}

struct iperf_stream * find_stream_by_socket(struct iperf_test *test, int sock)
{
    struct iperf_stream *n;
    n=test->streams;
        
    //find the correct stream for update
    while(1)
    {
        if(n->socket == sock)
        {                    
            break;
        }
        
        if(n->next==NULL)
            break;
        
        n = n->next;
    }
    
    return n;    
}

void iperf_run_server(struct iperf_test *test)
{
    struct timeval tv;
    struct iperf_stream *n;   
    int j, result,count= 0;
    char *read = NULL;
    
    FD_ZERO(&test->read_set);
    FD_SET(test->listener_sock, &test->read_set);
    
    test->max_fd = test->listener_sock;
    
    while(1)
    {    
        memcpy(&test->temp_set, &test->read_set,sizeof(test->temp_set));
        tv.tv_sec = 50;            
        tv.tv_usec = 0;
        
        // using select to check on multiple descriptors.
        result = select(test->max_fd + 1, &test->temp_set, NULL, NULL, &tv);
        
        if (result == 0) 
            printf("SERVER IDLE : %d sec\n", (int)tv.tv_sec);
            
        else if (result < 0 && errno != EINTR)
            printf("Error in select(): %s\n", strerror(errno));
        
        // Accept a new connection
        else if(result >0)
        {            
            if (FD_ISSET(test->listener_sock, &test->temp_set))
            {            
                test->accept(test);
                count++;
                FD_CLR(test->listener_sock, &test->temp_set);                                
            }                
            
        // test end message ?
        
        //result_request message ?        
        
        //Process the sockets for read operation
            for (j=0; j< test->max_fd+1; j++)
            {                
                if (FD_ISSET(j, &test->temp_set))
                {                    
                    // find the correct stream
                    n = find_stream_by_socket(test,j);
                    result = n->rcv(n);
                
                    if(result == 0)
                    {    
                        // stream shutdown message
                        count --;                        
                        FD_CLR(j, &test->read_set);                        
                    }
                    else if(result < 0)
                    {
                        printf("Error in recv(): %s\n", strerror(errno));
                    }
                }      // end if (FD_ISSET(j, &temp_set))
            
            }// end for (j=0;...)
            
            // Detect if ALL streams have finished
            if(count == 0)
            {
                test->state = TEST_END;
                
              read = test->reporter_callback(test);
                puts(read);  
                
                // FREE ALL STREAMS
                n = test->streams;        
                while(n)
                {
                    close(n->socket);                        
                    iperf_free_stream(test, n);
                    n= n->next;
                }
                printf(" TEST ENDED\n");                
            }            
            
        }// end else (result>0)        
               
    }// end while    
    
}

void iperf_run_client(struct iperf_test *test)
{
    int i,result;
    struct iperf_stream *sp, *np;
    struct timer *timer, *stats_interval, *reporter_interval;    
    char *buf;
    int64_t delayus, adjustus, dtargus; 
    struct timeval tv;
    int ret=0;
    tv.tv_sec = 15;            // timeout interval in seconds
    tv.tv_usec = 0;
    
    buf = (char *) malloc(test->default_settings->blksize);
            
    if (test->protocol == Pudp)
    {
        dtargus = (int64_t)(test->default_settings->blksize) * SEC_TO_US * 8;
        dtargus /= test->default_settings->rate;
        
        assert(dtargus != 0);
        
        delayus = dtargus;
        adjustus = 0;
        printf("%lld adj %lld delay\n", adjustus, delayus);
        
        sp = test->streams;
        for(i=0; i< test->num_streams; i++)
        {
            sp->data = new_timer(0, dtargus);
            sp= sp->next;
        }
        
    }    
    
    timer = new_timer(test->duration, 0);
    
    if(test->stats_interval != 0)
        stats_interval = new_timer(test->stats_interval, 0);
    
    if(test->reporter_interval != 0)
        reporter_interval = new_timer(test->reporter_interval, 0);
        
    // send data till the timer expires
    while(!timer->expired(timer))
    {
        ret = select(test->max_fd+1, NULL, &test->write_set, NULL, &tv);
        if(ret<0)
            continue;
        
        sp= test->streams;    
        
        for(i=0;i<test->num_streams;i++)
        {
            if(FD_ISSET(sp->socket, &test->write_set))
            {  
               result = sp->snd(sp);             
                
                if(sp->next==NULL)
                    break;
                sp=sp->next;
                
            }// FD_ISSET
        }// for
        
        if((test->stats_interval!= 0) && stats_interval->expired(stats_interval))
        {    
            test->stats_callback(test);            
            stats_interval = new_timer(test->stats_interval,0);
        }
        
        if((test->reporter_interval!= 0) && reporter_interval->expired(reporter_interval))
        {    
            test->reporter_callback(test);            
            reporter_interval = new_timer(test->reporter_interval,0);
        }
        
    }// while outer timer
    
    
    test->state = TEST_END;
    test->reporter_callback(test);
    
    // Send the EOF - 0 buffer packets
    sp = test->streams;
    np = sp;    
    do {
        sp = np;
        send(sp->socket, buf, 0, 0);
        np = sp->next;    
        close(sp->socket);
        iperf_free_stream(test, sp);
                
    } while (np);
                
}    

int iperf_run(struct iperf_test *test)
{
    test->state = TEST_RUNNING;
    
    if(test->role == 's')
    {
        iperf_run_server(test);
        return 0;
    }
                
            
    else if ( test->role == 'c')
    {
        iperf_run_client(test);
        return 0;
    }
        
    return -1;
}

int
main(int argc, char **argv)
{
    char ch;
    struct iperf_test *test;
            
    test= iperf_new_test();        
    iperf_defaults(test);
        
    while( (ch = getopt_long(argc, argv, "c:p:st:uP:b:l:w:i:mNM:f:", longopts, NULL)) != -1 )
        switch (ch) {
            case 'c':
                test->role = 'c';    
                test->server_hostname = (char *) malloc(strlen(optarg));
                strncpy(test->server_hostname, optarg, strlen(optarg));
                break;                
            case 'p':
                test->server_port = atoi(optarg);                
                break;
            case 's':
                test->role = 's';                
                break;
            case 't':
                test->duration = atoi(optarg);
                break;
            case 'u':
                test->protocol = Pudp;
                test->default_settings->blksize = DEFAULT_UDP_BLKSIZE;
                break;
            case 'P':
                test->num_streams = atoi(optarg);
                break;
            case 'b':
                test->default_settings->rate = atoi(optarg);
                break;
            case 'l':
                test->default_settings->blksize = atol(optarg);
                break;
            case 'w':
                test->default_settings->socket_bufsize = atoi(optarg);
            case 'i':
                test->stats_interval = atoi(optarg);
                test->reporter_interval = atoi(optarg);
                break;
            case 'm':
                test->mPrintMSS = 1;
                break;
            case 'N':
                test->mNodelay = 1;
                break;
            case 'M':
                test->default_settings->MSS = atoi(optarg);
            case 'f':
                test->mFormat = *optarg;
                break;      
        }
    
    printf("ROLE = %s\n", (test->role == 's') ? "Server" : "Client");
    
    test->stats_callback = iperf_stats_callback;    
    test->reporter_callback = iperf_reporter_callback;

    switch(test->protocol)
    {
        case Ptcp:
            test->accept = iperf_tcp_accept;
            test->new_stream = iperf_new_tcp_stream;

            break;
        case Pudp:
            test->accept = iperf_udp_accept;
            test->new_stream = iperf_new_udp_stream;
            break;
    }
        
    iperf_init_test(test);
    
    iperf_run(test);
    
    return 0;
}
