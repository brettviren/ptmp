// test from Phil to reuse TPSet instead of constructing it anew for
// each parse.
#include "ptmp/api.h"
#include <stdlib.h>

#include <vector>
#include <iostream>
#include <chrono>

struct NoReuseParser
{
    uint64_t parse(void* data, int size)
    {
        ptmp::data::TPSet tpset;
        tpset.ParseFromArray(data, size);
        return tpset.tstart();
    }
};

struct ReuseParser
{
    ptmp::data::TPSet m_tpset;

    uint64_t parse(void* data, int size)
    {
        m_tpset.ParseFromArray(data, size);
        return m_tpset.tstart();
    }
};

ptmp::data::TPSet random_tpset()
{
    static int count=0;
    ptmp::data::TPSet tpset;
    tpset.set_count(count++);
    tpset.set_created(100);
    tpset.set_detid(1);
    tpset.set_tstart(rand());
    for(size_t i=0; i<rand()%20; ++i){
        ptmp::data::TrigPrim* tp=tpset.add_tps();
        tp->set_channel(10);
        tp->set_tstart(10);
    }
    return tpset;
}

void fillBuffer(void* buffer, size_t buffer_size, std::vector<uint64_t>& sizes)
{
    size_t total_size=0;
    ptmp::data::TPSet tpset=random_tpset();
    size_t tpset_size=tpset.ByteSizeLong();
    char* current=(char*)buffer;
    while(total_size+tpset_size<buffer_size){
        tpset.SerializeToArray(current, tpset_size);
        total_size+=tpset_size;
        sizes.push_back(tpset_size);
        current+=tpset_size;
        tpset=random_tpset();
        tpset_size=tpset.ByteSizeLong();
    }
}

template<class T>
size_t readWithParser(T& parser, void* buffer, const std::vector<uint64_t>& sizes)
{
    size_t total_tstart=0;
    char* current=(char*)buffer;
    for(size_t i=0; i<sizes.size(); ++i){
        total_tstart+=parser.parse(current, sizes[i]);
        current+=sizes[i];
    }
    return total_tstart;
}

int main()
{
    srand(0xdeadfeeb);
    const size_t buffer_size=10000000;
    void* buffer=malloc(buffer_size);
    std::vector<uint64_t> sizes;
    fillBuffer(buffer, buffer_size, sizes);
    std::cout << "Filled " << sizes.size() << " tpsets" << std::endl;

    NoReuseParser noReuse;
    auto start1=std::chrono::steady_clock::now();
    size_t total_tstart1=readWithParser(noReuse, buffer, sizes);
    auto end1=std::chrono::steady_clock::now();
    size_t duration1=std::chrono::duration_cast<std::chrono::microseconds>(end1-start1).count();
    double rate1=double(sizes.size())/duration1;

    ReuseParser reuse;
    auto start2=std::chrono::steady_clock::now();
    size_t total_tstart2=readWithParser(reuse, buffer, sizes);
    auto end2=std::chrono::steady_clock::now();
    size_t duration2=std::chrono::duration_cast<std::chrono::microseconds>(end2-start2).count();
    double rate2=double(sizes.size())/duration2;

    if(total_tstart1==total_tstart2){
        std::cout << "totals matched" << std::endl;
    }
    else{
        std::cout << "totals did not match" << std::endl;
    }
    std::cout << "Without reuse: " << duration1 << " us, " << rate1 << "m sets/s" << std::endl;
    std::cout << "With reuse:    " << duration2 << " us, " << rate2 << "m sets/s" << std::endl;
    free(buffer);
}

