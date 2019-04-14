#include <czmq.h>

#include <vector>
#include <string>

void get_check(zpoller_t* poll, zsock_t* sock, const char* want, int iwant)
{
    void* which=NULL;
    for (int delay = 0; delay <= 1024; delay *= 2) {
        which = zpoller_wait(poll, delay);
        if (which) break;
        if (zpoller_expired(poll)) {
            zsys_info("delay %4d %d", delay, iwant);
            if (!delay) delay = 1;
            continue;
        }
        if (zpoller_terminated(poll)) {
            zsys_info("poll terminated");
            return;
        }
        assert(streq("this can","not happen"));
    }
    assert (which);
    assert (which == sock);
    int count=0;
    char* msg=NULL;
    int rc = zsock_recv(sock, "si", &msg, &count);
    assert (rc == 0);
    assert(msg);
    assert(streq(msg, want));
    assert(iwant == count);
    zsys_info("%d %s recv", count, msg);
    free (msg);
}

struct sender_args_t {
    int sock_type;
    const char* url;
    size_t nsends;
    int hwm;
    std::vector<std::string> msgs;
};
void sender(zsock_t* pipe, void* vargs)
{
    sender_args_t* args = (sender_args_t*)(vargs);

    zsock_t* src = zsock_new(args->sock_type);
    assert(src);
    zsock_set_sndhwm(src, args->nsends);
    int port = zsock_bind(src, args->url, NULL);
    assert (port >= 0);

    int rc = zsock_signal(pipe, 0);      // ready
    assert (rc == 0);

    const size_t nuniq = args->msgs.size();
    for (int count = 0; count < args->nsends; ++count) {
        const auto& msg = args->msgs[count%nuniq];
        int rc = zsock_send(src, "si", msg.c_str(), count);
        assert(rc == 0);
        zsys_info("%d %s sent", count, msg.c_str());
    }

    zsys_info("sender finished with %d, waiting for term signal",
        args->nsends);

    zsock_wait(pipe);

    zsock_destroy(&src);
}

int main(int argc, char* argv[])
{
    int src_type = ZMQ_PUSH;
    int tgt_type = ZMQ_PULL;
    if (argc > 1) {
        if (streq(argv[1], "pair")) {
            src_type = ZMQ_PAIR;
            tgt_type = ZMQ_PAIR;
        }
        if (streq(argv[1], "pubsub")) {
            src_type = ZMQ_PUB;
            tgt_type = ZMQ_SUB;
        }
        if (streq(argv[1], "pushpull")) {
            src_type = ZMQ_PUSH;
            tgt_type = ZMQ_PULL;
        }
    }

    const size_t nsends = 1000000;
    const char* url = "tcp://127.0.0.1:12345";
    sender_args_t sargs{src_type, url, nsends, 1000, {"one","two","tre"}};

    zsys_init();


    zsock_t* tgt = zsock_new(tgt_type);
    assert(tgt);
    zsock_set_rcvhwm(tgt, 10);
    int rc = zsock_connect(tgt, url, NULL);
    assert(rc == 0);
    zpoller_t* poll = zpoller_new(tgt, NULL);
    assert(poll);

    zactor_t* src = zactor_new(sender, &sargs);
    assert(src);

    const size_t nuniq = sargs.msgs.size();
    for (int count = 0; count < nsends; ++count) {
        const auto& want = sargs.msgs[count%nuniq];
        get_check(poll, tgt, want.c_str(), count);
        if (count % 100 == 0) {
            zclock_sleep(1);
            zsys_info("%d sleep", count);
        }
    }

    zsock_signal(src, 0);

    zpoller_destroy(&poll);
    zsock_destroy(&tgt);
    zactor_destroy(&src);    
    return 0;    
}
