#include "block.h"
#include "miner.h"
#include <sys/time.h>

#define MINE_DIFFICULTY 6

long elapsedUsec(struct timeval begin, struct timeval end)
{
    return (end.tv_sec * 1000000 + end.tv_usec) - (begin.tv_sec * 1000000 + begin.tv_usec);
}

int main()
{
    struct timeval begin;
    gettimeofday(&begin, NULL);
    Block block = Block(0, 0, "0");
    std::cout<<"Mining..."<<std::endl;
    Miner miner = Miner(MINE_DIFFICULTY);
    miner.mine(&block);
    struct timeval end;
    gettimeofday(&end, NULL);
    std::cout<<block.toString()<<std::endl;
    std::cout<<"Verified: "<<(miner.verify(block) ? "OK" : "FAIL")<<std::endl;
    std::cout<<"Elapsed time: "<<elapsedUsec(begin, end)/1000000.0<<"s"<<std::endl;
}

