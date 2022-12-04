#ifndef __BLOCK_H_
#define __BLOCK_H_

#include <string>
#include <time.h>
#include <sstream>

class Block
{
    public:
        int index;
        time_t timestamp;
        std::string previousHash;
        std::string hash;
        unsigned int nonce;
        
        Block() : Block(0, time(NULL), "0")
        {
        }
        
        Block(std::string content)
        {
            this->deserialize(content);
        }

        Block(int index, time_t timestamp, const std::string &previousHash)
        {
            this->index = index;
            this->timestamp = timestamp;
            this->previousHash = previousHash;
            this->nonce = 0;
        }

        std::string toString()
        {
            std::stringstream ss;
            ss<<"Index: "<<index<<"\nTimestamp: "<<timestamp<<"\nHash: "<<hash<<"\nPrevious hash: "<<previousHash<<"\nNonce: "<<nonce;

            return ss.str();
        }
        
        std::string serialize()
        {
            std::stringstream ss;
            ss<<index<<"\n"<<timestamp<<"\n"<<previousHash<<"\n"<<hash<<"\n"<<nonce;

            return ss.str();
        }
        
        void deserialize(const std::string &content)
        {
            std::stringstream ss(content);
            ss>>index;
            ss>>timestamp;
            ss>>previousHash;
            ss>>hash;
            ss>>nonce;
        }
};

#endif
