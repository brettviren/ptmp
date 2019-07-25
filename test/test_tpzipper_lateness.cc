// from Phil

#include "ptmp/api.h"
#include "json.hpp"

#include <cstdio>
#include <thread>
#include <chrono>
#include <atomic>

std::atomic<bool> goFlag{false};

// send a TPSet with the given `count` and `tstart` on `sender`, then
// sleep `sleep_ms` milliseconds
void send(ptmp::TPSender& sender, int count, uint64_t tstart, int sleep_ms=0)
{
    ptmp::data::TPSet tpset;
    tpset.set_count(count);
    tpset.set_created(100);
    tpset.set_detid(1);
    tpset.set_tstart(tstart);
    sender(tpset);
    if(sleep_ms) std::this_thread::sleep_for(std::chrono::milliseconds(sleep_ms));
}

// Create a TPSender that sends `n_messages` TPSets with contiguous
// count and tstart, separated by `window_ms` milliseconds, starting
// `late_ms` after goFlag goes high
void send_loop(int index, int n_messages, int window_ms, int late_ms)
{
    nlohmann::json sender_config;
    sender_config["socket"]["type"]="PUSH";
    sender_config["socket"]["bind"].push_back(std::string("inproc://sender")+std::to_string(index));
    ptmp::TPSender sender(sender_config.dump());

    int count=0;

    while(!goFlag.load()) ; // spin until told to start

    // Become as late as requested
    std::this_thread::sleep_for(std::chrono::milliseconds(late_ms));

    // Send a message every window_ms milliseconds
    for(int i=0; i<n_messages; ++i){
        send(sender, count++, i, window_ms);
    }
}

int main()
{
    // avoid:
    // test_tpzipper_lateness: src/zsys.c:213: zsys_init: Assertion `!s_process_ctx' failed.
    zsys_init();

    // Set up three sender threads, one that's "on time", one that's nearly a
    // full window behind, and one that's just over a full window
    // behind
    const size_t N=3;
    const int n_messages=20;
    const int window_ms=300;
    const int latenesses[N]={0, window_ms-5, window_ms+5};

    std::vector<std::thread> threads;
    for(size_t i=0; i<N; ++i){
        threads.emplace_back(send_loop, i, n_messages, window_ms, latenesses[i]);
    }

    // zip together the sender threads with a zipper with a huge sync_time
    nlohmann::json zipper_config;
    zipper_config["input"]["socket"]["type"]="PULL";
    for(size_t i=0; i<N; ++i){
        zipper_config["input"]["socket"]["connect"].push_back(std::string("inproc://sender")+std::to_string(i));
    }
    zipper_config["output"]["socket"]["type"]="PUB";
    zipper_config["output"]["socket"]["bind"].push_back("inproc://zipout");
    zipper_config["sync_time"]=UINT32_MAX;

    ptmp::TPZipper zipper(zipper_config.dump());

    // Give all the sockets a chance to connect
    std::this_thread::sleep_for(std::chrono::milliseconds(300));

    // Tell all the threads to start sending messages
    goFlag.store(true);

    for(auto& thread: threads) thread.join();

    // Wait or everything to get to the TPZipper before calling its dtor
    std::this_thread::sleep_for(std::chrono::milliseconds(300));

}
