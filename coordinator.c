#include "contiki.h"

#include "sys/node-id.h"
#include "net/netstack.h"
#include "net/nullnet/nullnet.h"

#include "packet.c"
#include "cc2420.h"

#include <string.h>
#include <stdlib.h>
#include <stdio.h> /* For printf() */

/* Log configuration */
#include "sys/log.h"
#define LOG_MODULE "App"
#define LOG_LEVEL LOG_LEVEL_INFO

/* Configuration */
#define SEND_INTERVAL (4 * CLOCK_SECOND)

#if MAC_CONF_WITH_TSCH
#include "net/mac/tsch/tsch.h"
//static linkaddr_t dest_addr =         {{ 0x01, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00 }};
static linkaddr_t coordinator_addr =  {{ 0x01, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00 }};
#endif /* MAC_CONF_WITH_TSCH */


//static linkaddr_t parent;
static pkt_list_t* neighbours;
static pkt_list_t* to_ack;
static pkt_list_t* kids;

/*---------------------------------------------------------------------------*/
PROCESS(nullnet_example_process, "NullNet broadcast example");
AUTOSTART_PROCESSES(&nullnet_example_process);

/*---------------------------------------------------------------------------*/
void input_callback(const void *data, uint16_t len,
  const linkaddr_t *src, const linkaddr_t *dest)
{
  if(len == sizeof(packet_t)) {
    if(neighbours==NULL) {
      neighbours = init_list();
    } if(to_ack==NULL) {
      to_ack = init_list();
    } if(kids==NULL) {
      kids = init_list();
    }
    
    packet_t pkt;
    memcpy(&pkt, data, sizeof(pkt));
    
    pkt.rssi = cc2420_last_rssi;    
    uint8_t res = add_parent(neighbours, &pkt, *src);
    uint8_t res2 = add_acker(to_ack, &pkt, *src);    
    if(pkt.status==PARENT) {
        add_acker(kids, &pkt, *src);
    }
    LOG_INFO_LLADDR(src);
    LOG_INFO(" -> %u, status %u, %u %u action, #nie %u, #kids %u",node_id, pkt.status, res,res2, size_list(neighbours), size_list(kids));
    LOG_INFO_("\n");
  }
}
/*---------------------------------------------------------------------------*/
PROCESS_THREAD(nullnet_example_process, ev, data)
{
  static struct etimer periodic_timer;
  static packet_t* pkt;
  static unsigned action = FALSE;

  PROCESS_BEGIN();

#if MAC_CONF_WITH_TSCH
  tsch_set_coordinator(linkaddr_cmp(&coordinator_addr, &linkaddr_node_addr));
#endif /* MAC_CONF_WITH_TSCH */

  /* Initialize NullNet */
  nullnet_buf = NULL;
  nullnet_len = 0;
  nullnet_set_input_callback(input_callback);

  pkt = init_pkt(SEARCH, COOR, 0);
  srand(node_id);
  etimer_set(&periodic_timer, 4* CLOCK_SECOND);
  while(1) {
    PROCESS_WAIT_EVENT_UNTIL(etimer_expired(&periodic_timer));
    
    // respond to all messages that require an ack
    uint8_t size2 = size_list(kids);
    pkt_list_t* temp = to_ack;
    unsigned kid_ok[size2];
    for(uint8_t i=0; i<size2; i++){
        kid_ok[i] = FALSE;
    }
    
    while(temp!=NULL && temp->head!=NULL) {
        //message from kid
        uint8_t pos = src_in_list(kids, temp->src);
        if(pos<100) {
            kid_ok[pos] = TRUE;
        }
        if(temp->head->status%2==0) {
            nullnet_buf = (uint8_t *)pkt;
            nullnet_len = sizeof(*pkt);
            pkt = init_pkt(temp->head->status +1, COOR, 0);
            LOG_INFO("%u -status %u ack-> ",node_id, temp->head->status);
            LOG_INFO_LLADDR(&temp->src);
            LOG_INFO_("\n");
    
            memcpy(nullnet_buf, pkt, sizeof(*pkt));
            nullnet_len = sizeof(*pkt);

            NETSTACK_NETWORK.output(&temp->src);
            action = TRUE;
        } else {
            LOG_INFO_LLADDR(&temp->src);
            LOG_INFO(" -ack status %u (skipped)-> %u",temp->head->status, node_id);
            LOG_INFO_("\n");
        }
        temp = temp->next;
    }
    // remove kids
    for(uint8_t i=0; i<size2; i++){
        if(kid_ok[i] == FALSE) {
            remove_pos(kids, size2-1-i);
            LOG_INFO("remove kid #%u",i);
            LOG_INFO_("\n");
        }
    }
    // free messages received
    free_list(to_ack);
    to_ack = NULL;
    
    // if no action listen
    if(action==FALSE) {
        LOG_INFO("no action");
        LOG_INFO_("\n");
        NETSTACK_NETWORK.output(NULL);
    }
    // reset
    action = FALSE;
    nullnet_buf = NULL;
    nullnet_len = 0;
    etimer_reset(&periodic_timer);
  }
  free_pkt(pkt);
  free_list(neighbours);

  PROCESS_END();
}
/*---------------------------------------------------------------------------*/
