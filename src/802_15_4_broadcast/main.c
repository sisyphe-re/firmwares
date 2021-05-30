/*
 * Copyright (C) 2018 Robert Hartung <hartung@ibr.cs.tu-bs.de>
 *
 * This file is subject to the terms and conditions of the GNU Lesser
 * General Public License v2.1. See the file LICENSE in the top level
 * directory for more details.
 */

#include <stdio.h>
#include <string.h>

#include "msg.h"
#include "thread.h"
#include "shell.h"
#include "shell_commands.h"
#include "xtimer.h"
#include "net/gnrc.h"
#include "net/gnrc/netreg.h"
#include "net/gnrc/netif/ieee802154.h"

#define SEND_INTERVAL (1)
#define RCV_QUEUE_SIZE (16)

char dump_thread_stack[512+256];
char send_thread_stack[512+256];

kernel_pid_t send_thread_pid = 0;

static void _dump(gnrc_pktsnip_t *pkt) {
    /// Iterate though snippets (Linked List)
    gnrc_pktsnip_t *snip = pkt;
    /// Payload and header not known
    gnrc_pktsnip_t *payload = NULL;
    gnrc_netif_hdr_t *hdr = NULL;
    
    while(snip != NULL) {
        switch(snip->type)  {
            case GNRC_NETTYPE_UNDEF :
                /// No specific network type -> payload
                payload = snip;
            break;
            case GNRC_NETTYPE_NETIF :
                /// Interface data -> header
                hdr = snip->data;
                (void) hdr;
                /// Print payload
                printf("Received message: '%s'\n", (char*)payload->data);
            break;
            default : 

            break;
        }
        snip = snip->next;
    }
    gnrc_pktbuf_release(pkt);
}

void *dump_thread(void *arg)
{
    (void) arg;
    msg_t dump_thread_msg_queue[RCV_QUEUE_SIZE];
    msg_init_queue(dump_thread_msg_queue, RCV_QUEUE_SIZE);

    gnrc_netreg_entry_t me_reg = GNRC_NETREG_ENTRY_INIT_PID(GNRC_NETREG_DEMUX_CTX_ALL, thread_getpid());
    gnrc_netreg_register(GNRC_NETTYPE_UNDEF, &me_reg);

    msg_t msg;
    while(1) {
        if(msg_receive(&msg) != 1) {
            puts("Unable to receive message");
            continue;
        }
        printf("[dump_thread] message received: %d\n", msg.type);
        switch(msg.type) {
            case GNRC_NETAPI_MSG_TYPE_RCV :
                _dump( msg.content.ptr );
            break;
        }
    }
    puts("END OF dump_thread");
    return NULL;
}

void *send_thread(void *arg)
{
    gnrc_netif_t *ieee802154_netif = arg;

    /// 0 Adress length means we want to use broadcast
    size_t addr_len = 0;
    uint8_t addr[GNRC_NETIF_L2ADDR_MAXLEN];
    gnrc_pktsnip_t *pkt, *hdr;
    gnrc_netif_hdr_t *nethdr;

    /// Send packet as broadcast
    uint8_t flags = 0 | GNRC_NETIF_HDR_FLAGS_BROADCAST;
    
    /// payload
    char message[] = "RIOT says hello.\0";
    
    while(1) {
        /// Sleep 1 second
        xtimer_sleep(SEND_INTERVAL);
        
        pkt = gnrc_pktbuf_add(NULL, message, sizeof(message), GNRC_NETTYPE_UNDEF);
        if (pkt == NULL) {
            puts("ERROR: packet buffer full");
            return NULL;
        }

        hdr = gnrc_netif_hdr_build(NULL, 0, addr, addr_len);
        if (hdr == NULL) {
            puts("ERROR: packet buffer full");
            gnrc_pktbuf_release(pkt);
            return NULL;
        }
        LL_PREPEND(pkt, hdr);
        nethdr = (gnrc_netif_hdr_t *)hdr->data;
        nethdr->flags = flags;
        int ret = gnrc_netapi_send(ieee802154_netif->pid, pkt);
        if (ret < 1) {
            printf("[send_thread] unable to send: %d\n", ret);
            gnrc_pktbuf_release(pkt);
        } else {
            puts("[send_thread] sent message");
        }
    }
    return NULL;
}

int main(void)
{
    thread_create(dump_thread_stack, sizeof(dump_thread_stack), THREAD_PRIORITY_MAIN + 1, THREAD_CREATE_STACKTEST, dump_thread, NULL, "dump_thread");
    
    gnrc_netif_t *netif = NULL;
    if((netif = gnrc_netif_iter(netif))) {
        gnrc_netif_t *ieee802154_netif = netif;
        send_thread_pid = thread_create(send_thread_stack, sizeof(send_thread_stack), THREAD_PRIORITY_MAIN + 2, THREAD_CREATE_STACKTEST, send_thread, ieee802154_netif, "send_thread");
    } else {
        puts("Unable to find netif");
    }

    (void) puts("Welcome to RIOT!");

    return 0;
}