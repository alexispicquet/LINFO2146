import socket
import numpy as np

MAX_NUMBER_OF_VALUES = 30
THRESHOLD = 2
valueTable = {}  # {address: [values list]}

def least_square(ytable):
    xtable = np.arange(len(ytable))
    ret = np.polyfit(xtable, ytable, 1)
    if ret[0] > THRESHOLD:
        return 1
    return 0

def add_new_data(address, value):
    if address in valueTable:
        valueTable[address].append(value)
        if len(valueTable[address]) > MAX_NUMBER_OF_VALUES:
            valueTable[address] = valueTable[address][-MAX_NUMBER_OF_VALUES:]
            return least_square(valueTable[address])
    else:
        valueTable[address] = [value]
    return 0

def main():
    server_address = ('localhost', 60001)
    buffer_size = 1024

    with socket.socket(socket.AF_INET, socket.SOCK_STREAM) as server_socket:
        server_socket.bind(server_address)
        server_socket.listen(1)
        print("Server is listening on {}:{}".format(*server_address))

        while True:
            print("Waiting for a connection...")
            client_socket, client_address = server_socket.accept()
            print("Accepted connection from {}:{}".format(*client_address))

            while True:
                data = client_socket.recv(buffer_size).decode().strip()
                if not data:
                    break

                if data.startswith("data"):
                    values = data.split()[1:]
                    print("Received data from border-router:", values)
                    address = tuple(map(int, values[:2]))
                    sensor_value = float(values[2])

                    open_valve = add_new_data(address, sensor_value)
                    if open_valve:
                        print("OPEN THE VALVES FOR {}!".format(address))
                        response = "open {} {}\n".format(*address)
                        client_socket.sendall(response.encode())

            client_socket.close()

if __name__ == "__main__":
    main()
