#include "ptmp/api.h"
#include "json.hpp"

#include <random>
#include <iostream>
#include <sstream>

using json = nlohmann::json;

struct conn_desc_t {
    std::string attach;         // "bind"/"connect"
    std::string socktype;       // "PUB", etc
    std::string scheme;         // "tcp"/"inproc"/"ipc"
    int number;
};

json swap_socktype(const json jsockcfg)
{
    json jcfg = jsockcfg;
    if (jcfg["socket"]["type"] == "PUB") {
        jcfg["socket"]["type"] = "SUB";
        return jcfg;
    }
    if (jcfg["socket"]["type"] == "SUB") {
        jcfg["socket"]["type"] = "PUB";
        return jcfg;
    }
    if (jcfg["socket"]["type"] == "PUSH") {
        jcfg["socket"]["type"] = "PULL";
        return jcfg;
    }
    if (jcfg["socket"]["type"] == "PULL") {
        jcfg["socket"]["type"] = "PUSH";
        return jcfg;
    }
    // PAIR is PAIR
    return jcfg;
}

json swap_sockbc(const json jsockcfg)
{
    json jcfg = jsockcfg;
    if (jcfg["socket"]["bind"].is_null()) {
        jcfg["socket"]["bind"] = jcfg["socket"]["connect"];
        jcfg["socket"].erase("connect");
    }
    else {
        jcfg["socket"]["connect"] = jcfg["socket"]["bind"];
        jcfg["socket"].erase("bind");
    }
    return jcfg;
}

json make_sockconfig(conn_desc_t desc)
{
    static int count = 0;
    json jcfg;
    jcfg["type"] = desc.socktype;
    for (int num=0; num<desc.number; ++num) {
        ++count;

        std::stringstream ss;
        if (desc.scheme == "tcp") {
            ss << "tcp://127.0.0.1:" << 12340+count;
        }
        if (desc.scheme == "ipc") {
            ss << "ipc://namedpipe" << count << ".ipc";
        }
        if (desc.scheme == "inproc") {
            ss << "inproc://name" << count;
        }
        jcfg[desc.attach].push_back(ss.str());
    }
    return jcfg;
}

std::vector<json> make_configs(conn_desc_t idesc, conn_desc_t odesc, int tardy=10)
{
    json jcfg;
    jcfg["tardy"] = tardy;
    jcfg["input"]["socket"] = make_sockconfig(idesc);
    jcfg["output"]["socket"] = make_sockconfig(odesc);
    json sendcfg = swap_socktype(swap_sockbc(jcfg["input"]));
    json recvcfg = swap_socktype(swap_sockbc(jcfg["output"]));
    return {sendcfg, jcfg, recvcfg};
}

std::vector<json> split_endpoints(json jcfg)
{
    std::vector<json> ret;
    std::vector<std::string> bcs{"bind","connect"};
    for (auto bc : bcs) {
        json endpoints = jcfg["socket"][bc];
        if (endpoints.is_null()) {
            continue;
        }
        for (auto jep : endpoints) {
            json one;
            one["socket"]["type"] = jcfg["socket"]["type"];
            one["socket"][bc][0] = jep;
            zsys_info("ENDPOINT %d: %s", ret.size(), one.dump(4).c_str());
            ret.push_back(one);
        }
    }
    return ret;
}

std::vector<ptmp::TPSender*> make_senders(json jsenders)
{
    std::vector<ptmp::TPSender*> ret;
    for (auto jcfg : split_endpoints(jsenders)) {
        std::string cfgstr = jcfg.dump(4);
        zsys_info("make sender: %s", cfgstr.c_str());
        ret.push_back(new ptmp::TPSender(cfgstr));
    }
    return ret;
}

void kill_senders(std::vector<ptmp::TPSender*> senders)
{
    for (auto s : senders) {
        delete s;
    }
}


const uint64_t usec = 50;       // 1 usec is 50 ticks of 50 MHz clock.
const uint64_t msec = 1000*usec;
const uint64_t sec = 1000*msec;

void dump_tpset(std::string ctx, ptmp::data::TPSet& tpset)
{
    return;
    zsys_info("%s: count:%-4d detid:%-2d tstart:%-8ld created:%ld",
              ctx.c_str(), tpset.count(), tpset.detid(), tpset.tstart(), tpset.created());
}

void run_sequence1(json scfg, json pcfg, json rcfg, int nsends, double period)
{
    // this is not a real time test.  It will send nsends TPSets
    // randomly across the senders and randomly each further in the
    // future assuming a mean period of time between sends of
    // "period".  It then checks that the correct ordering is
    // received.

    auto senders = make_senders(scfg);
    const size_t nsenders = senders.size();
    
    std::random_device rd;
    std::mt19937 gen(rd());
    std::uniform_int_distribution<> rsender(0, nsenders-1);
    std::exponential_distribution<double> rtimer(1.0/period);

    zsys_info("make sorted: %s", pcfg.dump(4).c_str());
    ptmp::TPSorted sorted_proxy(pcfg.dump());
    zsys_info("make receiver: %s", rcfg.dump(4).c_str());
    ptmp::TPReceiver recver(rcfg.dump());

    std::vector<ptmp::data::TPSet> send_records;

    zsys_info("sleeping 200ms to give time to connect");
    zclock_sleep(200);

    int64_t t0 = zclock_usecs();
    uint64_t tstart = 0;
    for (int seq=0; seq<nsends; ++seq) {
        int64_t dt = rtimer(gen);
        tstart += dt;
        int isender = rsender(gen);
        ptmp::data::TPSet tpset;
        tpset.set_count(seq);
        tpset.set_detid(isender);
        tpset.set_tstart(tstart);
        int64_t now = zclock_usecs() - t0;
        tpset.set_created(now);
        dump_tpset("send", tpset);
        send_records.push_back(tpset);
        (*senders[isender])(tpset);
    }


    int nerrors=0;
    for (int seq=0; seq<nsends; ++seq) {
        ptmp::data::TPSet& want = send_records[seq];
        assert (want.count() == seq);
        ptmp::data::TPSet got;
        bool ok = recver(got);
        dump_tpset("recv", got);
        assert(ok);
        if (got.count() != seq) {
            if (got.tstart() == want.tstart()) {
                zsys_warning("order warning, got mesg seq %d, expect %d, but tstart identical", got.count(), seq);
                dump_tpset("want:", want);
                dump_tpset(" got:", got);
            }
            else {
                zsys_error("order error, got mesg seq %d, expect %d", got.count(), seq);
                dump_tpset("want:", want);
                dump_tpset(" got:", got);
                ++nerrors;
            }
        }
    }

    assert(nerrors == 0);

    ptmp::data::TPSet nothing;
    bool ok = recver(nothing, 10); // should time out
    assert (!ok);

    kill_senders(senders);
}


void test1()
{
    // The typical data period between messages from one source.
    const uint64_t period = 50*usec;
    // when a source is considered tardy, in usec
    int tardy_msec = 10;

    // Proxy's input sockets first, then output.  If you get
    // "Operation not supported" then you've reversed them.
    auto jcfgs = make_configs({"connect", "SUB", "tcp",3},
                              {"bind", "PUB", "tcp", 1}, tardy_msec);

    run_sequence1(jcfgs[0], jcfgs[1], jcfgs[2], 10000, period);
}


//#include <pthread.h>
//#include <signal.h>
int main()
{
    //sigset_t block_set;
    ///sigfillset (&block_set);
    //sigaddset (&block_set, SIGPROF);
    //pthread_sigmask (SIG_SETMASK, &block_set, NULL);

    zsys_init();
    // eventually loop over various combos
    test1();
        
}
