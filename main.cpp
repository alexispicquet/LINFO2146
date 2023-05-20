#include "contiki.h"
#include "net/netstack.h"
#include "net/nullnet/nullnet.h"
#include "random.h"
#include "clock.h"
#include <stdio.h>
#include <string.h>

PROCESS(coordinator_process, "Coordinator Process");
AUTOSTART_PROCESSES(&coordinator_process);

#define NUM_SENSOR_NODES 5
#define COORDINATOR_NODE_ID 0

typedef struct {
    uint8_t id;
    uint8_t counter;
    uint8_t time_slot;
    clock_time_t time_offset;
} sensor_node_t;

sensor_node_t sensor_nodes[NUM_SENSOR_NODES];

static void send_data(uint8_t node_id, uint8_t counter) {
    printf("Sending data from noode %d: Counter = %d\n", node_id, counter);

    // Create a packet and fill in the data
    char packet[10];
    snprintf(packet, sizeof(packet), "Data: %d", counter);

    // Send the packet to the server via the border router
    nullnet_buf = (uint8_t*)packet;
    nullnet_len = strlen(packet) + 1;
    NETSTACK_NETWORK.output(NULL);
}

static void berkeley_time_sync() {
    static uint8_t sync_count = 0;
    static clock_time_t sum_offset = 0;

    // Get the clock time of the coordinator
    clock_time_t coordinator_time = clock_time();

    // Iterate through the sensoor nodes
    for (uint8_t i = 0; i < NUM_SENSOR_NODES; i++) {
        // Get the clock time oof the sensor node
        clock_time_t node_time = coordinator_time + sensor_nodes[i].time_offset;

        // Send the clock time oof the coordinator to the sensor node
        // ...

        // Receive the clock time of the sensor node
        // ...

        // Compute the time offset between the coordinator and the sensor node
        clock_time_t offset = // ...

                // Accumulate the time offset for averaging
                sum_offset += offset;
        sync_count++;
    }

    // Average the time offset
    clock_time_t avg_offset = sum_offset / sync_count;

    // Update the time offset for each sensor node
    for (uint8_t i = 0; i < NUM_SENSOR_NODES; i++) {
        sensor_nodes[i].time_offset += avg_offset;
    }
}

static void coordinator_process_loop() {
    static uint8_t current_slot = 0;

    // Check if it's the current slot for the coordinator
    if (COORDINATOR_NODE_ID == current_slot) {
        // Iterate through the sensor nodes and ask for data
        for (uint8_t i = 0; i < NUM_SENSOR_NODES; i++) {
            if (sensor_nodes[i].time_slot == current_slot) {
                // Ask the sensor node if it has data to send
                if (sensor_nodes[i].counter > 0) {
                    send_data(sensor_nodes[i].id, sensor_nodes[i].counter);
                    sensor_nodes[i].counter = 0;
                }
            }
        }

        // Perform Berkeley Time Synchronization every 5 slots
        if (current_slot % 5 == 0) {
            berkeley_time_sync();
        }
    }

    // Move to the next time slot
    current_slot = (current_slot + 1) % NUM_SENSOR_NODES;

    // Schedule the next loop iteration
    etimer_set(10); // Adjust the duration according to your requirements
    PROCESS_WAIT_EVENT_UNTIL(etimer_expired(&et));
}

PROCESS_THREAD(coordinator_process, ev, data) {
PROCESS_BEGIN();

// Initialize sensor nodes
for (uint8_t i = 0; i < NUM_SENSOR_NODES; i++) {
sensor_nodes[i].id = i + 1;
sensor_nodes[i].counter = 0;
sensor_nodes[i].time_slot = i;
sensor_nodes[i].time_offset = 0;
}

// Start the coordinator process loop
etimer_set(10);

while (1) {
PROCESS_WAIT_EVENT();

if (ev == PROCESS_EVENT_TIMER) {
coordinator_process_loop();
}
}

PROCESS_END();
}
