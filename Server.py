#!/usr/bin/python

import socket
import time
import threading
import json
import random
import argparse
import sys
from Queue import Queue
# from concurrent import futures

def get_args():
    """
    Get command line arguments from user
    """
    parser = argparse.ArgumentParser(
        description = 'Specify arguments to talk to the central indexing server'
    )
    parser.add_argument('p', '--port',
                        type = int,
                        required = True
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
            server_socket = socket.socket(socket.AF_INET, socket.SOCK_STERAM)
            server_socket.setsockopt(socket.SOL_SOCKET, socket.SO_REUSEADDR, 1)
            server_host = socket.gethostname()
            server_socket.bind((server_host, self.server_port)
            server_socket.listen(10)
            while True:
                conn, addr = server_socket.accept()
                self.listener_queue.put((conn, addr))
        except Exception as e:
            print(f"Server Listener on port failed: {e}")
            sys.exit()

    def register(self, addr, files, peer_port):
        """
        Invoked by the peer to register itself with the central indexing server.
        @param addr:            Address f the incoming connection.
        @param files:           File list sent by the peer.
        @param peer_port:       Peer's server port.

        @return free_socket:    Socket prot to be used as a peer server.
        """
        try:
            self.hash_table_ports_peers[peer_port] = addr[0]
            peer_id = addr[0] + ':' + str(peer_port)
            self.hash_table_peer_files[peer_id] = files
            for f in files:
                if self.hash_table_files.has_key(f):
                    self.hash_table_peer_files[f].append(peer_id)
                else:
                    self.hash_table_peer_files[f] = [peer_id]
            return True
        except Exception as e:
            print(f"Peer registration failure: {e}")
            return False

    def update(self, peer_update):
        """
        Invoked by the peer to update the files in the central indexing server.
        Peer invokes this method to add or remove file.
        @param peer_update:     Peer File update details
        """
        try:
            if peer_update['task'] == 'add':
                for f in peer_update['files']:
                    self.hash_table_peer_files[peer_update['peer_id']].append(f)
                    if self.hash_table_files.has_key(f):
                        self.hash_table_files[f].append(str(peer_update['peer_id']))
                    else:
                        self.hash_table_files[f] = [str(peer_update['peer_id'])]
            if peer_update['task'] == 'remove':
                for f in peer_update['files']:
                    self.hash_table_peer_files[peer_update['peer_id']].remove(f)
                    if self.hash_table_files.has_key(f):
                        for peer_id in self.hash_table_files[f]:
                            if peer_id == peer_update['peer_id']:
                                self.hash_table_files[f].remove(peer_id)
                                if len(self.hash_table_files[f]) == 0:
                                    self.hash_table_files.pop(f, None)
            return True
        except Exception as e:
            print(f"Peer file update failure: {e}")
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
                if self.hash_table_files.has_key(file_name):
                    peer_list = self.hash_table_files[file_name]
                else:
                    peer_list = []
                return peer_list
            except Exception as e:
                print(f"Searching files error: {e}")
