/*
 * Copyright (c) 2009-2011, The Regents of the University of California,
 * through Lawrence Berkeley National Laboratory (subject to receipt of any
 * required approvals from the U.S. Dept. of Energy).  All rights reserved.
 *
 * This code is distributed under a BSD style license, see the LICENSE file
 * for complete information.
 */

#ifndef        __IPERF_API_H
#define        __IPERF_API_H

#include <setjmp.h>

struct iperf_test;
struct iperf_stream_result;
struct iperf_interval_results;
struct iperf_stream;

/* default settings */
#define Ptcp SOCK_STREAM
#define Pudp SOCK_DGRAM
#define DEFAULT_UDP_BLKSIZE 1450 /* 1 packet per ethernet frame, IPV6 too */
#define DEFAULT_TCP_BLKSIZE (128 * 1024)  /* default read/write block size */

/* states */
#define TEST_START 1
#define TEST_RUNNING 2
#define RESULT_REQUEST 3
#define TEST_END 4
#define STREAM_BEGIN 5
#define STREAM_RUNNING 6
#define STREAM_END 7
#define ALL_STREAMS_END 8
#define PARAM_EXCHANGE 9
#define CREATE_STREAMS 10
#define SERVER_TERMINATE 11
#define CLIENT_TERMINATE 12
#define EXCHANGE_RESULTS 13
#define DISPLAY_RESULTS 14
#define IPERF_START 15
#define IPERF_DONE 16
#define ACCESS_DENIED (-1)
#define SERVER_ERROR (-2)

/* Getter routines for some fields inside iperf_test. */
int	iperf_get_test_duration( struct iperf_test* ipt );
char	iperf_get_test_role( struct iperf_test* ipt );
int	iperf_get_test_blksize( struct iperf_test* ipt );
uint64_t iperf_get_test_rate( struct iperf_test* ipt );
int	iperf_get_test_socket_bufsize( struct iperf_test* ipt );
double	iperf_get_test_reporter_interval( struct iperf_test* ipt );
double	iperf_get_test_stats_interval( struct iperf_test* ipt );
int	iperf_get_test_num_streams( struct iperf_test* ipt );
int	iperf_get_test_server_port( struct iperf_test* ipt );
char*	iperf_get_test_server_hostname( struct iperf_test* ipt );
int	iperf_get_test_protocol_id( struct iperf_test* ipt );

/* Setter routines for some fields inside iperf_test. */
void	iperf_set_test_duration( struct iperf_test* ipt, int duration );
void	iperf_set_test_reporter_interval( struct iperf_test* ipt, double reporter_interval );
void	iperf_set_test_stats_interval( struct iperf_test* ipt, double stats_interval );
void	iperf_set_test_state( struct iperf_test* ipt, char state );
void	iperf_set_test_blksize( struct iperf_test* ipt, int blksize );
void	iperf_set_test_rate( struct iperf_test* ipt, uint64_t rate );
void	iperf_set_test_server_port( struct iperf_test* ipt, int server_port );
void	iperf_set_test_socket_bufsize( struct iperf_test* ipt, int socket_bufsize );
void	iperf_set_test_num_streams( struct iperf_test* ipt, int num_streams );
void	iperf_set_test_role( struct iperf_test* ipt, char role );
void	iperf_set_test_server_hostname( struct iperf_test* ipt, char* server_hostname );

/**
 * exchange_parameters - handles the param_Exchange part for client
 *
 */
int      iperf_exchange_parameters(struct iperf_test * test);

/**
 * add_to_interval_list -- adds new interval to the interval_list
 *
 */
void      add_to_interval_list(struct iperf_stream_result * rp, struct iperf_interval_results *temp);

/**
 * connect_msg -- displays connection message
 * denoting senfer/receiver details
 *
 */
void      connect_msg(struct iperf_stream * sp);

/**
 * iperf_stats_callback -- handles the statistic gathering
 *
 */
void     iperf_stats_callback(struct iperf_test * test);


/**
 * iperf_reporter_callback -- handles the report printing
 *
 */
void     iperf_reporter_callback(struct iperf_test * test);


/**
 * iperf_new_test -- return a new iperf_test with default values
 *
 * returns NULL on failure
 *
 */
struct iperf_test *iperf_new_test();

int      iperf_defaults(struct iperf_test * testp);


/**
 * iperf_free_test -- free resources used by test, calls iperf_free_stream to
 * free streams
 *
 */
void      iperf_free_test(struct iperf_test * testp);


/**
 * iperf_new_stream -- return a net iperf_stream with default values
 *
 * returns NULL on failure
 *
 */
struct iperf_stream *iperf_new_stream(struct iperf_test *, int);

/**
 * iperf_add_stream -- add a stream to a test
 *
 */
void      iperf_add_stream(struct iperf_test * test, struct iperf_stream * stream);

/**
 * iperf_init_stream -- init resources associated with test
 *
 */
int       iperf_init_stream(struct iperf_stream *, struct iperf_test *);

/**
 * iperf_free_stream -- free resources associated with test
 *
 */
void      iperf_free_stream(struct iperf_stream * sp);

int has_tcpinfo(void);
int has_tcpinfo_retransmits(void);
void save_tcpinfo(struct iperf_stream *sp, struct iperf_interval_results *irp);
long get_tcpinfo_total_retransmits(struct iperf_interval_results *irp);
void print_tcpinfo(struct iperf_test *test);
void build_tcpinfo_message(struct iperf_interval_results *r, char *message);

