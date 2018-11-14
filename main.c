// UDP packet udprepeater utility
//
// As destination address can be used NS records type SRV. If type SRV record is not found then an type A record is searched.
// Can be used as load balancer for one instance, because UDP packets sending to all found servers (no roundrobin).
//
// Usage udpudprepeater [path/to/config/file.conf]
// if parameter [path/to/config/file.conf] exist then try this as path to config file,
// elese fild fonfig file in folder /etc like /etc/udprepeater.conf
//
// Maintainer Dmitry Teikovtsev <teikovtsev.dmitry@pdffiller.team>

#include <sys/types.h>
#include <netinet/in.h>
#include <arpa/nameser.h>
#include <arpa/inet.h>
#include <resolv.h>
#include <netdb.h>
#include <string.h>
#include <stdlib.h>
#include <time.h>

#include "lib/kunf.h"

#define MAXINTLENGTH     ( 10 )
#define MAXNSSRVRECORDS  ( 5 )
#define MAXNSARECORDS    ( 5 )
#define MAXSERVERS       ( MAXNSSRVRECORDS * MAXNSARECORDS )
#define MAXPACKETLENGTH  ( 65535 )

typedef struct __res_state *res_state;
typedef struct srv_server {
    int ttl;
    int prio;
    int weight;
    int port;
    int count;
    char srvname[MAXDNAME];
    char srvip[MAXNSARECORDS][MAXDNAME];
} srv;

typedef struct my_config {
    int debug;
    int bindport;
    char bindaddr[MAXDNAME];
    int serviceport;
    char serviceaddr[MAXDNAME];
    int maxttl;
    int srv_count;
    struct srv_server srvs[MAXNSSRVRECORDS];
} my_cfg;

typedef struct my_servers {
    int count;
    int ttl;
    struct sockaddr_in servaddr[MAXSERVERS];
} my_srvr;

// function parse variables from file
//
// if argv[1] exist then try this as path to config file,
// elese fild fonfig file in folder /etc like /etc/argv[0].conf
//
// parameters:
//      int argc - count cli parameters
//      char **argv - body cli parameters
//      struct my_config *config - structure contains parsed config
int get_config (int argc, char **argv, struct my_config *config ){

    KSTATE *k;
    char *str;
    char procname[PATH_MAX];
    char confname[PATH_MAX];
    char intvalue[MAXINTLENGTH];
    int i, j;

    if (argc > 1) {
        memset( &confname, 0, PATH_MAX);
        snprintf( confname, PATH_MAX, "%s" ,argv[1]);
    } else {
        bzero(procname, PATH_MAX); j=0;
        for( i = 0; i < strlen(argv[0]); i++ ){
            if(argv[0][i]==0x2F){
                bzero(procname, PATH_MAX);
                j=0;
            }else{
                procname[j] = argv[0][i];
                j++;
            }
        }
        snprintf( confname, PATH_MAX, "/etc/%s.conf", procname );
    }
    fprintf(stdout, "used config file %s\n", confname);

    k = kunfig_open(confname, KUNFIG_OPEN_STANDARD);
    if(!k){
        fprintf(stderr, "Can't open config file %s\n", confname);
        return (1);
    }

    config->debug = 0;
    str = kunfig_findvalue(k, 1, "debug");
    if (str){
        memset(&intvalue, 0, MAXINTLENGTH);
        snprintf(intvalue, MAXINTLENGTH, "%s", str);
        config->debug = atoi(intvalue);
    }

    str = kunfig_findvalue(k, 1, "bindport");
    if (!str){
        fprintf(stderr, "Can't find param bindport in %s\n", confname);
        return (1);
    }
    memset(&intvalue, 0, MAXINTLENGTH);
    snprintf(intvalue, MAXINTLENGTH, "%s", str);
    config->bindport = atoi(intvalue);
    if( config->debug > 0 ) fprintf(stdout, "found param bindport = %d\n", config->bindport);

    str = kunfig_findvalue(k, 1, "bindaddr");
    if (!str){
        fprintf(stderr, "Can't find param bindaddr in %s\n", confname);
        return (1);
    }
    memset(config->bindaddr, 0, MAXDNAME);
    snprintf(config->bindaddr, MAXDNAME, "%s", str);
    if( config->debug > 0 ) fprintf(stdout, "found param bindaddr = %s\n", config->bindaddr);

    str = kunfig_findvalue(k, 1, "serviceport");
    if (!str){
        fprintf(stderr, "Can't find param serviceport in %s\n", confname);
        return (1);
    }
    memset(&intvalue, 0, MAXINTLENGTH);
    snprintf(intvalue, MAXINTLENGTH, "%s", str);
    config->serviceport = atoi(intvalue);
    if( config->debug > 0) fprintf(stdout, "found param serviceport = %d\n", config->serviceport);

    str = kunfig_findvalue(k, 1, "serviceaddr");
    if (!str){
        fprintf(stderr, "Can't find param serviceaddr in %s\n", confname);
        return (1);
    }
    memset(config->serviceaddr, 0, MAXDNAME);
    snprintf(config->serviceaddr, MAXDNAME, "%s", str);
    if( config->debug > 0) fprintf(stdout, "found param serviceaddr = %s\n", config->serviceaddr);

    str = kunfig_findvalue(k, 1, "maxttl");
    if (!str){
        fprintf(stderr, "Can't find param maxttl in %s\n", confname);
        return (1);
    }
    memset(&intvalue, 0, MAXINTLENGTH);
    snprintf(intvalue, MAXINTLENGTH, "%s", str);
    config->maxttl = atoi(intvalue);
    if( config->debug > 0 ) fprintf(stdout, "found param maxttl = %d\n", config->maxttl);

    return (0);
}

