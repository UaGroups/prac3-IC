#include <string>
#include <iostream>
#include "socket.h"
#include "block.h"
#include "miner.h"
#include <sys/time.h>
#include <map>
#include <algorithm>
#include <climits>
#define PORT 9999

long elapsedUsec(struct timeval begin, struct timeval end)
{
    return (end.tv_sec * 1000000 + end.tv_usec) - (begin.tv_sec * 1000000 + begin.tv_usec);
}

struct Client
{
    Client()
    {
        name = "";
        id = -1;
        reward = 0.0;
    }
    Client(int id, const std::string &name) : Client()
    {
        this->id = id;
        this->name = name;
    }
    std::string name;
    int id;
    float reward;
};

Block computeReward(std::map<int, Client> &clients, std::vector<Message> &messages, std::map<int, struct timeval> &begins, Miner &miner, float maxReward = 0.25f, long usecThresh = 100000)
{
    std::sort(messages.begin(), messages.end(), [&begins](const Message &a, const Message &b)
    {
        return elapsedUsec(begins[a.client], a.timestamp) < elapsedUsec(begins[b.client], b.timestamp);
    });
    
    long minTime = LONG_MAX;
    Block bestBlock;
    
    for (int i = 0; i < messages.size();i++)
    {
        Block block(messages[i].content);
        if(miner.verify(block))
        {
            long time = elapsedUsec(begins[messages[i].client], messages[i].timestamp);
            if(time < minTime)
            {
                minTime = time;
                if(bestBlock.hash == "")
                    bestBlock = Block(messages[i].content);
            }
            if(time - minTime <= usecThresh)
                clients[messages[i].client].reward += maxReward;
        }
    }
    
    return bestBlock;
}

int main(int argc, char const* argv[])
{
    if(argc < 4)
    {
        std::cerr<<"Indicate number of clients as first argument, the number of transactions as second and the mine difficulty as third."<<std::endl;
    }
    float reward = 0.25f;
    std::map<int, Client> clients;
    std::vector<std::vector<Message>> transactions;
    int numClients = atoi(argv[1]);
    int numTransactions = atoi(argv[2]);
    int mineDifficulty = atoi(argv[3]);

    Miner miner(mineDifficulty);
    Block block = Block();
    SocketServer server = SocketServer(PORT);
    server.init();
    server.acceptClients(numClients);
    std::vector<Message> names = server.receiveMessageFromAll();
    for(int i=0;i<numClients;i++)
    {
        clients[names[i].client] = Client(names[i].client, names[i].content);
    }
    server.broadcastMessage(std::to_string(mineDifficulty));
    for(int i = 0; i < numTransactions; i++)
    {
        std::string previousHash = block.hash;
        block = Block(++block.index, time(NULL), previousHash);
        std::cout<<"-----------------------------------------"<<std::endl;
        std::cout<<"New block:"<<std::endl;
        std::cout<<block.toString()<<std::endl;
        std::cout<<"-----------------------------------------"<<std::endl;
        std::cout<<"Broadcasting..."<<std::endl;
        std::vector<Message> broadcastedMessages = server.broadcastMessage(block.serialize());
        std::map<int, struct timeval> begins;
        for(int i=0;i<broadcastedMessages.size();i++)
        {
            begins[broadcastedMessages[i].client] = broadcastedMessages[i].timestamp;
        }
        std::cout<<"Waiting for response..."<<std::endl;
        std::vector<Message> messages = server.receiveMessageFromAll();

        block = computeReward(clients, messages, begins, miner, reward);
        
        for(int i=0;i<numClients;i++)
        {
            Block b(messages[i].content);
            std::cout<<std::endl;
            std::cout<<"Client "<<clients[messages[i].client].name<<" finished mining in "<<elapsedUsec(begins[messages[i].client], messages[i].timestamp)/1000000.0<<"s"<<std::endl;
            std::cout<<"Total awarded points: "<<clients[messages[i].client].reward<<std::endl;
            std::cout<<"Verified: "<<(miner.verify(b) ? "OK" : "FAIL")<<std::endl;
            std::cout<<b.toString()<<std::endl;
        }
        std::cout<<std::endl;
    }
}
