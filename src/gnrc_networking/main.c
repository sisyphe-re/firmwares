/*
 * Copyright (C) 2015 Freie Universität Berlin
 *
 * This file is subject to the terms and conditions of the GNU Lesser
 * General Public License v2.1. See the file LICENSE in the top level
 * directory for more details.
 */

/**
 * @ingroup     examples
 * @{
 *
 * @file
 * @brief       Example application for demonstrating the RIOT network stack
 *
 * @author      Hauke Petersen <hauke.petersen@fu-berlin.de>
 *
 * @}
 */

#include <stdio.h>
#include <stddef.h>
#include <stdint.h>
#include <string.h>

#include "shell.h"
#include "msg.h"

/* Sensors */
#include "saul.h"
#include "saul_reg.h"
#include "fmt.h"

/* Inverse Transform Sampling */
#include "math.h"
#include "random.h"

/* Networking */
#include "net/gnrc.h"
#include "net/gnrc/ipv6.h"
#include "net/gnrc/netif.h"
#include "net/gnrc/netif/hdr.h"
#include "net/gnrc/pktdump.h"
#include "net/gnrc/udp.h"
#include "net/ipv6/addr.h"
#include "net/netif.h"
#include "net/gnrc/netif.h"
#include "net/netstats.h"
#include "net/netstats/neighbor.h"
#include "net/gnrc/rpl.h"
#include "net/gnrc/rpl/structs.h"
#include "net/gnrc/rpl/dodag.h"

/* Threading */
#include "thread.h"
#include "xtimer.h"

/* Readline */
#define ETX '\x03'  /** ASCII "End-of-Text", or Ctrl-C */
#define EOT '\x04'  /** ASCII "End-of-Transmission", or Ctrl-D */
#define BS  '\x08'  /** ASCII "Backspace" */
#define DEL '\x7f'  /** ASCII "Delete" */

#define min(a,b) (a<=b?a:b)

static int readline(char *buf, size_t size)
{
    int curr_pos = 0;
    bool length_exceeded = false;

    assert((size_t) size > 0);

    while (1) {
        assert((size_t) curr_pos < size);

        int c = getchar();

        switch (c) {

            case EOT:
                /* Ctrl-D terminates the current shell instance. */
                /* fall-thru */
            case EOF:
                return EOF;

            case ETX:
                /* Ctrl-C cancels the current line. */
                curr_pos = 0;
                length_exceeded = false;
                /* fall-thru */
            case '\r':
                /* fall-thru */
            case '\n':
                buf[curr_pos] = '\0';
                return (length_exceeded) ? -ENOBUFS : curr_pos;

            /* check for backspace: */
            case BS:    /* 0x08 (BS) for most terminals */
                /* fall-thru */
            case DEL:   /* 0x7f (DEL) when using QEMU */
                if (curr_pos > 0) {
                    curr_pos--;
                    if ((size_t) curr_pos < size) {
                        buf[curr_pos] = '\0';
                        length_exceeded = false;
                    }
                }
                break;

            default:
                /* Always consume characters, but do not not always store them */
                if ((size_t) curr_pos < size - 1) {
                    buf[curr_pos++] = c;
                }
                else {
                    length_exceeded = true;
                }
                break;
        }
    }
}

/* Thread printing network stats */
char stats_thread_stack[THREAD_STACKSIZE_DEFAULT];

/* Exponential sensor thread */
char exponential_sensors_thread_stack[THREAD_STACKSIZE_DEFAULT];

/* Periodic sensor thread */
char periodic_sensors_thread_stack[THREAD_STACKSIZE_DEFAULT];

/* Address and port of the destination server */
#define IPV6_ADDR_MAXLEN 45
char server_address[IPV6_ADDR_MAXLEN+1]  = "";
char server_port[5+1] = "1337";

/* Type of generation:
      - EXPONENTIAL
      - HYBRID
      - PERIODIC
*/
#define GENERATION_TYPE_MAXLEN 11
char generation_type[GENERATION_TYPE_MAXLEN+1] = "EXPONENTIAL";

/* Parameter for the data generation */
#define EXP_PARAMETER_MAXLEN 10
char exp_parameter_str[EXP_PARAMETER_MAXLEN+1] = "";
float exp_parameter = 0.25;

#define PERIOD_PARAMETER_MAXLEN 10
char period_parameter_str[PERIOD_PARAMETER_MAXLEN+1] = "";
float period_parameter = 1.0;