// function resolved SRV records
//
// after call this function need call resolv_srv_a for receive actual servers IP address
//
// parameters:
//      struct my_config *config - configuration strusture from which values are taken and save result to
int resolv_srv(struct my_config *config) {
    ns_msg handle;
    ns_rr rr;
    int ns_index, len;
    char dispbuf[MAXDNAME];
    size_t rdlen;
    const unsigned char *rdata;
    struct __res_state statp;
    unsigned char resbuf[NS_MAXMSG];

    // Initialize resolver
    memset(&statp, 0, sizeof(statp));
    if (res_ninit(&statp) < 0) {
        fprintf(stderr, "Can't initialize statp for SRV records.\n");
        return (1);
    }
    if( config->debug > 1 ) fprintf(stdout, "initialize statp for SRV records completly\n");

    // Turning on DEBUG mode for res_nsearch
    if( config->debug > 1 ) statp.options |= RES_DEBUG;

    // Search for SRV type records
    len = res_nsearch(&statp, config->serviceaddr, C_IN, T_SRV, resbuf, NS_MAXMSG);
    if (len < 0) {
        fprintf(stderr, "Error occured during search SRV records.\n");
        return (1);
    }
    if (len > NS_MAXMSG) {
        fprintf(stderr, "The buffer for SRV records is too small.\n");
        return (1);
    }
    if( config->debug > 1) fprintf(stdout, "res_nsearch for SRV records completly\n");

    // Initialize parser
    if (ns_initparse(resbuf, len, &handle) < 0) {
        fprintf(stderr, "ns_initparse for SRV records failed.\n");
        return (1);
    }
    if( config->debug > 1) fprintf(stdout, "ns_initparse for SRV records completly\n");

    // Get records count
    len = ns_msg_count(handle, ns_s_an);
    if (len < 0) {
        fprintf(stderr, "ns_msg_count no found records.\n");
        return (1);
    }
    if( config->debug > 1) fprintf(stdout, "ns_msg_count for SRV records = %d\n", len);

    // Parse SRV records
    config->srv_count = 0;
    for (ns_index = 0; config->srv_count < MAXNSSRVRECORDS && ns_index < len; ns_index++) {

        // Get next record
        if (ns_parserr(&handle, ns_s_an, ns_index, &rr)) {
            fprintf(stderr, "ns_parserr failed.\n");
            continue;
        }
        if( config->debug > 1) fprintf(stdout, "ns_parserr for %d SRV records completly\n", config->srv_count+1);

        // Save record in buffer
        ns_sprintrr (&handle, &rr, NULL, NULL, dispbuf, MAXDNAME);
        if( config->debug > 1) fprintf(stdout, "record in buffer for %d SRV records completly\n", config->srv_count+1);

        // if record class=inet and record type=SRV parse buffer to result structure
        if (ns_rr_class(rr) == ns_c_in && ns_rr_type(rr) == ns_t_srv) {
            rdata = ns_rr_rdata(rr);
            config->srvs[config->srv_count].ttl    = ns_rr_ttl(rr);
            config->srvs[config->srv_count].prio   = ns_get16(rdata); rdata += 2; rdlen -=2;
            config->srvs[config->srv_count].weight = ns_get16(rdata); rdata += 2; rdlen -=2;
            config->srvs[config->srv_count].port   = ns_get16(rdata); rdata += 2; rdlen -=2;
            dn_expand(ns_msg_base(handle), ns_msg_end(handle), rdata, config->srvs[config->srv_count].srvname, MAXDNAME);
            if( config->debug > 1) fprintf(stdout, "save data for %d SRV records completly\n", config->srv_count+1);

            // switch to next server
            config->srv_count++;
        }

    }

    return (0);
}

