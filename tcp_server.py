import datetime
import msvcrt
import socket
import os
import re
from pathlib import Path
import sys
# from _thread import *
import threading
import time


def convert_timestamp(epoch_microseconds):
    timestamp = datetime.datetime.fromtimestamp(epoch_microseconds / 1000000.0)
    return timestamp.strftime("%d-%m-%Y %H:%M:%S")

def evaluate_format(data):
    # pattern = r'^([A-Za-z0-9]+\.txt):(\d+),(-?\d+),(\d+)$'
    pattern = r'^(.*)/(.*)/(.*):(\d+),(-?\d+),(\d+)$'

    match = re.match(pattern, data)
    if match:
        # Extract the captured groups
        dir1 = match.group(1)
        dir2 = match.group(2)
        filename = match.group(3)
        id_value = (match.group(4))
        value = (match.group(5))
        timestamp = (match.group(6))
        
        # Do something with the extracted values
        print("\n")
        print("------------------------")
        print("Valid format:")
        print("Filename:", filename)
        print("Dir:", dir1+"/"+dir2+"/")
        print("ID:", id_value)
        print("Value:", value)
        print("Timestamp:", timestamp)
        print("Timestamp:",convert_timestamp(int(timestamp)))
        print("------------------------")
        print("\n")
        save_data_to_file(id_value+","+value+","+timestamp+"\n",dir1+"/"+dir2+"/",filename)
        return "ESP_OK"
    else:
        # Save the string for later use
        print("Invalid format. Saving for later:", data)
        return data if data != '' else "ESP_OK"


def save_data_to_file(data,file_path,filename):
    # Specify the file path where you want to save the data
    
    Path(file_path).mkdir(mode=777,parents=True, exist_ok=True)
    full_file_path = file_path+"/"+filename
    with open(full_file_path, "a") as file:
        file.write(data)
    print(f"Data saved to file: {full_file_path}")


stop_flag = threading.Event()


def threaded_client(sock, thread_count):

    sock.setblocking(0)
    max_idle_time = 30#s
    last_data_timestamp = time.time()
    aux_timestamp = 0
    preverr = ''

    while not stop_flag.is_set():
        
        try:
            # If there's data from a client, handle it as before
            data = sock.recv(2048).decode()
            if(not data):
                raise  ValueError("no data")
            
            last_data_timestamp = time.time()
            print("data = "+data)
            streams = data.split("\n")
            print("stream = "+streams)
            for stream in streams:
                if not len(stream): #string vazia ''
                    continue

                err = evaluate_format(stream)
                if err != "ESP_OK":
                    evaluate_format(preverr + err)
                    preverr = err

        except (socket.error, ValueError):
            seconds_since_last_data = int(time.time() - last_data_timestamp)

            if(seconds_since_last_data != aux_timestamp):
                print(f"Remaining time for {sock.getsockname()} = {max_idle_time - seconds_since_last_data}")
                aux_timestamp = seconds_since_last_data

            if(seconds_since_last_data >= max_idle_time):
                print("Idle time expired!")
                break # If there's no data from a client, move on to the next client
            pass

    print(f"Closing socket {sock.getsockname()}")
    sock.close()
    thread_count[0]-=1


def tcp_socket_server(config, client_sockets, threads):
    server_socket = socket.socket()
    t_count = [0]
    
    try:
        server_socket.bind(config)
    except socket.error as e:
        print(str(e))
        return
    
    print('TCP server stared, Waiting for a Connection...')
    server_socket.listen(5)
    server_socket.setblocking(0)

    while not stop_flag.is_set():
        try:
            client_socket, address = server_socket.accept()
            print('Connected to: ' + address[0] + ':' + str(address[1]))
            client_sockets.append(client_socket)
            t = threading.Thread(target=threaded_client, args=(client_socket, t_count))
            threads.append(t)
            t.start()
            # threads.append(start_new_thread(threaded_client, (client_socket, t_count)))
            t_count[0] += 1
            print('Thread Number: ' + str(t_count[0]))
        except:
           
            pass
    
    print("Closing TCP server socket...")
    server_socket.close()

def udp_socket_server(port):
    # Create a UDP socket
    server_socket = socket.socket(socket.AF_INET, socket.SOCK_DGRAM)

    # Enable broadcasting on the socket
    server_socket.setsockopt(socket.SOL_SOCKET, socket.SO_BROADCAST, 1)

    # Bind the socket to a specific IP address and port
    server_address = ("", port)  # Use an empty string to bind to all available network interfaces
    server_socket.bind(server_address)
    server_socket.setblocking(0)
    print("UDP server started and listening for broadcast messages.")

    server_ip = socket.gethostbyname(socket.gethostname())

    while not stop_flag.is_set():
        # Receive the data and client address
        try:
            data, client_address = server_socket.recvfrom(1024)

            # Decode the received data assuming it's a string
            received_message = data.decode("utf-8")

            print(f"Received broadcast message: {received_message} from {client_address}")

            # Get the IP address of the server

            # Send the server IP address back to the client
            server_socket.sendto(server_ip.encode("utf-8"), client_address)
        except:
            pass
    
    print("Closing UDP server socket...")
    server_socket.close()



def server():
    
    host = socket.gethostname()
    port = 23
    threads = []
    client_sockets = []

    t_udp = threading.Thread(target=udp_socket_server, args=(port,))
    t_tcp = threading.Thread(target=tcp_socket_server, args=((host,port), client_sockets, threads))

    t_udp.start()
    t_tcp.start()


    while True:
        if msvcrt.kbhit():
            # Read the key
            key = msvcrt.getch()
            if key.decode().upper() == 'Q':
                # If 'Q' is pressed, quit the server gracefully
                print("Quitting the server...")

                stop_flag.set()
                # time.sleep(3)
                return




if __name__ == "__main__":
    server()
    sys.exit()