/* Payload size */
#define PACKET_SIZE_MAXLEN 10
char packet_size_str[PACKET_SIZE_MAXLEN+1] = "";
int packet_size = 0;

void btox(char *xp, const uint8_t *bb, int n) 
{
    const char xx[]= "0123456789ABCDEF";
    while (--n >= 0) xp[n] = xx[(bb[n>>1] >> ((1 - (n&1)) << 2)) & 0xF];
}


static void send(char *addr_str, char *port_str, uint8_t *data)
{
    gnrc_netif_t *netif = NULL;
    char *iface;
    uint16_t port;
    ipv6_addr_t addr;

    iface = ipv6_addr_split_iface(addr_str);
    if ((!iface) && (gnrc_netif_numof() == 1))
    {
        netif = gnrc_netif_iter(NULL);
    }
    else if (iface)
    {
        netif = gnrc_netif_get_by_pid(atoi(iface));
    }

    /* parse destination address */
    if (ipv6_addr_from_str(&addr, addr_str) == NULL)
    {
        printf("error,unable to parse destination address %s\n", addr_str);
        return;
    }
    /* parse port */
    port = atoi(port_str);
    if (port == 0)
    {
        printf("error,unable to parse destination port %s\n", port_str);
        return;
    }

    gnrc_pktsnip_t *payload, *udp, *ip;
    unsigned payload_size;
    /* allocate payload */
    payload = gnrc_pktbuf_add(NULL, data, packet_size, GNRC_NETTYPE_UNDEF);
    if (payload == NULL)
    {
        puts("error,unable to copy data to packet buffer");
        return;
    }
    /* store size for output */
    payload_size = (unsigned)payload->size;
    /* allocate UDP header, set source port := destination port */
    udp = gnrc_udp_hdr_build(payload, port, port);
    if (udp == NULL)
    {
        puts("error,unable to allocate UDP header");
        gnrc_pktbuf_release(payload);
        return;
    }
    /* allocate IPv6 header */
    ip = gnrc_ipv6_hdr_build(udp, NULL, &addr);
    if (ip == NULL)
    {
        puts("error,unable to allocate IPv6 header");
        gnrc_pktbuf_release(udp);
        return;
    }
    /* add netif header, if interface was given */
    if (netif != NULL)
    {
        gnrc_pktsnip_t *netif_hdr = gnrc_netif_hdr_build(NULL, 0, NULL, 0);

        gnrc_netif_hdr_set_netif(netif_hdr->data, netif);
        ip = gnrc_pkt_prepend(ip, netif_hdr);
    }
    /* send packet */
    if (!gnrc_netapi_dispatch_send(GNRC_NETTYPE_UDP, GNRC_NETREG_DEMUX_CTX_ALL, ip))
    {
        puts("error,enable to locate UDP thread");
        gnrc_pktbuf_release(ip);
        return;
    }
    /* access to `payload` was implicitly given up with the send operation above
         * => use temporary variable for output */
    
    /* Limit the payload size  */
    int n = min(5,packet_size) << 1;
    char hexstr[n + 1];
    btox(hexstr, data, n);
    hexstr[n] = 0;
    printf("udp,%u,%s,%u,%s\n", payload_size, addr_str, port, hexstr);
}

static void read_sensor(void)
{
    uint8_t result[packet_size];
    random_bytes(result, packet_size);
    send(server_address, server_port, result);
}

static float exponential_distribution(void)
{
    return -(1 / exp_parameter) * log(random_real());
}

/* Reads the sensor in an infinite loop. */
static void *_run_exponential_sensor_loop(void *arg)
{
    (void)arg;
    while (1)
    {
        read_sensor();
        xtimer_usleep((exponential_distribution()) * US_PER_SEC);
    }
    return NULL;
}

static void *_run_periodic_sensor_loop(void *arg)
{
    (void)arg;
    while (1)
    {
        read_sensor();
        xtimer_usleep(period_parameter * US_PER_SEC);
    }
    return NULL;
}

static const char *_netstats_module_to_str(uint8_t module)
{
    switch (module)
    {
    case NETSTATS_LAYER2:
        return "Layer 2";
    case NETSTATS_IPV6:
        return "IPv6";
    case NETSTATS_ALL:
        return "all";
    default:
        return "Unknown";
    }
}

