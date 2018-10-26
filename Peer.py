#!/usr/bin/python

import os
import sys
import argparse
import threading
import json
import socket
import time

from queue import Queue
from concurrent import futures

SHARED_DIR = './shared-files'

def get_args():
    """
    Get command line arguments from user
    """
    parser = argparse.ArgumentParser(
        description = 'Specify arguments to talk to the central indexing server'
    )
    parser.add_argument('-s', '--server',
                        type = int,
                        required = True,
                        action = 'store',
                        help = 'Server Port Number')
    args = parser.parse_args()
    return args

class PeerOperations(threading.Thread):
    def __init__(self, threadid, name, p):
        """
        Constructor used to initialize class object.
        @param threadid:     Thread ID.
        @param name:         Name of the thread
        @param p:            Class peer object
        """
        threading.Thread.__init__(self)
        self.threadID = threadid
        self.name = name
        self.peer = p
        self.peer_server_listener_queue = Queue()

    def peer_server_listener(self):
        """
        Peer server listen on port assigned by central indexing server
        """
        try:
            peer_server_socket = socket.socket(socket.AF_INET, socket.SOCK_STREAM)
            peer_server_socket.setsockopt(socket.SOL_SOCKET, socket.SO_REUSEADDR, 1)
            peer_server_host = socket.gethostname()
            peer_server_port = self.peer.hosting_port
            peer_server_socket.bind(peer_server_host, peer_server_port)
            peer_server_socket.listen()
            while True:
                conn, addr = peer_server_socket.accept()
                self.peer_server_listener_queue.put((conn, addr))
        except Exception as e:
            print(f"Peer server Listener on port failed: {e}")
            sys.exit(1)

    def peer_server_upload(self, conn, addr, data_received):
        """
        Transfer file between peers
        @param conn:            Connection object
        @param data_received    Received data containing file name
        """
        try:
            f = open(SHARED_DIR + '/' + data_received['file_name'], 'rb')
            data = f.read()
            f.close()
            conn.sendall(data)
            conn.close()
        except Exception as e:
            print(f"File upload error, {e}")

    def peer_server_host(self):
        """
        Process client download request
        """
        try:
            while True:
                while not self.peer_server_listener_queue.empty():
                    with futures.ThreadPoolExecutor(max_workers = 8) as executor:
                        conn, addr = self.peer_server_listener_queue.get()
                        data_received = json.loads(conn.recv(1024))

                        if data_received['command'] == 'obtain':
                            fut = executor.submit(self.peer_server_upload, conn, data_received)
        except Exception as e:
            print(f"Peer server hosting error, {e}")

    def peer_server(self):
        """
        Start peer server listener and peer server download deamon thread.
        """
        try:
            listener_thread = threading.Thread(target = self.peer_server_listener)
            listener_thread.setDaemon(True)
            listener_thread.start()

            opeartions_thread = threading.Thread(target = self.peer_server_host)
            operations_thread.setDaemon(True)
            operations_thread.start()

            threads = []
            threads.append(listener_thread)
            threads.append(operations_thread)

            for t in threads:
                t.join()

        except Exception as e:
            print(f"Peer server error, {e}")
            sys.exit(1)

    def run(self):
        """
        Deamon thread for peer server
        """
        if self.name == "PeerServer":
            self.peer_server()

