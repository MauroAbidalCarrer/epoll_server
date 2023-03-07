#ifndef SERVER
# define SERVER
# include <iostream>
# include <string>
# include <vector>
# include <sys/epoll.h>
# include <sys/fcntl.h>
# include <stdio.h>
# include <cerrno>
# include <stdlib.h>
# include <unistd.h>
# include <sys/types.h>
# include <sys/socket.h>
# include <string>
# include <string.h>
# include <netinet/in.h>
# include <algorithm>
# include <iterator>
# include <list>
# include <cstdlib>

#define CONNEXION_PORT 8080
#define MAX_QUEUE_SIZE 10
#define CLIENT_MSG_BUFFER_SIZE 1000
#define MAX_EPOLL_EVENTS_TO_HANDLE_AT_ONCE 64

typedef struct sockaddr_in sockaddr_in_t;

void    ft_memset(void *b, int c, size_t len)
{
    unsigned long   i;
    char            *ptr;

    ptr = (char *)b;
    i = 0;
    while (i < len)
        ptr[i++] = (char)c;
}


/*
    Server that recieves client messages and sends them back to the other clients.
    Uses epoll.
*/
class Server
{
    private:
    //fields
    std::vector<int> client_messages_fds;   //vector of opened file descriptors needed to close them once Server gets deleted
    int listen_socket_fd, epoll_fd;         //listen socket and epoll file descriptors have them own variables even though they could be stored in client_messages_fds for easier access
    epoll_event *events_buffer;
    char client_msg_buffer[CLIENT_MSG_BUFFER_SIZE + 1];
    //mested classes
    class ClientMessageStopException {};

