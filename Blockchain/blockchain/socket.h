#include <arpa/inet.h>
#include <netinet/in.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/socket.h>
#include <unistd.h>
#include <vector>
#include <string>
#include <iostream>
#include <poll.h>
#include <sys/ioctl.h>
#include <sys/time.h>

struct Message
{
    Message()
    {
        client = -1;
        content = "";
        gettimeofday(&this->timestamp, NULL);
    }
    Message(int client, const std::string &content) : Message()
    {
        this->client = client;
        this->content = content;
    };
    Message(int client, const std::string &content, struct timeval timestamp) : Message(client, content)
    {
        this->timestamp = timestamp;
    };
    int client;
    std::string content;
    struct timeval timestamp;
};

class SocketServer
{
    public:
        SocketServer(int port)
        {
            this->port = port;
            this->server = -1;
            this->addrlen = sizeof(this->address);
        }
        ~SocketServer()
        {
            if (this->server >= 0)
            {
                for (int i = 0; i < this->clients.size(); i++)
                    close(this->clients[i]);

                shutdown(this->server, SHUT_RDWR);
            }
        }
    
        void init()
        {
            int opt = 1;
         
            this->server = socket(AF_INET, SOCK_STREAM, 0);
            if (this->server < 0) 
            {
                std::cerr<<"Could not create socket"<<std::endl;
                exit(EXIT_FAILURE);
            }
         
            if (setsockopt(this->server, SOL_SOCKET, SO_REUSEADDR | SO_REUSEPORT, &opt, sizeof(opt))) {
                std::cerr<<"Could not set socket options"<<std::endl;
                exit(EXIT_FAILURE);
            }
            address.sin_family = AF_INET;
            address.sin_addr.s_addr = INADDR_ANY;
            address.sin_port = htons(this->port);
         
            if (bind(this->server, (struct sockaddr*)&this->address, sizeof(this->address)) < 0) {
                std::cerr<<"Could not bind socket"<<std::endl;
                exit(EXIT_FAILURE);
            }
            if (listen(this->server, 3) < 0) {
                std::cerr<<"Error when listening"<<std::endl;
                exit(EXIT_FAILURE);
            }
        }
        
        void acceptClients(int numClients)
        {
            for (int i = 0; i < numClients; i++)
            {
                int client;
                if ((client = accept(this->server, (struct sockaddr*)&this->address,(socklen_t*)&this->addrlen)) < 0) 
                {
                    std::cerr<<"Error accepting client"<<std::endl;
                    exit(EXIT_FAILURE);
                }
                this->clients.push_back(client);
            }
        }
        
        Message sendMessage(const std::string &message, int client)
        {
            if(this->isConnected() && this->isClientConnected(client))
            {
                send(this->clients[client], message.c_str(), message.length(), 0);
            }
            
            return Message(client, message);
        }
        
        std::vector<Message> broadcastMessage(const std::string &message)
        {
            std::vector<Message> messages;
            for (int i=0;i<this->clients.size();i++)
            {
                messages.push_back(this->sendMessage(message, i));
            }
            
            return messages;
        }
        
        std::vector<Message> receiveMessageFromAll()
        {
            std::vector<Message> messages;
            struct pollfd poll_set[this->clients.size()];
            int numfds = this->clients.size();
            
            memset(poll_set, '\0', sizeof(poll_set));
            for(int i=0;i<this->clients.size();i++)
            {
                poll_set[i].fd = this->clients[i];
                poll_set[i].events = POLLIN;
            }
            
            while(messages.size() < this->clients.size())
            {
                poll(poll_set, numfds, -1);
                for(int i=0;i<this->clients.size();i++)
                {
                    if (poll_set[i].revents & POLLIN)
                    {
                        int nread;
                        ioctl(poll_set[i].fd, FIONREAD, &nread);
                        if(nread > 0)
                        {
                            messages.push_back(this->receiveMessageFrom(i));
                            poll_set[i] = {-1, 0, 0};
                        }
                        
                    }
                }
            }
            
            return messages;
        }
        
        Message receiveMessageFrom(int client)
        {
            if(this->isConnected() && this->isClientConnected(client))
            {
                char* buffer = new char[1024];
                int nbytes = 0;
                std::string message = "";
                do{
                    int nbytes = read(this->clients[client], buffer, 1023);
                    buffer[nbytes] = '\0';
                    message += buffer;
                }while(nbytes > 0);
                
                return Message(client, message);
            }
            return Message(client, "");
        }
        
        Message receiveMessageFromAny(int &client)
        {
            struct pollfd poll_set[this->clients.size()];
            int numfds = this->clients.size();
            
            memset(poll_set, '\0', sizeof(poll_set));
            for(int i=0;i<this->clients.size();i++)
            {
                poll_set[i].fd = this->clients[i];
                poll_set[i].events = POLLIN;
            }
            
            while(true)
            {
                poll(poll_set, numfds, -1);
                for(int i=0;i<this->clients.size();i++)
                {
                    if (poll_set[i].revents & POLLIN)
                    {
                        int nread;
                        ioctl(poll_set[i].fd, FIONREAD, &nread);
                        client = i;
                        return this->receiveMessageFrom(i);
                    }
                }
            }
            
            return Message(-1, "");
        }
        
        bool isConnected() const
        {
            return this->server >= 0;
        }
        bool isClientConnected(int client) const
        {
            return client < this->clients.size() && this->clients[client] >= 0;
        }
    
    private:
        int server;
        int port;
        std::vector<int> clients;
        struct sockaddr_in address;
        int addrlen;
    
};

class SocketClient
{
    public:
        SocketClient()
        {
            this->client = -1;
            this->sock = -1;
        }
        ~SocketClient()
        {
            if(this->client >= 0)
            {
                close(this->client);
            }
        }
        void connectTo(std::string ip, int port)
        {
            if ((this->sock = socket(AF_INET, SOCK_STREAM, 0)) < 0) {
                std::cerr<<"Could not create socket"<<std::endl;
                exit(0);
            }
         
            this->serverAddress.sin_family = AF_INET;
            this->serverAddress.sin_port = htons(port);
         
            if (inet_pton(AF_INET, ip.c_str(), &this->serverAddress.sin_addr) <= 0) {
                std::cerr<<"Invalid address/ Address not supported"<<std::endl;
                exit(0);
            }
         
            if ((this->client = connect(sock, (struct sockaddr*)&this->serverAddress, sizeof(this->serverAddress))) < 0) 
            {
                std::cerr<<"Could not connect to server"<<std::endl;
                exit(0);
            }
        }
        
        void sendMessage(std::string message)
        {
            send(this->sock, message.c_str(), message.length(), 0);
        }
        
        std::string receiveMessage()
        {
            char* buffer = new char[1024];
            int nbytes = 0;
            std::string message = "";
            do{
                int nbytes = read(this->sock, buffer, 1023);
                buffer[nbytes] = '\0';
                message += buffer;
            }while(nbytes > 0);
            
            return message;
        }
        
        unsigned char* receiveBytes()
        {
            unsigned char* buffer = new unsigned char[1024];
            int nbytes = read(this->sock, buffer, 1023);
            buffer[nbytes] = '\0';
            
            return buffer;
        }
    private:
        int client;
        int sock;
        struct sockaddr_in serverAddress;
};