// function resolved A records
//
// parameters:
//      struct my_config *config - configuration strusture
//      struct srv_server *server - configuration strusture contains servers
//              parameters from which values are taken and save result to
int resolv_srv_a(struct my_config *config, struct srv_server *server) {
    ns_msg handle;
    ns_rr rr;
    int len;
    char dispbuf[MAXDNAME];
    struct __res_state statp;
    unsigned char resbuf[NS_MAXMSG];

    // Initialize resolver
    memset(&statp, 0, sizeof(statp));
    if (res_ninit(&statp) < 0) {
        fprintf(stderr, "Can't initialize statp for A records %s.\n", server->srvname);
        return (1);
    }
    if( config->debug > 1) fprintf(stdout, "initialize statp for A records completly\n");

    // Search for A type records
    len = res_nsearch(&statp, server->srvname, C_IN, T_A, resbuf, NS_MAXMSG);
    if (len < 0) {
        fprintf(stderr, "Error occured during search for A records %s.\n", server->srvname);
        return (1);
    }
    if (len > NS_MAXMSG) {
        fprintf(stderr, "The buffer is too small for A records %s.\n", server->srvname);
        return (1);
    }
    if( config->debug > 1) fprintf(stdout, "res_nsearch for A records completly\n");

    // Initialize parser
    if (ns_initparse(resbuf, len, &handle) < 0) {
        fprintf(stderr, "ns_initparse for A records %s failed.\n", server->srvname);
        return (1);
    }
    if( config->debug > 1) fprintf(stdout, "ns_initparse for A records completly\n");

    // Get records count
    len = ns_msg_count(handle, ns_s_an);
    if (len < 0) {
        fprintf(stderr, "ns_msg_count no found A records for %s.\n", server->srvname);
        return (1);
    }
    if( config->debug > 1) fprintf(stdout, "found %d A records\n", len);

    if (len > MAXNSARECORDS){
        len = MAXNSARECORDS;
        fprintf(stderr, "The count buffers is too small for A records %s, truncated.\n", server->srvname);
    }

    // Parse A records
    for (server->count = 0; server->count < len; server->count++) {

        // Get next record
        if (ns_parserr(&handle, ns_s_an, server->count, &rr)) {
            fprintf(stderr, "ns_parserr for A records failed.\n");
            continue;
        }
        if( config->debug > 1) fprintf(stdout, "ns_parserr for %d A records completly\n", server->count+1);

        // Save record in buffer
        ns_sprintrr (&handle, &rr, NULL, NULL, dispbuf, MAXDNAME);
        if( config->debug > 1) fprintf(stdout, "ns_sprintrr for %d A records completly\n", server->count+1);

        // if record class=inet and record type=A parse buffer to result structure
        if (ns_rr_class(rr) == ns_c_in && ns_rr_type(rr) == ns_t_a) {

            // get ip address
            (void)inet_ntop(AF_INET, ns_rr_rdata(rr), server->srvip[server->count], MAXDNAME);
            if( config->debug > 1) fprintf(stdout, "ip address for %d A records = %s\n", server->count+1, server->srvip[server->count]);

            // calculate minimal ttl
            if (server->ttl > ns_rr_ttl(rr)) server->ttl = ns_rr_ttl(rr);
            if( config->debug > 1) fprintf(stdout, "calculate ttl for %d A records = %d\n", server->count+1, server->ttl);
        }
    }

    return (0);
}

