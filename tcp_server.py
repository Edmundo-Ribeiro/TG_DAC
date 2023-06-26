import socket

def save_data_to_file(data, filename):
    # Specify the file path where you want to save the data
    file_path = filename
    with open(file_path, "a") as file:
        file.write(data)
    print(f"Data saved to file: {file_path}")

def tcp_server():
    # Specify the host and port for the server to listen on
    host = "192.168.1.130"
    port = 23

    # Create a TCP socket and bind it to the specified host and port
    server_socket = socket.socket(socket.AF_INET, socket.SOCK_STREAM)
    server_socket.bind((host, port))

    # Listen for incoming connections
    server_socket.listen(1)
    print("TCP server is listening for connections...")

    while True:
        # Accept a client connection
        client_socket, address = server_socket.accept()
        print(f"Connection established from: {address}")

        # # Receive data from the client
        # data = client_socket.recv(1024)
        # filename, filesize = data.decode().split(',')
        # print(filename, filesize)

        # client_socket.send("ok".encode())

        data = 'ok'
        while(data):
            # Save the received data to a file
            data = client_socket.recv(1024).decode()
            print(data)
            streams = data.split("\n")
            print(streams)
            for stream in streams:
                if(not len(stream)):
                    continue

                filename, content = stream.split(":")
                save_data_to_file(content+"\n", filename)

        # Close the client socket
        client_socket.close()

    # Close the server socket
    server_socket.close()

# Start the TCP server
tcp_server()
