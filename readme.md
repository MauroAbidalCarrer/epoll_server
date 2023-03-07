This is a small project to learn how to use the low level sys scalls needed for the 42 school project ``webserv``.  
The server's soul goal it to broadcast messages from one client to the others.  
If a client sends a message ``stop``, the sever quits.  
No memory leaks, no file descriptor leaks (at least I hope so ._.).  
Once the server is running, connect to it with the command ``telnet localhost 8080``.