// function resolved SRV and A records
//
// function first try resolve type SRV record for config parameter 'serviceaddr' and
// next try resolve type A record for found type SRV records.
//
// if record type SRV not found then config parameter 'serviceaddr' try find only in type A records
//
// parameters:
//      struct my_config *config - configuration strusture from which values are taken
//      struct my_servers *servers - the resulting structure contains servers parameters
int get_servers(struct my_config *config, struct my_servers *servers) {

    memset (servers, 0, sizeof(struct my_servers));
    servers->ttl = config->maxttl;

    if ( resolv_srv(config) ){
        fprintf(stderr, "Can't resolvs %s type SRV try record type A \n", config->serviceaddr);
        config->srv_count = 1;
        memcpy(config->srvs[0].srvname, config->serviceaddr, MAXDNAME);
        config->srvs[0].port = config->serviceport;
        config->srvs[0].ttl = config->maxttl;
    }
    if( config->debug > 1) fprintf(stdout, "resolv_srv completly\n");

    for (int x=0; x<config->srv_count; x++){
        if (resolv_srv_a(config, &config->srvs[x])){
            fprintf(stderr, "Can't resolv %s type A\n", config->srvs[x].srvname);
        }
        for (int n=0; n < config->srvs[x].count && servers->count < MAXSERVERS; n++){

            if( config->debug > 0 ) fprintf(stdout,"%s -> %s, port=%d, ttl=%d\n",
                config->srvs[x].srvname,config->srvs[x].srvip[n], config->srvs[x].port, config->srvs[x].ttl);

            servers->servaddr[servers->count].sin_family = AF_INET;
            servers->servaddr[servers->count].sin_port = htons(config->srvs[x].port);
            servers->servaddr[servers->count].sin_addr.s_addr = inet_addr(config->srvs[x].srvip[n]);

            if(config->srvs[x].ttl != 0 && servers->ttl > config->srvs[x].ttl){
                servers->ttl = config->srvs[x].ttl;
            }

            servers->count++;
        }
    }
    if( config->debug > 1) fprintf(stdout, "resolv_srv_a completly\n");

    return (!servers->count);
}

