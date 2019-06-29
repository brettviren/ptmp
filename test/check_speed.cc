#include "ptmp/api.h"
#include "ptmp/data.h"
#include "ptmp/testing.h"

#include <iostream>
#include <chrono>

using namespace ptmp::data;
//typedef std::chrono::time_point<std::chrono::system_clock> time_point;
using namespace std::chrono;

static 
void serialize(TPSet& tpset)
{
    size_t siz = tpset.ByteSize();
    std::vector<char> buf(siz);
    tpset.SerializeToArray(buf.data(), siz);
}


struct fill_random_and_serialize_t {
    ptmp::testing::make_tps_t& filler;
    void operator()(TPSet& tpset) {
        filler(tpset);
        serialize(tpset);
    }
};

struct fill_and_serialize_t {
    
    void operator()(TPSet& tpset) {
        for (int ind=0; ind < 100; ++ind) {
            tpset.add_tps();
        }
        serialize(tpset);
    }
};


template<typename tpset_doer_t>
void test_ntimes(int count, tpset_doer_t& dosomething)
{
    for (int ind=0; ind<count; ++ind) {
        TPSet tpset;
        ptmp::testing::init(tpset);
        dosomething(tpset);
    }
}

template<typename tpset_doer1_t, typename tpset_doer2_t>
void test_fillonce(int count, tpset_doer1_t& filler, tpset_doer2_t& doer)
{
    TPSet tpset;
    ptmp::testing::init(tpset);
    filler(tpset);
    for (int ind=0; ind<count; ++ind) {
        doer(tpset);
    }
}


static
void dump_diff(int n, system_clock::time_point t1, system_clock::time_point t0, std::string msg)
{
    auto dt = duration_cast<microseconds>(t1 - t0).count();
    const double khz = n / (dt*1.0e-3);
    std::cout << khz << " kHz " << msg << "\n";
}

int main(int argc, char* argv[])
{
    ptmp::testing::make_tps_t filler(100, 10);
    fill_random_and_serialize_t frands{filler};
    fill_and_serialize_t fands;

    const int ntries = 100000;
    
    auto t0 = system_clock::now();

    test_ntimes(ntries, serialize);
    auto t1 = system_clock::now();

    test_ntimes(ntries, filler);
    auto t2 = system_clock::now();
    
    test_ntimes(ntries, frands);
    auto t3 = system_clock::now();

    test_ntimes(ntries, serialize);
    auto t4 = system_clock::now();

    test_ntimes(ntries, fands);
    auto t5 = system_clock::now();

    test_fillonce(ntries, frands, serialize);
    auto t6 = system_clock::now();

    
    dump_diff(ntries, t1, t0, "serialize empty");
    dump_diff(ntries, t2, t1, "fill random");
    dump_diff(ntries, t3, t2, "fill random and serialize");
    dump_diff(ntries, t4, t3, "serialize empty (again)");
    dump_diff(ntries, t5, t4, "fill dummy and serialize");
    dump_diff(ntries, t6, t5, "fill once, serialize many");

    return 0;
}
