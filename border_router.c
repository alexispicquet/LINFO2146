#include "contiki.h"
#include "net/netstack.h"
#include "net/nullnet/nullnet.h"
#include "sys/log.h"

#include <string.h>
#include <stdio.h>

/* Log configuration */
#define LOG_MODULE "BorderRouter"
#define LOG_LEVEL LOG_LEVEL_INFO

/* Configuration */
#define SERVER_PORT 60001

static struct uip_udp_conn *server_conn;

/*---------------------------------------------------------------------------*/
PROCESS(borderouter_process, "Border Router Process");
AUTOSTART_PROCESSES(&borderouter_process);

/*---------------------------------------------------------------------------*/
void input_callback(const void *data, uint16_t len,
                    const linkaddr_t *src, const linkaddr_t *dest)
{
    /* Send received data to the server */
    uip_udp_packet_send(server_conn, data, len);
}

/*---------------------------------------------------------------------------*/
PROCESS_THREAD(borderouter_process, ev, data)
{
PROCESS_BEGIN();

/* Initialize NullNet */
nullnet_buf = NULL;
nullnet_len = 0;
nullnet_set_input_callback(input_callback);

/* Create UDP connection to the server */
server_conn = udp_new(NULL, UIP_HTONS(SERVER_PORT), NULL);
udp_bind(server_conn, UIP_HTONS(SERVER_PORT));

while (1) {
PROCESS_WAIT_EVENT();
}

PROCESS_END();
}
/*---------------------------------------------------------------------------*/
