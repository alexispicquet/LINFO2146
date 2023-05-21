#include "contiki.h"
#include "net/netstack.h"
#include "dev/serial-line.h"
#include "net/ipv6/uip-ds6.h"
#include "net/ipv6/uip-udp-packet.h"
#include "net/ipv6/simple-udp.h"
#include <stdio.h>
#include <string.h>

#define SERVER_IP_ADDR "127.0.0.1"
#define SERVER_PORT 60001

#define MAX_PAYLOAD_LEN 128

PROCESS(border_router, "Border Router Process");
AUTOSTART_PROCESSES(&border_router);

static struct simple_udp_connection udp_conn;

PROCESS_THREAD(border_router, ev, data)
{
  PROCESS_BEGIN();

  /* Set up serial communication */
  serial_line_init();

  /* Initialize UDP connection */
  simple_udp_register(&udp_conn, SERVER_PORT, NULL, SERVER_PORT, NULL);

  while (1) {
    PROCESS_WAIT_EVENT_UNTIL(ev == serial_line_event_message && data != NULL);
    char* received_data = (char*)data;

    /* Parse received data */
    int message_type;
    int source_mote;
    float sensor_value;

    sscanf(received_data, "%d/%d/%f", &message_type, &source_mote, &sensor_value);

    /* Send data to server via UDP */
    char message[MAX_PAYLOAD_LEN];
    snprintf(message, sizeof(message), "%d/%d/%f", message_type, source_mote, sensor_value);

    uip_ipaddr_t server_addr;
    uip_ip6addr(&server_addr, 0xaaaa, 0, 0, 0, 0, 0, 0, 0x1); 

    simple_udp_sendto(&udp_conn, message, strlen(message), &server_addr);

 
  }

  PROCESS_END();
}
