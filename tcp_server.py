import datetime
import msvcrt
import socket
import os
import re
from pathlib import Path
import sys
from _thread import *
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


def threaded_client(connection):
    connection.setblocking(0)
    max_idle_time = 0.5#min
    last_data_timestamp = time.time()
    aux_timestamp = 0
    preverr = ''
    while True:
        try:
            # If there's data from a client, handle it as before
            data = connection.recv(2048).decode()
            
            last_data_timestamp = time.time()
            print(data)
            streams = data.split("\n")
            print(streams)
            for stream in streams:
                if not len(stream): #string vazia ''
                    continue

                err = evaluate_format(stream)
                if err != "ESP_OK":
                    evaluate_format(preverr + err)
                    preverr = err
        except socket.error:
            seconds_since_last_data = int(time.time() - last_data_timestamp)

            if(seconds_since_last_data != aux_timestamp):
                print(f"Remaining time for {connection.getsockname()} = {max_idle_time*60 - seconds_since_last_data}")
                aux_timestamp = seconds_since_last_data

            if(seconds_since_last_data >= max_idle_time*60):
                print("Idle time expired!")
                break # If there's no data from a client, move on to the next client
            pass

    print(f"Closing socket {connection.getsockname()}")
    connection.close()


def tcp_server():
    ServerSocket = socket.socket()
    host = socket.gethostname()
    port = 23
    ThreadCount = 0
    
    try:
        ServerSocket.bind((host, port))
    except socket.error as e:
        print(str(e))

    print('Waiting for a Connection...')
    ServerSocket.listen(5)
    ServerSocket.setblocking(0)

    client_sockets = []
    while True:
        try:
            Client, address = ServerSocket.accept()
            print('Connected to: ' + address[0] + ':' + str(address[1]))
            client_sockets.append(Client)
            Client
            start_new_thread(threaded_client, (Client, ))
            ThreadCount += 1
            print('Thread Number: ' + str(ThreadCount))
        except:
            if msvcrt.kbhit():
                # Read the key
                key = msvcrt.getch()
                if key.decode().upper() == 'Q':
                    # If 'Q' is pressed, quit the server gracefully
                    print("Quitting the server...")
                    for client_socket in client_sockets:
                        client_socket.close()
                    ServerSocket.close()
                    sys.exit()
                # elif key.decode().upper() == "E":
                #     print("Closing sockets...")
                #     for client_socket in client_sockets:
                #         client_socket.close()
            pass



def tcppp_server():
    # Specify the host and port for the server to listen on
    host = socket.gethostname()
    port = 23
    server_socket = socket.socket(socket.AF_INET, socket.SOCK_STREAM)
    # Create a TCP socket and bind it to the specified host and port
    print(f"Binding to {host}:{port}...")
    server_socket.bind((host, port))


    # Listen for incoming connections
    server_socket.listen(1)
    print("TCP server is listening for connections...")

       # Create a list to store the client sockets
    client_sockets = []

    while True:
        # Accept a client connection
        client_socket, address = server_socket.accept()
        print(f"Connection established from: {address}")
        client_sockets.append(client_socket)

        # Set the server socket to non-blocking mode
        server_socket.setblocking(0)

        while True:
            # Check if a key is pressed
            if msvcrt.kbhit():
                # Read the key
                key = msvcrt.getch()
                if key.decode().upper() == 'Q':
                    # If 'Q' is pressed, quit the server gracefully
                    print("Quitting the server...")
                    for client_socket in client_sockets:
                        client_socket.close()
                    server_socket.close()
                    sys.exit()

            # Handle client data
            for client_socket in client_sockets:
                try:
                    client_socket.setblocking(0)
                    # If there's data from a client, handle it as before
                    data = client_socket.recv(2048).decode()
                    print(data)
                    streams = data.split("\n")
                    print(streams)
                    for stream in streams:
                        if not len(stream):
                            continue
                        err = evaluate_format(stream)
                        if err != "ESP_OK":
                            evaluate_format(preverr + err)
                except socket.error:
                    # If there's no data from a client, move on to the next client
                    pass



# Start the TCP server
tcp_server()