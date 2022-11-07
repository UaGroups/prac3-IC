#ifndef __MINER_H_
#define __MINER_H_
#include <omp.h>
#include <openssl/evp.h>
#include "block.h"
#include <iostream>
#include <sstream>
#include <iomanip>
#include <string>

std::string sha256(const std::string& unhashed)
{
    EVP_MD_CTX* context = EVP_MD_CTX_new();
    if(context == NULL)
    {
        std::cerr<<"Error on EVP_MD_CTX_new"<<std::endl;
        exit(0);
    }

    if(EVP_DigestInit_ex(context, EVP_sha256(), NULL) != 1)
    {
        std::cerr<<"Error on EVP_DigestInit_ex"<<std::endl;
        exit(0);
    }
    if(EVP_DigestUpdate(context, unhashed.c_str(), unhashed.length()) != 1)
    {
        std::cerr<<"Error on EVP_DigestUpdate"<<std::endl;
        exit(0);
    }
    unsigned char hash[EVP_MAX_MD_SIZE];
    unsigned int lengthOfHash = 0;

    if(EVP_DigestFinal_ex(context, hash, &lengthOfHash) != 1)
    {
        std::cerr<<"Error on EVP_DigestFinal_ex"<<std::endl;
        exit(0);
    }
    std::stringstream ss;
    for(unsigned int i = 0; i < lengthOfHash; ++i)
    {
        ss << std::hex << std::setw(2) << std::setfill('0') << (int)hash[i];
    }

    EVP_MD_CTX_free(context);

    return ss.str();
}

class Miner
{
    public:
        Miner(int difficulty)
        {
            std::stringstream ss;
            for(int i=0;i<difficulty;i++)
                ss<<'0';
            this->zeros = ss.str();
        }
        
        Block* mine(Block* block) const
        {
            // create a copy of the block
            Block mined = Block(block->serialize());
            mined.nonce = 0;
            std::string hash = this->calculateHash(&mined);
            while(!this->verify(hash))
            {
                mined.nonce++;
                hash = this->calculateHash(&mined);
            };
            
            // update block with mined hash
            block->nonce = mined.nonce;
            block->hash = hash;
            
            return block;
        }
        
        bool verify(Block& block)
        {
            return this->verify(this->calculateHash(&block));
        }
        
        bool verify(const std::string &hash) const
        {
            return hash.substr(0, this->zeros.length()).compare(this->zeros) == 0;
        }
        
    private:
        std::string zeros;
        
        std::string calculateHash(Block* block) const
        {
            std::stringstream ss;
            ss<<block->index<<block->timestamp<<block->previousHash<<block->nonce;
            
            return sha256(ss.str());
        }
};

#endif
