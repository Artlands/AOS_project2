#!/usr/bin/python

import sys
import argparse
import threading
import json
import socket
import time
import random

from queue import Queue
from concurrent import futures

def get_args():
    """
    Get command line arguments from user
    """
    parser = argparse.ArgumentParser(
        description = 'Specify arguments to talk to the central indexing server'
    )
    parser.add_argument('-p', '--port',
                        type = int,
                        required = True,
                        action = 'store',
                        help = 'Server Port Number')
    args = parser.parse_args()
    return args

class ServerOperations(threading.Thread):
    def __init__(self, threadid, name, server_port):
        """
        Constructor used to initialize class object.
        @param threadid:     Thread ID.
        @param name:         Name of the thread
        """
        threading.Thread.__init__(self)
        self.threadID = threadid
        self.name = name
        self.server_port = server_port
        self.hash_table_ports_peers = {}
        self.hash_table_files = {}
        self.hash_table_peer_files = {}
        self.listener_queue = Queue()

    def server_listener(self):
        """
        Start to listen on port: 8080 for incoming connections.
        """
        try:
            server_socket = socket.socket(socket.AF_INET, socket.SOCK_STREAM)
            server_socket.setsockopt(socket.SOL_SOCKET, socket.SO_REUSEADDR, 1)
            server_host = socket.gethostname()
            server_socket.bind((server_host, self.server_port))
            server_socket.listen()
            while True:
                conn, addr = server_socket.accept()
                self.listener_queue.put((conn, addr))
        except Exception as e:
            print(f"Server Listener on port failed: {e}")
            sys.exit(1)

    def register(self, addr, files, peer_port):
        """
        Invoked by the peer to register itself with the central indexing server.
        @param addr:            Address of the incoming connection.
        @param files:           File list sent by the peer.
        @param peer_port:       Peer's server port.

        @return free_socket:    Socket port to be used as a peer server.
        """
        try:
            self.hash_table_ports_peers[peer_port] = addr[0]
            peer_id = addr[0] + ':' + str(peer_port)
            self.hash_table_peer_files[peer_id] = files
            for f in files:
                if f in self.hash_table_files:
                    self.hash_table_peer_files[f].append(peer_id)
                else:
                    self.hash_table_peer_files[f] = [peer_id]
            return True
        except Exception as e:
            print(f"Peer registration failure: {e}")
            return False

    def list(self):
        """
        List all files registered with the central indexing server.

        @return files_list:     List of files present in the server
        """
        try:
            files_list = self.hash_table_files.keys()
            return files_list
        except Exception as e:
            print(f"Listing files error: {e}")

    def search(self, file_name):
        """
        Search for a particular file.
        @param file_name:       File name to be searched
        @return:                List of peers associated with the file.
        """
        try:
            if file_name in self.hash_table_files:
                peer_list = self.hash_table_files[file_name]
            else:
                peer_list = []
            return peer_list
        except Exception as e:
            print(f"Searching files error: {e}")

    def run(self):
        """
        Start thread to carry out server operations.
        """
        try:
            print("Starting server listener...")
            listener_thread = threading.Thread(target = self.server_listener)
            listener_thread.setDaemon(True)
            listener_thread.start()
            while True:
                while not self.listener_queue.empty():
                    with futures.ThreadPoolExecutor(max_workers = 8) as executor:
                        conn, addr = self.listener_queue.get()
                        data_received = json.loads(conn.recv(1024))
                        print(f"Connected with {addr[0]} on port {addr[1]}, requesting for {data_received['command']}")

                        if data_received['command'] == 'register':
                            fut = executor.submit(self.register, addr,
                                                  data_received['files'],
                                                  data_received['peer_port'])
                            success = fut.result(timeout = None)
                            if success:
                                print(f"Registration successfull, Peer ID: {addr[0]} : {data_received['peer_port']}")
                                conn.send(json.dumps([addr[0], success]).encode())
                            else: #?
                                print(f"Registration unsuccessfull, Peer ID: {addr[0]} : {data_received['peer_port']}")
                                conn.send(json.dumps([addr[0], success]).encode())

                        elif data_received['command'] == 'list':
                            fut = executor.submit(self.list)
                            file_list = fut.result(timeout = None)
                            print(f"File list generated {file_list}")
                            conn.send(json.dumps(file_list).encode())

                        elif data_received['command'] == 'search':
                            fut = executor.submit(self.search, data_received['file_name'])
                            peer_list = fut.result(timeout = None)
                            print(f"Peer list generated {peer_list}")
                            conn.send(json.dumps(peer_list).encode())

                        print(f"Hash table: files || {self.hash_table_files}")
                        print(f"Hash table: Port-Peers || {self.hash_table_ports_peers}")
                        print(f"Hash table: Peer-Files || {self.hash_table_peer_files}")
                        conn.close()
        except Exception as e:
            print(f"Server error; {e}")
            sys.exit(1)

if __name__ == "__main__":
    """
    Main method to start deamon threads for listener and opeartions.
    """
    try:
        args = get_args()
        print(f"Starting Central Indexing Server...")
        print(f"Starting Server operations thread...")
        operations_thread = ServerOperations(1, "ServerOperations", args.port)
        operations_thread.run()
    except Exception as e:
        print(e)
        sys.exit(1)
    except (KeyboardInterrupt, SystemExit):
        print("Central Indexing Server shutting down...")
        time.sleep(1)
        sys.exit(1)
