import socket

def udp_broadcast_client():
    # Create a UDP socket
    client_socket = socket.socket(socket.AF_INET, socket.SOCK_DGRAM)

    # Enable broadcasting on the socket
    client_socket.setsockopt(socket.SOL_SOCKET, socket.SO_BROADCAST, 1)

    # Set the broadcast address and port
    broadcast_address = "<broadcast>"
    port = 23

    # Craft the broadcast message
    message = "Hello, server! Are you there?"

    # Send the broadcast message


    client_socket.settimeout(5)
    while True:
        client_socket.sendto(message.encode("utf-8"), (broadcast_address, port))
        print("Broadcast message sent. Waiting for responses...")
        try:
            # Set a timeout for receiving responses

            # Receive the response and server address
            response, server_address = client_socket.recvfrom(1024)

            # Decode the received response assuming it's a string
            received_response = response.decode("utf-8")

            # Check if the response is a valid IP address
            try:
                socket.inet_aton(received_response)
                print(f"Received response from server: {received_response}")

                # Establish a TCP connection with the server
                tcp_client_socket = socket.socket(socket.AF_INET, socket.SOCK_STREAM)
                tcp_client_socket.connect((received_response, 23))

                print("TCP connection established with the server.")
                # Perform further communication or actions with the server as needed

                # Close the TCP connection
                tcp_client_socket.close()
                break
            except socket.error:
                continue

        except socket.timeout:
            print("No responses received within the timeout.")
            continue

if __name__ == "__main__":
    udp_broadcast_client()