    //methods
    //Creates a socket, binds it, listens to it and returns the fd of the socket.
    void setup_connexion_queue(int port)
    {
        //create socket
        if ((listen_socket_fd = socket(AF_INET, SOCK_STREAM, 0)) == -1)
            throw ServerException("failed to create the connexion socket: ");
        //set socket to non blocking
        if (fcntl(listen_socket_fd, F_SETFL, O_NONBLOCK) == -1)
            throw ServerException("failed fcntl call on the connexion socket: ");
        //set sock option SO_REUSEADDR to prevent TCP related error 'Adress already in use' when restartingint
        int dump = 1;
        if (setsockopt(listen_socket_fd, SOL_SOCKET, SO_REUSEADDR, &dump, sizeof(int)) == -1)
            throw ServerException("failed to set connexion_socket SO_REUSEADDR option : ");
        //binding of socket to port
        sockaddr_in_t sock_addr_for_connexion;
        //Using ft_memset before setting the variables we care about abeginllows us to set all the variable we don't care about to zero in one line.
        ft_memset(&sock_addr_for_connexion, 0, sizeof(sockaddr_in_t));
        sock_addr_for_connexion.sin_family = AF_INET;                       //define adress family, in this case, IPv4
        sock_addr_for_connexion.sin_addr.s_addr = htonl(INADDR_ANY);        //define IP adress, in this case, localhost
        sock_addr_for_connexion.sin_port = htons(port);                     //define port
        if (bind(listen_socket_fd, (const struct sockaddr *)&sock_addr_for_connexion, sizeof(sockaddr_in_t)) == -1)
            throw ServerException("failed to bind the queue connexion to port: ");
        //start listening for clients connexion requests
        if (listen(listen_socket_fd, MAX_QUEUE_SIZE) == -1)
            throw ServerException("failed to listen the connexion socket: ");
        printf("listening...\n");
    }
    void create_epoll_instance()
    {
        if ((epoll_fd = epoll_create1(0)) == -1)
            throw ServerException("failed to create epoll instance: ");
        epoll_event listen_epoll_event;
        listen_epoll_event.events = EPOLLIN;
        listen_epoll_event.data.fd = listen_socket_fd;
        if (epoll_ctl(epoll_fd, EPOLL_CTL_ADD, listen_socket_fd, &listen_epoll_event) == -1)
            throw ServerException("epoll_ctl raised an error: ");
        std::cout << "Created epoll instance." << std::endl;
    }
    //Accepts new connexion, adds the new socket file descriptor to the open_fds vector and the epoll instance interset list.
    void accept_new_connexion()
    {
        //accept new socket_fd
        epoll_event data_exchage_epoll_event;
        if ((data_exchage_epoll_event.data.fd = accept(listen_socket_fd, NULL, NULL)) == -1)
            throw ServerException("failed to accept new client connexion, error: ");
        //set new element values
        data_exchage_epoll_event.events = EPOLLIN;
        //add new epoll_event to the epoll instance interest list
        if (epoll_ctl(epoll_fd, EPOLL_CTL_ADD, data_exchage_epoll_event.data.fd, &data_exchage_epoll_event) == -1)
            throw ServerException("failed to add new client connexion to epoll instance, error: ");
        client_messages_fds.push_back(data_exchage_epoll_event.data.fd);
        //debugging
        std::cout << "Accepted new connexion, data_exchage_epoll_event.data.fd = " << data_exchage_epoll_event.data.fd << std::endl;
    }
    void close_connexion(int client_message_socket_fd)
    {
        //make sure to delete socket from interest list before closing the scoket
        if (epoll_ctl(epoll_fd, EPOLL_CTL_DEL, client_message_socket_fd, NULL) == -1)
            throw ServerException("failed to delete client connexion from epoll instance, error: ");
        if (close(client_message_socket_fd) == -1)
            throw ServerException("failed to close client_message_socket_fd: ");
        std::vector<int>::iterator iterator_in_opened_fds = find(client_messages_fds.begin(), client_messages_fds.end(), client_message_socket_fd);
        client_messages_fds.erase(iterator_in_opened_fds);
        std::cout << "Closed connections with client." << std::endl;
    }
    int my_strcmp(const char *str1, const char *str2)
    {
        int i = 0;
        while (str2[i] && str1[i] && str1[i] == str2[i])
            i++;
        return str1[i] - str2[i];
    }
    void handle_client_message(int client_message_socket_fd)
    {
        //check for cariage return, not just \n
        if (!my_strcmp("stop\r\n", client_msg_buffer))
            throw ClientMessageStopException();
        std::cout << "message from client: " << client_msg_buffer;
        for (size_t i = 0; i < client_messages_fds.size(); i++)
            if (client_messages_fds[i] != client_message_socket_fd)
                if (send(client_messages_fds[i], client_msg_buffer, strlen(client_msg_buffer), 0) == -1)
                    throw ServerException("failed to send client message to other clients: ");
    }
    void read_client_message(int client_message_socket_fd)
    {
        ssize_t read_bytes;
        read_bytes = recv(client_message_socket_fd, client_msg_buffer, CLIENT_MSG_BUFFER_SIZE, 0);
        if (read_bytes == -1 && EAGAIN != errno && EWOULDBLOCK != errno)
            throw ServerException("failed to read client message: ");
        client_msg_buffer[read_bytes] = 0;
        //if connexion was closed by client, close socket_fd, delete from epoll instance's interest list and erase it from client_messages_fds
        if (read_bytes == 0)
            close_connexion(client_message_socket_fd);
        else//read_bytes is neither -1 nor 0, which means that there is an actual message to broadcast.
            handle_client_message(client_message_socket_fd);
    }
    void wait_for_events()
    {
        int nb_events = epoll_wait(epoll_fd, events_buffer, MAX_EPOLL_EVENTS_TO_HANDLE_AT_ONCE, -1);
        if (nb_events == -1)
            throw ServerException("failed to wait for epoll events: ");
        for (int i = 0; i < nb_events; i++)
        {
            int fd = events_buffer[i].data.fd;
            if (fd == listen_socket_fd)
                accept_new_connexion();
            else
                read_client_message(events_buffer[i].data.fd);
        }
    }

    public:
    Server() : client_messages_fds(), listen_socket_fd(-1), epoll_fd(-1), events_buffer(new epoll_event[MAX_EPOLL_EVENTS_TO_HANDLE_AT_ONCE]) { }
    void Run()
    {
        setup_connexion_queue(CONNEXION_PORT);
        create_epoll_instance();
        try
        {
            while (true)
                wait_for_events();
        }
        catch(const ClientMessageStopException &e)
        {
            std::cout << "Client commanded server to stop... so the server will stop. This is not a very secure server ._." << std::endl;
        }
    }
    ~Server()
    {
        if (epoll_fd != -1)
            close(epoll_fd);
        if (listen_socket_fd != -1)
            close(listen_socket_fd);
        for (size_t i = 0; i < client_messages_fds.size(); i++)
            close(client_messages_fds[i]);
        delete[] events_buffer;
    }

    class ServerException : public std::exception
    {
        private:
        std::string errormsg;

        public:
        ServerException(std::string errormsg_suffix)
        {
            errormsg = errormsg_suffix + std::string(strerror(errno));
        }
        ~ServerException() throw () {}

        const char* what() const throw()
        {
            return errormsg.c_str();
        }
    };
};
#endif