static int _netif_stats(netif_t *iface, unsigned module)
{
    netstats_t *stats;
    int res = netif_get_opt(iface, NETOPT_STATS, module, &stats, sizeof(&stats));

    if (res < 0)
    {
        printf("stats,0,-1,-1,-1,-1,-1,-1,-1,-1\n");
    }
    else
    {
        printf("stats,1,%s,%u,%u,%u,%u,%u,%u,%u\n",
               _netstats_module_to_str(module),
               (unsigned)stats->rx_count,
               (unsigned)stats->rx_bytes,
               (unsigned)(stats->tx_unicast_count + stats->tx_mcast_count),
               (unsigned)stats->tx_mcast_count,
               (unsigned)stats->tx_bytes,
               (unsigned)stats->tx_success,
               (unsigned)stats->tx_failed);
        res = 0;
    }
    return res;
}

int rpl_stats(void)
{
    printf("rpl_stats,DIO,packets,%10" PRIu32 ",%-10" PRIu32 ",%10" PRIu32 ",%-10" PRIu32 "\n",
           gnrc_rpl_netstats.dio_rx_ucast_count, gnrc_rpl_netstats.dio_tx_ucast_count,
           gnrc_rpl_netstats.dio_rx_mcast_count, gnrc_rpl_netstats.dio_tx_mcast_count);
    printf("rpl_stats,DIO,bytes,%10" PRIu32 ",%-10" PRIu32 ",%10" PRIu32 ",%-10" PRIu32 "\n",
           gnrc_rpl_netstats.dio_rx_ucast_bytes, gnrc_rpl_netstats.dio_tx_ucast_bytes,
           gnrc_rpl_netstats.dio_rx_mcast_bytes, gnrc_rpl_netstats.dio_tx_mcast_bytes);
    printf("rpl_stats,DIS,packets,%10" PRIu32 ",%-10" PRIu32 ",%10" PRIu32 ",%-10" PRIu32 "\n",
           gnrc_rpl_netstats.dis_rx_ucast_count, gnrc_rpl_netstats.dis_tx_ucast_count,
           gnrc_rpl_netstats.dis_rx_mcast_count, gnrc_rpl_netstats.dis_tx_mcast_count);
    printf("rpl_stats,DIS,bytes,%10" PRIu32 ",%-10" PRIu32 ",%10" PRIu32 ",%-10" PRIu32 "\n",
           gnrc_rpl_netstats.dis_rx_ucast_bytes, gnrc_rpl_netstats.dis_tx_ucast_bytes,
           gnrc_rpl_netstats.dis_rx_mcast_bytes, gnrc_rpl_netstats.dis_tx_mcast_bytes);
    printf("rpl_stats,DAO,packets,%10" PRIu32 ",%-10" PRIu32 ",%10" PRIu32 ",%-10" PRIu32 "\n",
           gnrc_rpl_netstats.dao_rx_ucast_count, gnrc_rpl_netstats.dao_tx_ucast_count,
           gnrc_rpl_netstats.dao_rx_mcast_count, gnrc_rpl_netstats.dao_tx_mcast_count);
    printf("rpl_stats,DAO,bytes,%10" PRIu32 ",%-10" PRIu32 ",%10" PRIu32 ",%-10" PRIu32 "\n",
           gnrc_rpl_netstats.dao_rx_ucast_bytes, gnrc_rpl_netstats.dao_tx_ucast_bytes,
           gnrc_rpl_netstats.dao_rx_mcast_bytes, gnrc_rpl_netstats.dao_tx_mcast_bytes);
    printf("rpl_stats,DAO-ACK,packets,%10" PRIu32 ",%-10" PRIu32 ",%10" PRIu32 ",%-10" PRIu32 "\n",
           gnrc_rpl_netstats.dao_ack_rx_ucast_count, gnrc_rpl_netstats.dao_ack_tx_ucast_count,
           gnrc_rpl_netstats.dao_ack_rx_mcast_count, gnrc_rpl_netstats.dao_ack_tx_mcast_count);
    printf("rpl_stats,DAO-ACK,bytes,%10" PRIu32 ",%-10" PRIu32 ",%10" PRIu32 ",%-10" PRIu32 "\n",
           gnrc_rpl_netstats.dao_ack_rx_ucast_bytes, gnrc_rpl_netstats.dao_ack_tx_ucast_bytes,
           gnrc_rpl_netstats.dao_ack_rx_mcast_bytes, gnrc_rpl_netstats.dao_ack_tx_mcast_bytes);
    return 0;
}