int main(int argc, char **argv)
{
    struct my_config config;
    struct my_servers servers;
    struct sockaddr_in servaddr, cliaddr;
    int clisockfd, servsockfd, maxfd;
    int ret, receivelen, sendlen, srvnum;
    socklen_t len;
    const int optval = 1;
    fd_set readfds;
    struct timeval timeout;
    char buffer[MAXPACKETLENGTH];
    time_t current_time;

    memset(&config, 0, sizeof(struct my_config));
    if (get_config(argc, argv, &config)) {
        fprintf(stderr, "Can't get_config\n");
    }
    if( config.debug > 1) fprintf(stdout, "get_config completly\n");

    if (get_servers(&config, &servers)){
        fprintf(stderr, "Can't found any servers\n");
        return (1);
    }
    // update timer for next resolv
    current_time = time(NULL);
    if( config.debug > 1) fprintf(stdout, "get_servers completly\n");

    // Creating socket file descriptor for incoming connections
    if ( (servsockfd = socket(AF_INET, SOCK_DGRAM, 0)) < 0 ) {
        fprintf(stderr, "Can't create server socket\n");
        return (1);
    }
    maxfd = servsockfd;
    if( config.debug > 1) fprintf(stdout, "create server socket completly\n");

    if (setsockopt(servsockfd, SOL_SOCKET, SO_REUSEADDR, &optval, sizeof(optval)) < 0){
        fprintf(stderr, "Can't setsockopt SO_REUSEADDR\n");
        return (1);
    }
    if( config.debug > 1) fprintf(stdout, "setsockopt completly\n");

    // Filling my_server information
    memset(&servaddr, 0, sizeof(servaddr));
    servaddr.sin_family = AF_INET;
    servaddr.sin_addr.s_addr = inet_addr(config.bindaddr);
    servaddr.sin_port = htons(config.bindport);

    // Bind the socket with the server address
    if ( bind(servsockfd, (const struct sockaddr *)&servaddr, sizeof(servaddr)) < 0 ){
        fprintf(stderr, "Can't bind to socket\n");
        return (1);
    }
    if( config.debug > 1) fprintf(stdout, "bind to socket completly\n");

    // Creating socket file descriptor for outgoing connections
    if ( (clisockfd = socket(AF_INET, SOCK_DGRAM, 0)) < 0 ) {
        fprintf(stderr, "Can't create client socket\n");
        return (1);
    }
    if (maxfd < clisockfd) maxfd = clisockfd;
    maxfd++;
    if( config.debug > 1) fprintf(stdout, "create client socket completly\n");

    while (1){

        timeout.tv_sec  = servers.ttl;
        timeout.tv_usec = 10;

        FD_ZERO(&readfds);
        FD_SET(servsockfd, &readfds);

        ret = select(maxfd, &readfds, NULL, NULL, &timeout);
        if( ret < 0 ){
            fprintf(stderr, "Select thrown an exception\n");
            return (1);

        } else if ( ret == 0 ) {

            // time out, retry resolv servers addr
            if (get_servers(&config, &servers)){
                fprintf(stderr, "Can't found any servers\n");
                return (1);
            }
            // update timer for next resolv
            current_time = time(NULL);
            if( config.debug > 1) fprintf(stdout, "get_servers completly\n");

        } else if(FD_ISSET(servsockfd, &readfds)){

            // persist data in receive buffer
            memset(&cliaddr, 0, sizeof(cliaddr));
            memset( buffer, 0, MAXPACKETLENGTH);

            receivelen = recvfrom(servsockfd, (char *)buffer, MAXPACKETLENGTH, MSG_WAITALL,
                ( struct sockaddr *) &cliaddr, &len);

            if ( config.debug > 0) fprintf(stdout, "recv: %d %s\n", receivelen, buffer);

            // no rounribin send buffer to all servers
            for (srvnum = 0; srvnum < servers.count; srvnum++){
                sendlen = sendto(clisockfd, (const char *)buffer, receivelen, MSG_CONFIRM,
                    (const struct sockaddr *) &servers.servaddr[srvnum], sizeof(servers.servaddr[srvnum]));
                if ( sendlen == receivelen ) {
                    if ( config.debug > 0) fprintf(stdout, "sendto: %d to %d server\n", sendlen, srvnum+1);
                } else {
                    if ( config.debug > 0) fprintf(stdout, "error sendto: %d to %d server\n", sendlen, srvnum+1);
                }
            }

            // if long time no resolv dns update servers addr
            if ( current_time + servers.ttl < time(NULL)) {
                if (get_servers(&config, &servers)){
                    fprintf(stderr, "Can't found any servers\n");
                    return (1);
                }
                // update timer for next resolv
                current_time = time(NULL);
                if( config.debug > 1) fprintf(stdout, "get_servers completly\n");
            }

        } else {
            fprintf(stderr, "Select unknown exception try next\n");
        }
    }
}
