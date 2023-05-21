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

// Sync interval for Berkeley Time Synchronization Algorithm
#define SYNC_INTERVAL 10

#if MAC_CONF_WITH_TSCH
#include "net/mac/tsch/tsch.h"
//static linkaddr_t dest_addr =         {{ 0x01, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00 }};
static linkaddr_t coordinator_addr =  {{ 0x01, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00 }};
#endif /* MAC_CONF_WITH_TSCH */

static pkt_list_t* neighbours;
static pkt_list_t* to_ack;
static pkt_list_t* kids;

// Variable for storing local time offset
static clock_time_t time_offset = 0;

/*---------------------------------------------------------------------------*/
PROCESS(nullnet_example_process, "NullNet broadcast example");
AUTOSTART_PROCESSES(&nullnet_example_process);

/*---------------------------------------------------------------------------*/
void adjust_local_clock(clock_time_t adjustment) {
    time_offset += adjustment;
}

// Berkeley Time Synchronization Algorithm
void berkeley_sync_algorithm() {
    int node_count = size_list(neighbours);
    clock_time_t time_sum = 0;

    // Sum all clock values from neighbours
    pkt_list_t* temp = neighbours;
    while (temp != NULL) {
        time_sum += temp->head->timestamp;
        temp = temp->next;
    }

    // Average time
    clock_time_t average_time = time_sum / node_count;

    // Adjust local clock
    adjust_local_clock(average_time - clock_time());

    // Update all neighbours
    temp = neighbours;
    while (temp != NULL) {
        clock_time_t adjustment = average_time - temp->head->timestamp;
        // Send adjustment to neighbour. This is a placeholder function
        // and you would need to implement it according to your specific needs
        send_time_adjustment(&temp->src, adjustment);
        temp = temp->next;
    }
}

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
  static unsigned action = FALSE;
  static unsigned iter = 0;

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
    //TODO lower if routing must be always perfect => overhead
    if(iter>20) {iter=0;}
    iter++;
    PROCESS_WAIT_EVENT_UNTIL(etimer_expired(&periodic_timer));
    if(iter % SYNC_INTERVAL == 0) {
    berkeley_sync_algorithm();
    }
    // broadcast if no neighbours or periodically
    clock_time_t current_time = clock_time() + time_offset; // Get synchronized time
    if (current_time % (TOTAL_TIMESLOTS * TIMESLOT_DURATION) == MY_TIMESLOT * TIMESLOT_DURATION) {
    // It's my timeslot, do the transmission
    nullnet_buf = (uint8_t *)pkt;
    nullnet_len = sizeof(*pkt);
    memcpy(nullnet_buf, pkt, sizeof(*pkt));
    nullnet_len = sizeof(*pkt);
    NETSTACK_NETWORK.output(NULL);
    action = TRUE;
    } else {
    // It's not my timeslot, wait for the next timeslot
    etimer_set(&periodic_timer, (MY_TIMESLOT * TIMESLOT_DURATION) - (current_time % (TOTAL_TIMESLOTS * TIMESLOT_DURATION)));
    PROCESS_WAIT_EVENT_UNTIL(etimer_expired(&periodic_timer));
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
        //message from kid should update alone
        
        if(temp->head->status%2==0) {
            // parent => add kid
            if(temp->head->status==PARENT) {
                add_acker(kids, init_pkt2(temp->head), temp->src);
                LOG_INFO_("%u new kid #%u", node_id, size_list(kids));
                LOG_INFO_("\n");
            }
        
            nullnet_buf = (uint8_t *)pkt;
            nullnet_len = sizeof(*pkt);
            pkt = init_pkt(temp->head->status +1, COOR, 0);
            LOG_INFO_("%u -status %u ack-> ",node_id, temp->head->status);
            LOG_INFO_LLADDR(&temp->src);
            LOG_INFO_("\n");
    
            memcpy(nullnet_buf, pkt, sizeof(*pkt));
            nullnet_len = sizeof(*pkt);

            NETSTACK_NETWORK.output(&temp->src);
            action = TRUE;
        } else {
            LOG_INFO_LLADDR(&temp->src);
            LOG_INFO_(" -ack status %u (skipped)-> %u",temp->head->status, node_id);
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
    // if no action listen
    if(action==FALSE) {
        LOG_INFO_("no action");
        LOG_INFO_("\n");
        NETSTACK_NETWORK.output(NULL);
    }
    // reset
    action = FALSE;
    nullnet_buf = NULL;
    nullnet_len = 0;
    etimer_reset(&periodic_timer);

    // Call Berkeley Time Synchronization Algorithm every SYNC_INTERVAL iteration
    if(iter % SYNC_INTERVAL == 0) {
        berkeley_sync_algorithm();
    }
  }
  free_pkt(pkt);
  free_list(neighbours);

  PROCESS_END();
}
/*---------------------------------------------------------------------------*/
