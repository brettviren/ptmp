#include "ptmp/api.h"
#include "ptmp/internals.h"
#include "czmq.h"

#include "json.hpp"

#include "CLI11.hpp"

#include <thread>


void print_tpset(ptmp::data::TPSet& tpset)
{
    for(auto const& tp: tpset.tps()){
        zsys_debug("    ch: %d t: %ld (+%ld) span: %d",
                   tp.channel(), tp.tstart(), tp.tstart()-tpset.tstart(), tp.tspan());
    }
}

int main(int argc, char** argv)
{
    zsys_init();

    CLI::App app{"Print dumped TPSets"};

    // /nfs/sw/work_dirs/phrodrig/hit-dumps/run8567/FELIX_BR_508.dump
    std::string input_file{""};
    app.add_option("input file", input_file);

    int nsets=1000000;
    app.add_option("-n", nsets, "number of TPSets to print (-1 for no limit)", true);

    size_t buffer=5000;
    app.add_option("-b", buffer, "size of TPWindow buffer in 50MHz ticks", true);

    size_t window_size=2500; // 50us in 50MHz-ticks
    app.add_option("-w", window_size, "size of TPWindow window in 50MHz ticks", true);

    CLI11_PARSE(app, argc, argv);
    if (input_file.empty()) {
        zsys_error("usage: [options] <inputfile.dump>");
        return 0;               // avoid bombing the CI
    }

    // zip together the sender threads with a zipper with a huge sync_time
    nlohmann::json window_config;
    window_config["input"]["socket"]["type"]="PULL";
    window_config["input"]["socket"]["connect"].push_back("inproc://sets");
    window_config["output"]["socket"]["type"]="PUSH";
    window_config["output"]["socket"]["bind"].push_back("inproc://windowout");
    window_config["toffset"]=0;
    window_config["tspan"]=window_size; 
    window_config["tbuf"]=buffer;
    window_config["verbose"] = 1;


    ptmp::data::real_time_t proc_start = ptmp::data::now();

    // make on heap so we can destroy at will
    ptmp::TPWindow* window = new ptmp::TPWindow(window_config.dump());
    
    // crazy data structure to hold map of channel to map of data to
    // real time at sending.  This could get rather big depending on
    // how long this test runs.
    typedef std::unordered_map<int,
                               std::unordered_map<ptmp::data::data_time_t,
                                                  ptmp::data::real_time_t> > tstats_t;

    // only read after readthread has completed
    tstats_t tstats_send;

    std::thread readthread([&input_file, &tstats_send, nsets](){
            zsock_t* sender=zsock_new(ZMQ_PUSH);
            zsock_bind(sender, "inproc://sets");

            FILE* fp=fopen(input_file.c_str(), "r");
            zmsg_t* msg=NULL;
            int counter=0;
            while((msg = ptmp::internals::read(fp))){
                ptmp::data::real_time_t now = ptmp::data::now();
                ptmp::data::TPSet tpset;
                ptmp::internals::recv(&msg, tpset);
                if (tpset.tps_size() == 0) {
                    zsys_error("read empty TPSet after %d", counter);
                    assert(tpset.tps_size() > 0);
                }
                tpset.set_created(now); // just for the hell of it
                for (auto& tp : tpset.tps()) {
                    tstats_send[tp.channel()][tp.tstart()] = now;
                }
                ptmp::internals::send(sender, tpset);
                ++counter;
                if(nsets>0 && counter>nsets) break;
            }
            zsys_debug("readthread finished after %ld", counter);
            zsock_destroy(&sender);
        });

    // only read after sinkthread has completed
    tstats_t tstats_recv;
    std::thread sinkthread([&tstats_recv](){
            const ptmp::data::real_time_t trecv_beg = ptmp::data::now();

            nlohmann::json recv_config;
            recv_config["socket"]["type"]="PULL";
            recv_config["socket"]["connect"].push_back("inproc://windowout");

            ptmp::TPReceiver receiver(recv_config.dump());
            
            uint64_t prev_tstart=0, ntps_total = 0;
            size_t counter=0;
            int nprinted=0;
            ptmp::data::TPSet tpset, prev_tpset;
            while(receiver(tpset, 1000)){
                int detid = tpset.detid();
                auto ntps = tpset.tps_size();
                if (ntps == 0) {
                    zsys_error("received TPSet det:%d after %d", detid, counter);
                    assert(ntps > 0);
                    continue;
                }
                ntps_total += ntps;
                ptmp::data::real_time_t now = ptmp::data::now();
                for (auto& tp : tpset.tps()) {
                    tstats_recv[tp.channel()][tp.tstart()] = now;
                }

                if(nprinted<100 && tpset.tstart()==prev_tpset.tstart()){
                    zsys_debug("Duplicate tstart:");
                    zsys_debug("prev tpset: %d 0x%06x %ld %ld %d", prev_tpset.count(), prev_tpset.detid(), prev_tpset.tstart(), prev_tpset.tspan(), prev_tpset.tps().size());
                    print_tpset(prev_tpset);
                    zsys_debug("this tpset: %d 0x%06x %ld %ld %d", tpset.count(), tpset.detid(), tpset.tstart(), tpset.tspan(), tpset.tps().size());
                    print_tpset(tpset);
                    ++nprinted;
                }
                assert (tpset.tstart() > prev_tpset.tstart());
                prev_tpset=tpset;
                ++counter;
            }
            const ptmp::data::real_time_t trecv_dt = ptmp::data::now() - trecv_beg;
            const double trecv_dt_ms = trecv_dt / 1000.0;
            zsys_debug("sinkthread in %.3f s received %ld TPSets (%.1f kHz) %ld TPs (%.1f kHz)",
                       trecv_dt_ms/1000.0, counter, counter/trecv_dt_ms, ntps_total, ntps_total/trecv_dt_ms);
            // fixme add similar chirp at send end.......
        });

    readthread.join();
    sinkthread.join();
    delete window; window=nullptr;

    { // for throughput 
        double proc_time_ms = (ptmp::data::now() - proc_start) / 1000.0;
        assert(proc_time_ms > 0);

        double n=0, t_ms=0, t2_ms=0, mint_ms=-1, maxt_ms=-1;
        zsys_debug("stats for %ld channels sent, %ld channels recv",
                   tstats_send.size(), tstats_recv.size());
        assert(tstats_recv.size() > 0);
        assert(tstats_send.size() > 0);
        for (auto& tss : tstats_send) {
            int chan = tss.first;
            auto& ts = tss.second;
            auto& tr = tstats_recv[chan];
            assert (ts.size() > 0);
            assert (tr.size() > 0);

            for (auto& stt : ts) {
                auto send_time = stt.second;
                if (send_time == 0) {
                    zsys_debug("chan %d recv TP that wasn't sent", chan);
                }
                assert (send_time > 0);
                    
                auto recv_time = tr[stt.first];
                if (recv_time == 0) {
                    zsys_debug("chan %d missing TP %ld", chan, stt.first);
                    continue;
                }

                double dt_ms = (recv_time - send_time)/1000.0;
                if (dt_ms < 0 ) {
                    zsys_debug("chan %d violates causality: dt_ms = %f ms", chan, dt_ms);
                }
                assert(dt_ms >= 0);

                if (mint_ms <0 ) {
                    mint_ms = maxt_ms = dt_ms;
                }
                else {
                    mint_ms = std::min(mint_ms, dt_ms);
                    maxt_ms = std::max(maxt_ms, dt_ms);
                }
                n += 1;
                t_ms += dt_ms;
                t2_ms += dt_ms*dt_ms;
            }
        }
        double mean_ms = t_ms/n;
        double rms_ms = sqrt(t2_ms/n - mean_ms*mean_ms);

        zsys_info("throughput: %.3f kHz, latency (ms): %.3f +/- %.4f [%.3f, %.3f]",
                  n/proc_time_ms,
                  mean_ms, rms_ms, mint_ms, maxt_ms);
    }
    return 0;
}
