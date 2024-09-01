import socket

def main():
    server_ip = input("Enter server IP address: ")
    server_port = 8080
    buffer_size = 4096

    username = input("Enter username: ")
    password = input("Enter password: ")

    source_dir = input("Enter source directory path: ")
    destination = input("Enter destination file path (including filename, without extension): ")
    compression_level = input("Enter compression level (0-9): ")
    zip_password = input("Enter zip password (or leave blank for no password): ")

    # Create a socket
    client_socket = socket.socket(socket.AF_INET, socket.SOCK_STREAM)

    try:
        # Connect to the server
        client_socket.connect((server_ip, server_port))
        print("Connected to server")

        # Send username and password for authentication
        auth_message = f"{username} {password}"
        client_socket.sendall(auth_message.encode())

        # Receive the authentication response from the server
        auth_response = client_socket.recv(buffer_size).decode()
        print(f"Server: {auth_response}")

        if "Authentication failed" in auth_response:
            print("Authentication failed. Exiting.")
            return

        # Prepare the compression request message
        compression_message = f"{source_dir} {destination} {compression_level} {zip_password}"
        client_socket.sendall(compression_message.encode())

        # Receive the response from the server
        response = client_socket.recv(buffer_size).decode()
        print(f"Server: {response}")

    except Exception as e:
        print(f"Error: {e}")
    finally:
        # Close the socket
        client_socket.close()

if __name__ == "__main__":
    main()
