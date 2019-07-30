// from Phil

#include "ptmp/api.h"
#include "json.hpp"

#include "CLI11.hpp"

#include <cstdio>
#include <thread>
#include <chrono>
#include <atomic>
#include <cmath>

std::atomic<bool> goFlag{false};

// send a TPSet with the given `count` and `tstart` on `sender`, then
// sleep `sleep_ms` milliseconds
void send(ptmp::TPSender& sender, int detid, int count, uint64_t tstart, int sleep_ms=0)
{
    ptmp::data::TPSet tpset;
    tpset.set_count(count);
    tpset.set_created(ptmp::data::now());
    tpset.set_detid(detid);
    tpset.set_tstart(tstart);
    sender(tpset);
    if(sleep_ms) std::this_thread::sleep_for(std::chrono::milliseconds(sleep_ms));
}

// Create a TPSender that sends `n_messages` TPSets with contiguous
// count and tstart, separated by `window_ms` milliseconds, starting
// `late_ms` after goFlag goes high
void send_loop(int index, int n_messages, int window_ms, int late_ms, std::string addr)
{
    nlohmann::json sender_config;
    sender_config["socket"]["type"]="PUSH";
    sender_config["socket"]["bind"].push_back(addr);
    ptmp::TPSender sender(sender_config.dump());
    const int detid = 100*(1+index);
    int count=0;

    while(!goFlag.load()) ; // spin until told to start

    // Become as late as requested
    std::this_thread::sleep_for(std::chrono::milliseconds(late_ms));

    // Send a message every window_ms milliseconds
    for(int i=0; i<n_messages; ++i){
        send(sender, detid, count++, i, window_ms);
    }
}

void recv_loop(int n_messages, int sync_ms, std::string addr)
{
    struct stat { ptmp::data::real_time_t t2, t, n; };
    std::unordered_map<int, stat> stats;

    nlohmann::json recver_config;
    recver_config["socket"]["type"]="SUB";
    recver_config["socket"]["connect"].push_back(addr);
    ptmp::TPReceiver recver(recver_config.dump());

    for (int ind=0; ind<n_messages; ++ind) {
        ptmp::data::TPSet tpset;
        bool ok = recver(tpset);
        assert(ok);
        ptmp::data::real_time_t dt = ptmp::data::now() - tpset.created();
        
        int detid = tpset.detid();
        stats[detid].n += 1;
        stats[detid].t += dt;
        stats[detid].t2 += dt*dt;
    }

    zsys_debug("latency through zipper (sync time %d ms):", sync_ms);
    for (auto sit : stats) {
        auto& s = sit.second;
        double n = s.n;
        double mean = s.t/n;
        double rms = sqrt(s.t2/n - mean*mean);
        zsys_debug("\t%d: n:%.0f %.3f +/- %.4f ms",
                   sit.first, n, mean/1000, rms/1000);
    }

}

int main(int argc, char* argv[])
{
    zsys_init();

    CLI::App app{"Check TPZipper"};

    int n_messages=20;
    app.add_option("-n", n_messages, "number of TPSets to send from each input stream", true);
    int window_ms=300;
    app.add_option("-w", window_ms, "nominal time between sends (in ms)", true);
    int lateness_ms=5;
    app.add_option("-l", lateness_ms, "epsilon lateness for streams (in ms)", true);    
    int sync_ms=100;
    app.add_option("-s", sync_ms, "zipper sync time (in ms)", true);    


    std::string input_address = "inproc://input%d";
    app.add_option("-i", input_address, "address for input to TPZipper (include a %d)", true);
    std::string output_address = "inproc://output";
    app.add_option("-o", output_address, "address for output from TPZipper", true);

    CLI11_PARSE(app, argc, argv);
    

    // Set up three sender threads, one that's "on time", one that's nearly a
    // full window behind, and one that's just over a full window
    // behind

    const int N=3;
    const int latenesses[N]={0, window_ms-lateness_ms, window_ms+lateness_ms};

    std::vector<std::thread> threads;
    std::vector<std::string> input_addrs;
    for(size_t i=0; i<N; ++i){
        char addr[1024]={0};
        snprintf(addr, 1024, input_address.c_str(), i);
        input_addrs.push_back(addr);
        threads.emplace_back(send_loop, i, n_messages, window_ms, latenesses[i], input_addrs.back());
    }
    threads.emplace_back(recv_loop, n_messages*N, sync_ms, output_address);

    // zip together the sender threads with a zipper with a huge sync_time
    nlohmann::json zipper_config;
    zipper_config["verbose"] = 1;
    zipper_config["input"]["socket"]["type"]="PULL";
    for(size_t i=0; i<N; ++i){
        zipper_config["input"]["socket"]["connect"].push_back(input_addrs[i]);
    }
    zipper_config["output"]["socket"]["type"]="PUB";
    zipper_config["output"]["socket"]["bind"].push_back(output_address.c_str());
    zipper_config["sync_time"]=sync_ms;

    ptmp::TPZipper zipper(zipper_config.dump());

    // Give all the sockets a chance to connect
    std::this_thread::sleep_for(std::chrono::milliseconds(300));

    // Tell all the threads to start sending messages
    goFlag.store(true);

    for(auto& thread: threads) thread.join();

    // Wait for at a sync time to allow for last messages to flush
    // before causing zipper to shutdown through destruction.
    std::this_thread::sleep_for(std::chrono::milliseconds(2*sync_ms));

    return 0;
}