int rpl_dodag_show(void)
{
    for (uint8_t i = 0; i < GNRC_RPL_INSTANCES_NUMOF; ++i) {
        printf("rpl_status,instance,%d,%d\n", i, gnrc_rpl_instances[i].state);
    }

    for (uint8_t i = 0; i < GNRC_RPL_PARENTS_NUMOF; ++i) {
        printf("rpl_status,parent,%d,%d\n", i, gnrc_rpl_parents[i].state);
    }

    gnrc_rpl_dodag_t *dodag = NULL;
    char addr_str[IPV6_ADDR_MAX_STR_LEN];
    uint64_t tc;

    for (uint8_t i = 0; i < GNRC_RPL_INSTANCES_NUMOF; ++i) {
        if (gnrc_rpl_instances[i].state == 0) {
            continue;
        }

        dodag = &gnrc_rpl_instances[i].dodag;

        printf("rpl_stats_instance,%d,%" PRIkernel_pid ",%d,%d,%d,%d\n",
                gnrc_rpl_instances[i].id, dodag->iface,
                gnrc_rpl_instances[i].mop, gnrc_rpl_instances[i].of->ocp,
                gnrc_rpl_instances[i].min_hop_rank_inc, gnrc_rpl_instances[i].max_rank_inc);

        tc = xtimer_left_usec(&dodag->trickle.msg_timer);
        tc = (int64_t) tc == 0 ? 0 : tc / US_PER_SEC;

        printf("rpl_stats_dodag,%d,%s,%d,%s,%s,%d,%d,%d,%d,%" PRIu32 "\n", gnrc_rpl_instances[i].id,
               ipv6_addr_to_str(addr_str, &dodag->dodag_id, sizeof(addr_str)),
               dodag->my_rank, (dodag->node_status == GNRC_RPL_LEAF_NODE ? "Leaf" : "Router"),
               ((dodag->dio_opts & GNRC_RPL_REQ_DIO_OPT_PREFIX_INFO) ? "on" : "off"),
               (1 << dodag->dio_min), dodag->dio_interval_doubl, dodag->trickle.k,
               dodag->trickle.c, (uint32_t) (tc & 0xFFFFFFFF));

        gnrc_rpl_parent_t *parent = NULL;
        LL_FOREACH(gnrc_rpl_instances[i].dodag.parents, parent) {
            printf("rpl_stats_parent,%d,%s,%d\n", gnrc_rpl_instances[i].id, ipv6_addr_to_str(addr_str, &parent->addr, sizeof(addr_str)), parent->rank);
        }
    }
    return 0;
}

static void _print_neighbors(netif_t *dev)
{
    netstats_nb_t *stats = &dev->neighbors.pstats[0];
    char l2addr_str[3 * L2UTIL_ADDR_MAX_LEN];
    
    for (unsigned i = 0; i < NETSTATS_NB_SIZE; ++i) {
        netstats_nb_t *entry = &stats[i];

        if (entry->l2_addr_len == 0) {
            continue;
        }
        printf("neighbor_stats,");
        printf("%-24s,",
               gnrc_netif_addr_to_str(entry->l2_addr, entry->l2_addr_len, l2addr_str));
        if (netstats_nb_isfresh(dev, entry)) {
            printf("%5u,", (unsigned)entry->freshness);
        } else {
            printf("STALE,");
        }

        printf("%3u%%,", (100 * entry->etx) / NETSTATS_NB_ETX_DIVISOR);
        printf("%4"PRIu16",%8"PRIu16",", entry->tx_count, entry->rx_count);
        printf("%4i,", (int8_t) entry->rssi);
        printf("%u,", entry->lqi);
        printf("%7"PRIu32, entry->time_tx_avg);
        printf("\n");
    }
}