void print_interval_results(struct iperf_test * test, struct iperf_stream *sp);
int iperf_send(struct iperf_test *);
int iperf_recv(struct iperf_test *);
void sig_handler(int);
void usage();
void usage_long();
void warning(char *);
int all_data_sent(struct iperf_test *);
int iperf_exchange_results(struct iperf_test *);
int iperf_init_test(struct iperf_test *);
int iperf_parse_arguments(struct iperf_test *, int, char **);
void iperf_reset_test(struct iperf_test *);

struct protocol *get_protocol(struct iperf_test *, int);
int set_protocol(struct iperf_test *, int);

void iperf_on_new_stream(struct iperf_stream *);
void iperf_on_test_start(struct iperf_test *);
void iperf_on_connect(struct iperf_test *);
void iperf_on_test_finish(struct iperf_test *);

extern jmp_buf env;

/* Client routines. */
int iperf_run_client(struct iperf_test *);
int iperf_connect(struct iperf_test *);
int iperf_create_streams(struct iperf_test *);
int iperf_handle_message_client(struct iperf_test *);
int iperf_client_end(struct iperf_test *);

/* Server routines. */
int iperf_run_server(struct iperf_test *);
int iperf_server_listen(struct iperf_test *);
int iperf_accept(struct iperf_test *);
int iperf_handle_message_server(struct iperf_test *);
void iperf_test_reset(struct iperf_test *);

/* Error routines. */
void iperf_error(char *);
char *iperf_strerror(int);
extern int i_errno;
enum {
    IENONE = 0,             // No error
    /* Parameter errors */
    IESERVCLIENT = 1,       // Iperf cannot be both server and client
    IENOROLE = 2,           // Iperf must either be a client (-c) or server (-s)
    IECLIENTONLY = 3,       // This option is client only
    IEDURATION = 4,         // test duration too long. Maximum value = %dMAX_TIME
    IENUMSTREAMS = 5,       // Number of parallel streams too large. Maximum value = %dMAX_STREAMS
    IEBLOCKSIZE = 6,        // Block size too large. Maximum value = %dMAX_BLOCKSIZE
    IEBUFSIZE = 7,          // Socket buffer size too large. Maximum value = %dMAX_TCP_BUFFER
    IEINTERVAL = 8,         // Report interval too large. Maxumum value = %dMAX_INTERVAL
    IEMSS = 9,              // MSS too large. Maximum value = %dMAX_MSS
    /* Test errors */
    IENEWTEST = 10,         // Unable to create a new test (check perror)
    IEINITTEST = 11,        // Test initialization failed (check perror)
    IELISTEN = 12,          // Unable to listen for connections (check perror)
    IECONNECT = 13,         // Unable to connect to server (check herror/perror) [from netdial]
    IEACCEPT = 14,          // Unable to accept connection from client (check herror/perror)
    IESENDCOOKIE = 15,      // Unable to send cookie to server (check perror)
    IERECVCOOKIE = 16,      // Unable to receive cookie from client (check perror)
    IECTRLWRITE = 17,       // Unable to write to the control socket (check perror)
    IECTRLREAD = 18,        // Unable to read from the control socket (check perror)
    IECTRLCLOSE = 19,       // Control socket has closed unexpectedly
    IEMESSAGE = 20,         // Received an unknown message
    IESENDMESSAGE = 21,     // Unable to send control message to client/server (check perror)
    IERECVMESSAGE = 22,     // Unable to receive control message from client/server (check perror)
    IESENDPARAMS = 23,      // Unable to send parameters to server (check perror)
    IERECVPARAMS = 24,      // Unable to receive parameters from client (check perror)
    IEPACKAGERESULTS = 25,  // Unable to package results (check perror)
    IESENDRESULTS = 26,     // Unable to send results to client/server (check perror)
    IERECVRESULTS = 27,     // Unable to receive results from client/server (check perror)
    IESELECT = 28,          // Select failed (check perror)
    IECLIENTTERM = 29,      // The client has terminated
    IESERVERTERM = 30,      // The server has terminated
    IEACCESSDENIED = 31,    // The server is busy running a test. Try again later.
    IESETNODELAY = 32,      // Unable to set TCP NODELAY (check perror)
    IESETMSS = 33,          // Unable to set TCP MSS (check perror)
    IESETBUF = 34,          // Unable to set socket buffer size (check perror)
    IESETTOS = 35,          // Unable to set IP TOS (check perror)
    IESETCOS = 36,          // Unable to set IPv6 traffic class (check perror)
    IEREUSEADDR = 37,       // Unable to set reuse address on socket (check perror)
    IENONBLOCKING = 38,     // Unable to set socket to non-blocking (check perror)
    IESETWINDOWSIZE = 39,   // Unable to set socket window size (check perror)
    IEPROTOCOL = 40,        // Protocol does not exist
    /* Stream errors */
    IECREATESTREAM = 41,    // Unable to create a new stream (check herror/perror)
    IEINITSTREAM = 42,      // Unable to initialize stream (check herror/perror)
    IESTREAMLISTEN = 43,    // Unable to start stream listener (check perror) 
    IESTREAMCONNECT = 44,   // Unable to connect stream (check herror/perror)
    IESTREAMACCEPT = 45,    // Unable to accepte stream connection (check perror)
    IESTREAMWRITE = 46,     // Unable to write to stream socket (check perror)
    IESTREAMREAD = 47,      // Unable to read from stream (check perror)
    IESTREAMCLOSE = 48,     // Stream has closed unexpectedly
    IESTREAMID = 49,        // Stream has invalid ID
    /* Timer errors */
    IENEWTIMER = 50,        // Unable to create new timer (check perror)
    IEUPDATETIMER = 51,     // Unable to update timer (check perror)
};

#endif /* !__IPERF_API_H */