class Peer():
    def __init__(self, server_port):
        """
        Constructor used to initialize class object.
        """
        self.peer_hostname = socket.gethostname()
        self.server_port = server_port
        self.file_list = []

    def get_file_list(self):
        """
        Obtain file list in shared dir.
        """
        try:
            for filename in os.listdir(SHARED_DIR):
                self.file_list.append(filename)
        except Exception as e:
            print(f"Error: retriving file list, {e}")

    def get_free_socket(self):
        """
        Obtain free socket port for the registring peer where the peer
        can use this port for hosting file as a server.
        @return free_socket:           Socket prot to be used as peer server
        """
        try:
            peer_socket = socket.socket(socket.AF_INET, socket.SOCK_STREAM)
            peer_socket.setsockopt(socket.SOL_SOCKET, socket.SO_REUSEADDR, 1)
            peer_socket.bind(('', 0))
            free_socket = peer_socket.getsockname()[1]
            peer_socket.close()
            return free_socket
        except Exception as e:
            print(f"Obtaining free sockets failed: {e}")
            sys.exit(1)

    def register_peer(self):
        """
        Registring peer with the central indexing server
        """
        try:
            self.get_file_list()
            free_socket = self.get_free_socket()
            print(f"Registring peer with server...")

            peer_to_server_socket = socket.socket(socket.AF_INET, socket.SOCK_STREAM)
            peer_to_server_socket.setsockopt(socket.SOL_SOCKET, socket.SO_REUSEADDR, 1)
            peer_to_server_socket.connect((self.peer_hostname, self.server_port))

            cmd_issue = {
                'command': 'register',
                'peer_port': free_socket,
                'files': self.file_list
            }

            peer_to_server_socket.sendall(json.dumps(cmd_issue).encode())
            rcv_data = json.loads(peer_to_server_socket.recv(1024))
            peer_to_server_socket.close()
            if rcv_data[1]:
                self.hosting_port = free_socket
                self.peer_id = rcv_data[0] + ':' + str(free_socket)
                print(f"Registration successfull, Peer ID: {rcv_data[0]}:{free_socket}")
            else:
                print(f"Registration unsuccessfull, Peer ID: {rcv_data[0]}:{free_socket}")
                sys.exit(1)
        except Exception as e:
            print(f"Registering peer error, {e}")
            sys.exit(1)

    def list_files_index_server(self):
        """
        Obtain files present in central indexing server
        """
        try:
            peer_to_server_socket = socket.socket(socket.AF_INET, socket.SOCK_STREAM)
            peer_to_server_socket.setsockopt(socket.SOL_SOCKET, socket.SO_REUSEADDR, 1)
            peer_to_server_socket.connect(self.peer_hostname, self.server_port)

            cmd_issue = {
                'command': 'list'
            }
            peer_to_server_socket.sendall(json.dumps(cmd_issue).encode())
            rcv_data = json.loads(peer_to_server_socket.recv(1024))
            peer_to_server_socket.close()
            print(f"File list in the central indexing server")
            # for f in rcv_data:
            #     print(f)
        except Exception as e:
            print(f"Listing files from central indexing server error, {e}")

    def search_file(self, file_name):
        """
        Search for a file in central indexing server
        @param file_name:           File name to be searched
        """
        try:
            peer_to_server_socket = socket.socket(socket.AF_INET, socket.SOCK_STREAM)
            peer_to_server_socket.setsockopt(socket.SOL_SOCKET, socket.SO_REUSEADDR, 1)
            peer_to_server_socket.connect(self.peer_hostname, self.server_port)

            cmd_issue = {
                'command': 'search',
                'file_name': file_name
            }
            peer_to_server_socket.sendall(json.dumps(cmd_issue).encode())
            rcv_data = json.loads(peer_to_server_socket.recv(1024))
            if len(rcv_data) == 0:
                print(f"File Not Found")
            else:
                print(f"File present in below following peers:")
                for peer in rcv_data:
                    if peer == self.peer_id:
                        print(f"File present locally, peer ID: {peer}")
                    else:
                        print(f"Peer ID: {peer}")
            peer_to_server_socket.close()
        except Exception as e:
            print(f"Search File error, {e}")

    def obtain(self, file_name, peer_request_id):
        """
        Download file from another peer

        @param file_name:           File name to be downloaded
        @param peer_request_id:     Peer ID to be downloaded
        """
        try:
            peer_request_addr, peer_request_port = peer_request_id.split(':')
            peer_request_socket = socket.socket(socket.AF_INET, socket.SOCK_STREAM)
            peer_request_socket.setsockopt(socket.SOL_SOCKET, socket.SO_REUSEADDR, 1)
            peer_request_socket.connect(sock.gethostname(), int(peer_request_port))

            cmd_issue = {
                'command': 'obtain',
                'file_name': file_name
            }
            peer_request_socket.sendall(json.dumps(cmd_issue).encode())
            rcv_data = peer_request_socket.recv(1024000)
            f = open(SHARED_DIR + '/' + file_name, 'wb')
            f.write(rcv_data)
            f.close()
            peer_request_socket.close()
            print(f"File download successfully")
        except Exception as e:
            print(f"Obtain file error, {e}")

if __name__ == '__main__':
    """
    Main method starting deamon threads and peer operations.
    """
    try:
        args = get_args()
        print(f"Starting peer...")
        # create a instance of Peer, use argument specified by user to initialize
        p = Peer(args.server)

        print(f"Starting peer server deamon thread...")
        server_thread = PeerOperations(1, 'PeerServer', p)
        server_thread.setDaemon(True)
        server_thread.start()

        while True:
            print("-" * 80)
            print("register: Register the peer on the central indexing server")
            print("list: List all files in central indexing server")
            print("search: Search for File")
            print("get: Get file from peer")
            print("exit: Exit")
            print("-" * 80)
            ops = input("Enter choice: ")

            if ops == "register":
                p.register_peer()
            elif ops == "list":
                p.list_files_index_server()
            elif ops == "search":
                print("Enter file name: ")
                file_name = raw_input()
                p.search_file(file_name)
            elif ops == "get":
                print("Enter file name: ")
                file_name = raw_input()
                print("Enter Peer ID: ")
                peer_request_id = raw_input()
                p.obtain(file_name, peer_request_id)
            elif ops == "exit":
                break
            else:
                print("Invalid choice...")
                continue
    except Exception as e:
        print(e)
        sys.exit(1)
    except (KeyboardInterrupt, SystemExit):
        print("Peer shutting down...")
        time.sleep(1)
        sys.exit(1)
