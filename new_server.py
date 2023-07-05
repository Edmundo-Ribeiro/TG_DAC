import datetime
import os
import socket
import re
from pathlib import Path
import sys
import threading
import time

KEEPALIVE = '+'


def convert_timestamp(epoch_microseconds):
    timestamp = datetime.datetime.fromtimestamp(epoch_microseconds / 1000000.0)
    return timestamp.strftime("%d-%m-%Y %H:%M:%S")

def store_tokens(data):
    pattern = r'(DAC_\d{4}/\d{2}_\d{2}_\d{2}/\w{1,8}/\d{2}_\d{2}_\d{2}\.txt):(\d+,-?\d+,\d+),'
    match = re.match(pattern, data)
    if match:
        full_file_path, data = match.groups()
        store_data(full_file_path, data)
        return 1
    return 0


def store_data(full_file_path, data):
    
    file_path,file_name = os.path.split(full_file_path)
    Path(file_path).mkdir(mode=777, parents=True, exist_ok=True)
    with open(full_file_path, "a") as file:
        file.write(data+'\n')


def separate_tokens(data):
    pattern = r'(DAC_\d{4})/(\d{2}_\d{2}_\d{2})/(\w{1,8})/(\d{2}_\d{2}_\d{2}\.txt):(\d+),(-?\d+),(\d+),'
    match = re.match(pattern, data)
    if match:
        dir1, dir2, dir3, filename, id_value, value, timestamp = match.groups()
        print("\n")
        print("------------------------")
        print("Valid format:")
        print("Filename:", filename)
        print("Dir:", dir1+"/"+dir2+"/"+dir3+"/")
        print("ID:", id_value)
        print("Value:", value)
        print("Timestamp:", timestamp)
        print("Timestamp:",convert_timestamp(int(timestamp)))
        print("------------------------")
        print("\n")
        return (dir1, dir2, dir3, filename, id_value, value, timestamp)

    return (0, 0, 0, 0, 0, 0, 0)        

def evaluate_format(data):
    dir1, dir2, dir3, filename, id_value, value, timestamp = separate_tokens(data)
    if(dir1):
        return "ESP_OK" 
    else:
        print("\n\nInvalid format. Saving for later:", data)
        
        return data if data != '' else "ESP_OK"

def save_data_to_file(data, file_path, filename):
    Path(file_path).mkdir(mode=777, parents=True, exist_ok=True)
    full_file_path = os.path.join(file_path, filename)
    with open(full_file_path, "a") as file:
        file.write(data)

stop_flag = threading.Event()


def threaded_client(sock,msg):
    
    sock.setblocking(0)
    max_idle_time = 30  # s
    last_data_timestamp = time.time()
    aux_timestamp = 0
    buffer = ''
    delimiter = '\n'
    while not stop_flag.is_set():
        try:
            data = sock.recv(1024).decode().replace('\0','')
            
           
            if not data:
                raise ValueError("no data")

            last_data_timestamp = time.time()

            buffer+=data

            while delimiter in buffer:
                message, _, buffer = buffer.partition(delimiter)
                if message == KEEPALIVE:
                    continue
                
                if(not store_tokens(message)):
                    print("error: ", message)
                  

        except (socket.error, ValueError):
            seconds_since_last_data = int(time.time() - last_data_timestamp)

           
            
            if (aux_timestamp != seconds_since_last_data):
                aux_timestamp = seconds_since_last_data
                print("\n")
                print(f"{msg[-5:]} - remaining idle time IP:{sock.getpeername()} = {max_idle_time - seconds_since_last_data}s")
                print("\n")
               
            
            if (seconds_since_last_data >= max_idle_time):
                print("Idle time expired!")
                break

    print(f"Closing socket {sock.getpeername()}")
    
    sock.close()

def tcp_socket_server(config, port, msg):
    server_socket = socket.socket()
    
    try:
        server_socket.bind((config, port))
    except socket.error as e:
        print(str(e))
        return
    
    print('TCP server started, waiting for a connection...')
    server_socket.listen(5)
    server_socket.setblocking(0)

    while not stop_flag.is_set():
        try:
            client_socket, address = server_socket.accept()
            print('Connected to:', address[0] + ':' + str(address[1]))
            t = threading.Thread(target=threaded_client, args=(client_socket, msg))
            t.start()
            print('Thread Number:', threading.active_count())
            break
        except:
            pass
    
    print("Closing TCP server socket...")
    server_socket.close()

def udp_socket_server(port):
    server_socket = socket.socket(socket.AF_INET, socket.SOCK_DGRAM)
    server_socket.setsockopt(socket.SOL_SOCKET, socket.SO_BROADCAST, 1)
    server_socket.bind(("", port))
    print("UDP server started and listening for broadcast messages.")
    server_socket.settimeout(5)

    server_ip = socket.gethostbyname(socket.gethostname())
    while not stop_flag.is_set():
        try:
            data, client_address = server_socket.recvfrom(1024)
            received_message = data.decode("utf-8")
            print(f"Received broadcast message: {received_message} from {client_address}")
            server_socket.sendto(server_ip.encode("utf-8"), client_address)
            t_tcp = threading.Thread(target=tcp_socket_server, args=(socket.gethostname(), port,received_message ))
            t_tcp.start()
        except Exception as ex:
            if(isinstance(ex,TimeoutError)):
                continue
            print(ex)
            server_socket.close()
            server_socket.bind(("", port))
            pass
      
    print("Closing UDP server socket...")
    server_socket.close()
def server():
    port = 23
    t_udp = threading.Thread(target=udp_socket_server, args=(port,))
    t_udp.start()

    while True:
        key = input("Press 'Q' to quit the server:")
        if key.upper() == 'Q':
            print("Quitting the server...")
            stop_flag.set()
            return
        
        
            

if __name__ == "__main__":
    server()
    sys.exit()