/* Reads the sensor in an infinite loop. */
static void *_run_stats_loop(void *arg)
{
    (void)arg;

    printf("neighbor_stats,L2 address,fresh,etx,sent,received,rssi (dBm),lqi,avg tx time (µs)\n");
    printf("rpl_stats,Packet Type,Measurement Type,RX unicast,TX unicast,RX multicast,TX multicast\n");
    printf("stats,success,layer,rx packets,rx bytes,tx packets,tx multicast packets,tx bytes,tx succeeded,tx errors\n");
    printf("rpl_status,Type of table,Index of the table,Table status\n");
    printf("rpl_stats_instance,Instance ID,Interface ID,Mode of Operation,Objective Code Point,Min Hop Rank Increase,Max Rank Increase\n");
    printf("rpl_stats_dodag,Instance ID,IPv6 Adress,Rank,Role,Prefix Information,Trickle Interval Size Min,Trickle Interval Size Max,Trickle Redundancy Constant,Trickle Counter,Trickle TC\n");
    printf("rpl_stats_parent,Instance ID,IPv6 Adress,Rank,Lifetime\n");

    while (1)
    {
        netif_t *netif = NULL;
        gnrc_netif_t *gnrc_netif = NULL;
        while ((netif = netif_iter(netif)))
        {
            _netif_stats(netif, NETSTATS_LAYER2);
            _netif_stats(netif, NETSTATS_IPV6);
        }
        while ((gnrc_netif = gnrc_netif_iter(gnrc_netif))) {
            _print_neighbors(&gnrc_netif->netif);
        }
        rpl_stats();
        rpl_dodag_show();
        xtimer_usleep(1 * US_PER_SEC);
    }
    return NULL;
}

extern int _gnrc_netif_config(int argc, char **argv);

int main(void)
{
    puts("info,message");
    printf("info,wait for the IPV6 address of the server (max len: %d)\n", IPV6_ADDR_MAXLEN);
    readline(server_address, IPV6_ADDR_MAXLEN);
    readline(generation_type, GENERATION_TYPE_MAXLEN);
    readline(exp_parameter_str, EXP_PARAMETER_MAXLEN);
    readline(period_parameter_str, PERIOD_PARAMETER_MAXLEN);
    readline(packet_size_str, PACKET_SIZE_MAXLEN);
    sscanf(exp_parameter_str, "%f", &exp_parameter);
    sscanf(period_parameter_str, "%f", &period_parameter);
    packet_size = atoi(packet_size_str);
    xtimer_sleep(20);
    printf("info,The server address is '%s'\n", server_address);
    printf("info,The server port is '%s'\n", server_port);
    printf("info,Generation type is '%s'\n", generation_type);
    printf("info,Exponential parameter is '%f'\n", exp_parameter);
    printf("info,Periodic parameter is '%f'\n", period_parameter);
    printf("info,Packet size is '%d'\n", packet_size);
    puts("info,wait 10 sec before the network is ready to initialize the sensors");
    xtimer_sleep(10);
    _gnrc_netif_config(0, NULL);

    puts("info,Starting the stats thread");
    thread_create(stats_thread_stack, sizeof(stats_thread_stack),
                  THREAD_PRIORITY_MAIN - 1, THREAD_CREATE_STACKTEST,
                  _run_stats_loop, NULL, "read_stats_thread");


    puts("info,Starting the sensors thread");
    printf("udp,payload size,destination address,destination port,payload\n");
    
    if (strncmp(generation_type, "EXPONENTIAL", strlen(generation_type)) == 0) {
        puts("info,Starting the exponential sensor thread");
        thread_create(exponential_sensors_thread_stack, sizeof(exponential_sensors_thread_stack),
                    THREAD_PRIORITY_MAIN - 1, THREAD_CREATE_STACKTEST,
                    _run_exponential_sensor_loop, NULL, "exponential_read_sensors_thread");
    } else if (strncmp(generation_type, "PERIODIC", strlen(generation_type)) == 0) {
        puts("info,Starting the periodic sensor thread");
        thread_create(periodic_sensors_thread_stack, sizeof(periodic_sensors_thread_stack),
                    THREAD_PRIORITY_MAIN - 1, THREAD_CREATE_STACKTEST,
                    _run_periodic_sensor_loop, NULL, "periodic_read_sensors_thread");
    } else if (strncmp(generation_type, "HYBRID", strlen(generation_type)) == 0) {
        puts("info,Starting both sensor threads (exponential, periodic)");
        thread_create(exponential_sensors_thread_stack, sizeof(exponential_sensors_thread_stack),
            THREAD_PRIORITY_MAIN - 1, THREAD_CREATE_STACKTEST,
            _run_exponential_sensor_loop, NULL, "exponential_read_sensors_thread");
        thread_create(periodic_sensors_thread_stack, sizeof(periodic_sensors_thread_stack),
            THREAD_PRIORITY_MAIN - 1, THREAD_CREATE_STACKTEST,
            _run_periodic_sensor_loop, NULL, "periodic_read_sensors_thread");
    } else {
        printf("info,Wrong type of packet generation.\n");
        exit(0);
    }
    /* should be never reached */
    return 0;
}