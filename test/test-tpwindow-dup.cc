#include "ptmp/api.h"
#include "czmq.h"

#include "json.hpp"

#include "CLI11.hpp"

#include <thread>

// Every message data is assumed to be prefixed by a 64 bit unsigned
// int holding the size of the message data in bytes.
zmsg_t* read_msg(FILE* fp)
{
    size_t size=0;
    int nread = fread(&size, sizeof(size_t), 1, fp);
    if (nread != 1) {
        return NULL;
    }
    zchunk_t* chunk = zchunk_read(fp, size);
    zframe_t* frame = zchunk_pack(chunk);
    zchunk_destroy(&chunk);
    zmsg_t* msg = zmsg_decode(frame);
    zframe_destroy(&frame);
    return msg;
}

void print_tpset(ptmp::data::TPSet& tpset)
{
    for(auto const& tp: tpset.tps()){
        zsys_debug("    ch: %d t: %ld span: %d", tp.channel(), tp.tstart(), tp.tspan());
    }
}

int main(int argc, char** argv)
{
    CLI::App app{"Print dumped TPSets"};

    std::string input_file{"/nfs/sw/work_dirs/phrodrig/hit-dumps/run8567/FELIX_BR_508.dump"};
    app.add_option("-f", input_file, "Input file", false);

    int nsets=1000000;
    app.add_option("-n", nsets, "number of TPSets to print (-1 for no limit)", true);

    size_t buffer=5000;
    app.add_option("-b", buffer, "size of TPWindow buffer in 50MHz ticks", true);

    size_t window_size=2500; // 50us in 50MHz-ticks
    app.add_option("-w", window_size, "size of TPWindow window in 50MHz ticks", true);

    CLI11_PARSE(app, argc, argv);
    

    // zip together the sender threads with a zipper with a huge sync_time
    nlohmann::json window_config;
    window_config["input"]["socket"]["type"]="PULL";
    window_config["input"]["socket"]["connect"].push_back("inproc://sets");
    window_config["output"]["socket"]["type"]="PUSH";
    window_config["output"]["socket"]["bind"].push_back("inproc://windowout");
    window_config["toffset"]=0;
    window_config["tspan"]=window_size; 
    window_config["tbuf"]=buffer;

    ptmp::TPWindow window(window_config.dump());
    
    std::thread readthread([&input_file, nsets](){
            zsock_t* sender=zsock_new(ZMQ_PUSH);
            zsock_bind(sender, "inproc://sets");

            FILE* fp=fopen(input_file.c_str(), "r");
            zmsg_t* msg=NULL;
            int counter=0;
            while((msg = read_msg(fp))){
                zsock_send(sender, "m", msg);
                ++counter;
                if(nsets>0 && counter>nsets) break;
            }
            zsys_debug("readthread finished after %ld", counter);
            zsock_destroy(&sender);
        });

    std::thread sinkthread([](){
            nlohmann::json recv_config;
            recv_config["socket"]["type"]="PULL";
            recv_config["socket"]["connect"].push_back("inproc://windowout");

            ptmp::TPReceiver receiver(recv_config.dump());
            
            uint64_t prev_tstart=0;
            size_t counter=0;
            int nprinted=0;
            ptmp::data::TPSet tpset, prev_tpset;
            while(receiver(tpset, 1000)){
                if(nprinted<100 && tpset.tstart()==prev_tpset.tstart()){
                    zsys_debug("Duplicate tstart:");
                    zsys_debug("prev tpset: %d 0x%06x %ld %d", prev_tpset.count(), prev_tpset.detid(), prev_tpset.tstart(), prev_tpset.tps().size());
                    // print_tpset(prev_tpset);
                    zsys_debug("this tpset: %d 0x%06x %ld %d", tpset.count(), tpset.detid(), tpset.tstart(), tpset.tps().size());
                    print_tpset(tpset);
                    ++nprinted;
                }
                prev_tpset=tpset;
                ++counter;
            }
            zsys_debug("sinkthread finished after %ld", counter);
        });

    readthread.join();
    sinkthread.join();
}
