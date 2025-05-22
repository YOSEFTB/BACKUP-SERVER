# BACKUP-SERVER
A service for backing up, retrieving, and deleting files.

## overview
This program implements a server (written in C++) designed to back up and restore users files, and a client to interact with the server (written in Python)

## features
The server supports the following operations- backing up files, restoring files, listing users files, and deleting files for user. Handles concurrency (and stores each users files in separate folders) using C++ threads. Communication is done using Boost.Asio on the server side, and Python sockets on the client side.
