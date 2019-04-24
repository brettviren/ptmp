// Generate messages

#include "CLI11.hpp"
#include <czmq.h>

#include <vector>
#include <string>
#include <unordered_map>

struct sock_opt_t {
    zsock_t* sock;
    int hwm;
    std::string socktype;
    std::string attach;
    std::vector<std::string> endpoints;
    std::vector<std::string> subscriptions; 
};
int make_sock(sock_opt_t& sopt)
{
    if (sopt.endpoints.empty()) {
        return -1;
    }
    const std::unordered_map<std::string, int> socktypemap{
        {"PUB",ZMQ_PUB},{"SUB",ZMQ_SUB},{"PUSH",ZMQ_PUSH},{"PULL",ZMQ_PULL}
    };
    const auto& st = socktypemap.find(sopt.socktype);
    if (st == socktypemap.end()) {
        zsys_error("Unknown socket type: \"%s\"", sopt.socktype.c_str());
        return -1;
    }
    int socktype = st->second;
    sopt.sock = zsock_new(socktype);
    if (socktype == ZMQ_SUB) {
        if (sopt.subscriptions.empty()) {
            zsock_set_subscribe(sopt.sock, "");
            //zsys_debug("subscribing to default");
        }
        else {
            for (auto sub : sopt.subscriptions) {
                zsock_set_subscribe(sopt.sock, sub.c_str());
                //zsys_debug("subscribing to \"%s\"", sub.c_str());
            }
        }
    }
    if (sopt.attach == "connect") {
        for (auto ep : sopt.endpoints) {
            zsock_connect(sopt.sock, ep.c_str(), NULL);
        }
    }
    if (sopt.attach == "bind") {
        for (auto ep : sopt.endpoints) {
            zsock_bind(sopt.sock, ep.c_str(), NULL);
        }
    }
    return 0;
}
int main(int argc, char* argv[])
{
    zsys_init();

    CLI::App app{"Yet another ZeroMQ netcat'ish program"};

    int number=1;
    app.add_option("-n,--number", number,
                   "Number of messages to send before terminating");

    int loop_delay=0;
    app.add_option("-l,--loop-delay-ms", loop_delay, "Loop delay");

    int size=10000;
    app.add_option("-S,--size", size, "Size of messages in bytes");

    int debug=0;
    app.add_option("-d,--debug", debug,
                   "Debug level. 0=errors, 1=summary, 2=verbose");

    int begwait=0;
    app.add_option("-B,--begin-wait-ms", begwait,
                   "Number of ms to wait between creating socket and starting to send");
    int endwait=0;
    app.add_option("-E,--end-wait-ms", endwait,
                   "Number of ms to wait between completing --count messages and terminating");

    // Sockets
    sock_opt_t sopt{NULL,1000,"PUB", "bind"};
    app.add_option("-m,--socket-hwm", sopt.hwm,
                   "The ZeroMQ socket highwater mark");    
    app.add_option("-p,--socket-pattern", sopt.socktype,
                   "The ZeroMQ socket pattern for endpoint [PUB, PAIR, PUSH]");
    app.add_option("-a,--socket-attachment", sopt.attach,
                   "The socket attachement method [bind|connect] for this endpoint");
    app.add_option("-s,--subscriptions", sopt.subscriptions,
                   "Any subscriptions if SUB.");
    app.add_option("-e,--socket-endpoints", sopt.endpoints,
                   "The socket endpoint addresses in Zero MQ format (tcp:// or ipc://)")->expected(-1);

    CLI11_PARSE(app, argc, argv);

    if (make_sock(sopt) < 0) {
        return -1;
    }

    void* data = malloc(size);
    zmsg_t* msg = zmsg_new();
    zmsg_pushmem(msg, data, size);

    if (begwait) {
        zclock_sleep(begwait);
    }

    int64_t t1 = zclock_usecs();

    for (int count = 0 ; count < number; ++count) {
        zsock_send(sopt.sock, "m", msg);
        if (loop_delay) {
            zclock_sleep(loop_delay);
        }
    }
    
    int64_t t2 = zclock_usecs();

    if (endwait) {
        zclock_sleep(endwait);
    }

    const double usec = (t2-t1);
    const double msec = usec / 1000;
    const double sec = msec / 1000;

    double mb = size;
    mb /= 1000000;

    zsys_info("sent %d of %.3f MB in %.3f s: %.3f kHz, %.3f GB/s",
              number, mb, sec, number/msec, (mb*number)/msec);

    zmsg_destroy(&msg);
    free(data);
    zsock_destroy(&sopt.sock);
    return 0;
}
