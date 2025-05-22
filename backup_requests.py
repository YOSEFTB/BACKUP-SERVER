import random
import struct
import socket
import sys
import utils

#  This program implements a client in python. The client obtains a list of files from a backups file, the servers address 
#  and port from a server_info file. The client then executes a series of requests regarding saving, deleting, or backing up files, 
#  or just getting a list of the backups stored in the server.
#  The client packs the relevant data for the request into a struct (in little endian format)and sends the binary data to the cpp server.
#  The client receives a response from the server and displays the relevant message to the user, depending on the response.
#  The client will store a file in the filespace for the user, when is relevant (like when requesting a restore).
#  After the client sends the requests and processes the responses, it exits and terminates the connection.


def main(): 
    random_4byte =random.randint(1000,4294967295)
    HOST,PORT=get_server_info()
    version=127
    list_of_backups=get_backups()
    execute_series_of_requests(random_4byte,version,HOST,PORT,list_of_backups)

    
def execute_series_of_requests(random_4byte,version,HOST,PORT,backup_list):   #executes a series of requests from the server
    first_request(random_4byte,version,202,HOST,PORT)
    second_request(random_4byte,version,100,HOST,PORT,backup_list[0])
    third_request(random_4byte,version,100,HOST,PORT,backup_list[1])
    fourth_request(random_4byte,version,202,HOST,PORT)
    fifth_request(random_4byte,version,200,HOST,PORT,backup_list[0])
    sixth_request(random_4byte,version,201,HOST,PORT,backup_list[0])
    seventh_request(random_4byte,version,200,HOST,PORT,backup_list[0])

def get_server_info():    #obtains the info about server address and port
    with open('server.info', 'r') as file:
        content = file.read()
        h,p=content.split(":")
        return h,p
    
def get_backups():   #obtains list of files from the backup folder
    backups_list=list()
    try:
        with open('backup.info', 'r') as file:
            for line in file:
                if(line.strip()==''):
                    continue
                backups_list.append(line.strip())
    except FileNotFoundError:
        print("Error: 'backup.info' file not found. Please attach a backup file.\n")
        sys.exit()
    return backups_list

def create_file(filename: str, content: bytes):  #creates folder to store the file the server returns
    with open(filename, "wb") as file: 
        file.write(content)

def get_file_extension(filename: str):
    return filename.split('.')[-1] 

def first_request(id,vers,op,host,port):  #obtains list of files in backup from the server
    get_backups_from_server(id,vers,op,host,port)

def second_request(id,vers,op,host,port,first_file_in_backup):   #save the first file in backup.info
    save(id,vers,op,host,port,first_file_in_backup)

def third_request(id,vers,op,host,port,second_file_in_backup):  #save the second file in backup.info 
    save(id,vers,op,host,port,second_file_in_backup)

def fourth_request(id,vers,op,host,port):   # get list of files from server
    get_backups_from_server(id,vers,op,host,port)

def fifth_request(random_4byte,version,op,HOST,PORT,first_file_in_backup):  #restore first file in backup.info
    restore_and_delete(random_4byte,version,op,HOST,PORT,first_file_in_backup)

def sixth_request(random_4byte,version,op,HOST,PORT,first_file_in_backup):   #delete first file in backup.info
    restore_and_delete(random_4byte,version,op,HOST,PORT,first_file_in_backup)

def seventh_request(random_4byte,version,op,HOST,PORT,first_file_in_backup):   #restore first file in backup.info
    restore_and_delete(random_4byte,version,op,HOST,PORT,first_file_in_backup)    

def get_backups_from_server(id,vers,op,host,port):    # Structures the request for backups list
    user_id = id
    version = vers
    opcode = op
    format_string = '<IBB'
    packed_data = struct.pack(format_string, user_id, version, opcode)
    utils.sendrequest(id,packed_data,host,port)

def restore_and_delete(id,vers,op,HOST,PORT,file_name):  # Structures the request for deleting or restoring a file 
    if(op==200):
        opcode=200
    if(op==201):
        opcode=201
    name_length=len(file_name)
    format_string = f'<IBBH{name_length}s'
    packed_data=struct.pack(format_string,id,vers,opcode,name_length,file_name.encode('ascii'))
    utils.sendrequest(id,packed_data,HOST,PORT)

def save(id,vers,op,HOST,PORT,file_name):   # Structures the request for server backup
    name_length=len(file_name)
    content,file_length=get_content_and_length(file_name)
    format_string=f'<IBBH{name_length}sI{file_length}s'
    packed_data=struct.pack(format_string,id,vers,op,name_length,file_name.encode('ascii'),file_length,content)
    utils.sendrequest(id,packed_data,HOST,PORT)

def get_content_and_length(file_name):  #  Reads the file to be sent for backup 
    with open(file_name, "rb") as f:
        data = f.read()
    return data,len(data)

if __name__=='__main__':
    main()