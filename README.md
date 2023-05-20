# LINFO2146 - Code Readme

This code is an implementation of a coordinator node in a wireless sensor network using the Contiki operating system. The coordinator node coordinates the data transmission and time synchronization among multiple sensor nodes.

## Usage
1. Initialize the sensor nodes by setting their IDs, counters, time slots, and time offsets.
2. Start the coordinator process loop.
3. The coordinator node will periodically check for data from sensor nodes in its current time slot and request the data if available.
4. The coordinator node will perform Berkeley Time Synchronization every 5 time slots.
5. Adjust the duration of each time slot by modifying the value in the `etimer_set` function.
6. Customize the implementation to meet specific requirements.
