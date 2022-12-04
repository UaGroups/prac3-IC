#include <iostream>
#include <string>
#include <time.h>
#include "miner.h"
#include "socket.h"
#include <omp.h>

int main(int argc, char** argv)
{
    if(argc < 3)
    {
        std::cerr<<"Indicate the server ip as first argument and the name of the group as second"<<std::endl;
        exit(0);
    }
    SocketClient client = SocketClient();
    client.connectTo(argv[1], 9999);
    client.sendMessage(argv[2]);
    int difficulty = atoi(client.receiveMessage().c_str());
    Miner miner(difficulty);
    while(true)
    {
        std::string message = client.receiveMessage();
        if (message == "")
        {
            std::cout<<"Disconnected"<<std::endl;
            return 0;
        }
        Block block(message);
        std::cout<<"Received block:"<<std::endl;
        std::cout<<block.toString()<<std::endl;
        std::cout<<std::endl;
        
        miner.mine(&block);
        std::cout<<block.toString()<<std::endl;
        client.sendMessage(block.serialize());
    }
}
