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
static linkaddr_t coordinator_addr =  {{ 0x01, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00 }};
#endif /* MAC_CONF_WITH_TSCH */

static uint8_t rank = 100;
static uint8_t new_parent = FALSE;
static linkaddr_t parent;
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
    uint8_t res = add_parent(neighbours, init_pkt2(&pkt), *src);
    uint8_t res2 = add_acker(to_ack, init_pkt2(&pkt), *src);
    LOG_INFO_LLADDR(src);
    LOG_INFO_(" -> %u, status %u, %u %u action, #nie %u, #ack %u",node_id, pkt.status, res,res2, size_list(neighbours), size_list(to_ack));
    LOG_INFO_("\n");
  }
}
/*---------------------------------------------------------------------------*/
PROCESS_THREAD(nullnet_example_process, ev, data)
{
  static struct etimer periodic_timer;
  static packet_t* pkt;
  
  static unsigned parent_cnt = 0;
  static unsigned parent_ok = FALSE;
  static unsigned parent_wait = FALSE;
  static unsigned action = FALSE;
  
  PROCESS_BEGIN();

#if MAC_CONF_WITH_TSCH
  tsch_set_coordinator(linkaddr_cmp(&coordinator_addr, &linkaddr_node_addr));
#endif /* MAC_CONF_WITH_TSCH */

  /* Initialize NullNet */
  nullnet_buf = NULL;
  nullnet_len = 0;
  nullnet_set_input_callback(input_callback);

  srand(node_id);
  etimer_set(&periodic_timer, SEND_INTERVAL);
  while(1) {
    PROCESS_WAIT_EVENT_UNTIL(etimer_expired(&periodic_timer));
    
    // choose parent
if(size_list(neighbours)>0 && (rank==100 || linkaddr_cmp(&parent,&(neighbours->src))==0)) {
        rank = 100;
        nullnet_buf = (uint8_t *)pkt;
        nullnet_len = sizeof(*pkt);
        
        pkt = init_pkt(PARENT, SENSOR, rank);
        LOG_INFO_("%u -parent-> ",node_id);
        LOG_INFO_LLADDR(&(neighbours->src));
        LOG_INFO_("\n");
        
        memcpy(nullnet_buf, pkt, sizeof(*pkt));
        nullnet_len = sizeof(*pkt);
        NETSTACK_NETWORK.output(&(neighbours->src));
        action = TRUE;
        // possible deadlock between 2 sensors
        if (neighbours->head->type==COOR) {
            parent_wait = TRUE;
        }
    }
    // update kids counter (+1)
    pkt_list_t* temp = kids;
    while(temp!=NULL && temp->head!=NULL) {
        temp->counter += 1;
        temp = temp->next;
    }
    // respond to all messages that require an ack
    temp = to_ack;
    while(temp!=NULL && temp->head!=NULL) {
        LOG_INFO_LLADDR(&temp->src);
        LOG_INFO_(" -> %u, status %u",node_id, temp->head->status);
        LOG_INFO_("\n");
        action = TRUE;
        // parent confirmation
        if(temp->head->status==PARENT_ACK && rank==100) {
            parent = temp->src;
            rank = temp->head->rank +1;
            new_parent = TRUE;
            parent_cnt = 0;
            LOG_INFO_LLADDR(&temp->src);
            LOG_INFO_(" -parent ack-> %u", node_id);
            LOG_INFO_("\n");
        }
        // message from parent
        if(rank!=100 && linkaddr_cmp(&parent, &(temp->src))!=0) {
            // parent not elligible anymore
            if(temp->head->status==PARENT) {
                rank=100;
                pkt_list_t* first;
                free_pkt(neighbours->head);
                first = neighbours;
                neighbours = neighbours->next;
                free(first);
                parent_cnt = 0;
                
                free_list(kids);
                kids = NULL;
        
                LOG_INFO_("remove parent ");
                LOG_INFO_LLADDR(&parent);
                LOG_INFO_("\n");
                temp = temp->next;
                continue;
            }
            // parent still on
            if(rank!=100) {
                LOG_INFO_("parent message");
                LOG_INFO_("\n");
                parent_ok = TRUE;
                temp = temp->next;
                continue;
            }
        }
        //message from kid should update alone
        
        if(temp->head->status%2==0) {
            if(temp->head->status==PARENT) {
                // no parent => cant have kids
                if(rank==100) {
                    LOG_INFO_LLADDR(&temp->src);
                    LOG_INFO_(" -parent (denied)-> %u", node_id);
                    LOG_INFO_("\n");
                    temp = temp->next;
                    continue;
                }
                // parent => add kid
                add_acker(kids, init_pkt2(temp->head), temp->src);
                LOG_INFO_("%u new kid #%u", node_id, size_list(kids));
                LOG_INFO_("\n");
            }
            nullnet_buf = (uint8_t *)pkt;
            nullnet_len = sizeof(*pkt);
            pkt = init_pkt(temp->head->status +1, SENSOR, rank);
            LOG_INFO_("%u -status %u ack-> ",node_id, temp->head->status);
            LOG_INFO_LLADDR(&temp->src);
            LOG_INFO_("\n");
    
            memcpy(nullnet_buf, pkt, sizeof(*pkt));
            nullnet_len = sizeof(*pkt);

            NETSTACK_NETWORK.output(&(temp->src));
        } else {
            LOG_INFO_LLADDR(&temp->src);
            LOG_INFO_(" -ack status %u (skipped)-> %u", temp->head->status, node_id);
            LOG_INFO_("\n");
        }
        temp = temp->next;
    }
    // remove kids
    temp = kids;
    while(temp!=NULL && temp->head!=NULL) {
        if(temp->counter > 10) {
            pkt_list_t* first;
            free_pkt(temp->head);
            first = temp;
            temp = temp->next;
        
            LOG_INFO_("remove kid ");
            LOG_INFO_LLADDR(&first->src);
            LOG_INFO_("\n");
            
            free(first);
        } else {
            temp = temp->next;
        }
    }
    // free messages received
    free_list(to_ack);
    to_ack = NULL;
    // if new parent or no parent broadcast
    if(new_parent==TRUE || (rank==100 && parent_wait==FALSE)) {
        nullnet_buf = (uint8_t *)pkt;
        nullnet_len = sizeof(*pkt);
        pkt = init_pkt(SEARCH, SENSOR, rank);
        LOG_INFO_("%u broadcast, rank %u, parent ",node_id, rank);
        if(rank<100) {LOG_INFO_LLADDR(&parent);}
        LOG_INFO_("\n");
    
        memcpy(nullnet_buf, pkt, sizeof(*pkt));
        nullnet_len = sizeof(*pkt);

        NETSTACK_NETWORK.output(NULL);
        action = TRUE;
    }
    // update counter
    if(parent_ok==FALSE){
        parent_cnt++;
    }
    // remove parent
    if(parent_cnt>10){
        rank=100;
        pkt_list_t* first;
        free_pkt(neighbours->head);
        first = neighbours;
        neighbours = neighbours->next;
        free(first);
        parent_cnt = 0;
        
        free_list(kids);
        kids = NULL;
        
        LOG_INFO_("remove parent ");
        LOG_INFO_LLADDR(&parent);
        LOG_INFO_("\n");
    }
    
    // if no action listen
    if(action==FALSE) {
        LOG_INFO_("no action .. parent ");
        LOG_INFO_LLADDR(&parent);
        LOG_INFO_("\n");
        NETSTACK_NETWORK.output(NULL);
    }
    
    // reset
    parent_wait = FALSE;
    action = FALSE;
    parent_ok = FALSE;
    new_parent = FALSE;
    nullnet_buf = NULL;
    nullnet_len = 0;
    etimer_reset(&periodic_timer);
  }
  free_pkt(pkt);
  free_list(neighbours);

  PROCESS_END();
}
/*---------------------------------------------------------------------------*/
