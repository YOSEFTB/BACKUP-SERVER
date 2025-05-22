import socket
import struct
import backup_requests

#  This function takes a struct of data (the request), and sends it to the server. Also calls the function that receives the response
def sendrequest(id,message,HOST,PORT):
    try:
        with socket.socket(socket.AF_INET,socket.SOCK_STREAM) as s:
            s.connect((HOST,int(PORT)))
            s.send(message)
            header=receive_header(s,id)
    except (ConnectionRefusedError, socket.timeout) as e:
        print(f"Error: Could not connect to {HOST}:{PORT} - {e}")

def recv_exact(sock, size):
    # Receives exactly `size` bytes from the socket
    data = b""
    while len(data) < size:
        packet = sock.recv(size - len(data))
        if not packet:
            raise ConnectionError("Connection closed by server")
        data += packet
    return data

def receive_header(sock,id):   # Receives the response from server, and prints (or stores) the relevant information to the user
    version = struct.unpack("B", recv_exact(sock, 1))[0]  # 1 byte
    status = struct.unpack("<H", recv_exact(sock, 2))[0]  # 2 bytes (little-endian)
    header_data = {}
    sock.setblocking(False)
    try:
        if sock.recv(1, socket.MSG_PEEK):  # Peek to see if more data exists in the response
            filename_length = struct.unpack("<H", recv_exact(sock, 2))[0]  # 2 bytes
            filename = recv_exact(sock, filename_length).decode()
            header_data["filename"] = filename

        if sock.recv(3, socket.MSG_PEEK):  # Peek again, to see if there is additional info in the response
            file_length = struct.unpack("<I", recv_exact(sock, 4))[0]  # 4 bytes
            file_content = recv_exact(sock, file_length)  # File content
            header_data["file_content"] = file_content
    except BlockingIOError:
        pass
    sock.setblocking(True)


#  These are the messages that the client program presents to the user, depending on the status code

    if(status==210):  # Restore success
        print(f"Message from server, version {version}. For user {id}. Status- {status}. The file {filename} was restored.\
The file is {file_length} bytes long. It is now stored in the tmp file in this folder.")
        backup_requests.create_file(f"tmp.{backup_requests.get_file_extension(filename)}",file_content)

    if(status==211):   # Backups list returned
        print(f"Message from server, version {version}. For user {id}. Status- {status}. The list of files in backup server has \
been generated. The following files are available by the server:\n{file_content.decode()}")
        
    if(status==212):  # Delete/Backup success
        print(f"Message from server, version {version}. For user {id}. Status- {status}. The operation on file {filename} was successful.")

    if(status==1001):  # Error. File sent by client does not exist
        print(f"Message from server, version {version}. For user {id}. Status- {status}. The operation on file {filename} \
was unsuccessful, because file {filename} does not exist.")
        
    if(status==1002):  # Error. No files on server for this client, list cannot be sent
        print(f"Message from server, version {version}. For user {id}. Status- {status}. This user does not have any files on the server.")

    if(status==1003):  # Server side error
        print(f"Message from server, version {version}. For user {id}. Status- {status}. \
There was a problem with the server, and your request could not be processed.")

