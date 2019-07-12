#include <czmq.h>
#include <chrono>
#include <vector>

const int hwm = 1000;

static
uint64_t now_us() {
    return std::chrono::duration_cast<std::chrono::microseconds>(
        std::chrono::system_clock::now().time_since_epoch()).count();
}

struct args_t {
    const char* addr;
    int count, id;
};

void server (zsock_t* pipe, void* vargs)
{
    const args_t& args = *(const args_t*)vargs;

    zsock_t* sock = zsock_new_router(args.addr);
    if (!sock) {
        zsys_error("failed to make router to \"%s\"", args.addr);
        return;
    }

    int count = args.count;
    zsock_set_sndhwm(sock, hwm);
    zsock_set_rcvhwm(sock, hwm);

    zpoller_t* poller = zpoller_new(sock, NULL);

    zsock_signal(pipe, 0);      // ready

    while (!zsys_interrupted) {

        void* which = zpoller_wait(poller, -1);
        if (!which) {
            zsys_info("interrupted");
            break;
        }
        

        //zframe_t* routing = zframe_recv(sock);
        zframe_t* routing = NULL;
        int seq=0;
        uint64_t then=0;
        int rc = zsock_recv(sock, "fi8", &routing, &seq, &then);
        if (rc != 0) {
            zsys_error("recv failed at %d", count);
            continue;
        }
        
        uint64_t now = now_us();
        zsys_info("%lu - %lu = %lu us", now, then, now-then);

        //zframe_send(routing, sock, ZFRAME_MORE);
        rc = zsock_send(sock, "fi88", routing, seq, then, now);
        if (rc != 0) {
            zsys_error("send failed at %d", count);
            continue;
        }

        --count;
        if (0 == count or 0 == seq) {
            break;
        }
    }
    zpoller_destroy(&poller);
    zsock_destroy(&sock);
    zsock_signal(pipe, args.id);      // done
}



void client(zsock_t* pipe, void* vargs)
{
    const args_t& args = *(const args_t*)vargs;

    zsock_t* sock = zsock_new_dealer(args.addr);
    int count = args.count;
    zsock_set_sndhwm(sock, hwm);
    zsock_set_rcvhwm(sock, hwm);

    const int snooze = 1000;

    zsock_signal(pipe, 0);      // ready

    while (!zsys_interrupted) {

        zclock_sleep(snooze);

        uint64_t t1 = now_us(), t1copy=0, st=0;
        int rc = zsock_send(sock, "i8", count, t1);
        if (rc != 0) {
            zsys_error("client: send failed at %d", count);
            continue;
        }
        
        int newcount = 0;
        rc = zsock_recv(sock, "i88", &newcount, &t1copy, &st);
        if (count != newcount) {
            zsys_error("count mismatch %d != %d", count, newcount);
        }
        if (t1 != t1copy) {    
            zsys_error("time mismatch %ld != %ld", t1, t1copy);
        }

        uint64_t t2 = now_us();

        zsys_info("[%d] there: %ld us, back: %ld us, total: %ld us",
                  count, st-t1, t2-st, t2-t1);

        --count;
        if (count == 0) {
            break;
        }
    }

    zsock_destroy(&sock);
    zsock_signal(pipe, args.id);      // done
}

int main(int argc, char* argv[])
{
    // check-latency c:<client-address>|s:<server-address> [c:...|s:...] ...

    zsys_init();
    const int nsend = 20;
    std::vector<zactor_t*> actors;

    zpoller_t* poller = zpoller_new(NULL);

    for (int ind=1; ind < argc; ++ind) {
        zactor_t* actor=NULL;
        int id = actors.size();
        if (argv[ind][0] == 's') {
            args_t* leak = new args_t{argv[ind]+2, nsend, id};
            actor = zactor_new(server, (void*)leak);
        }
        else if (argv[ind][0] == 'c') {
            args_t* leak = new args_t{argv[ind]+2, nsend, id};
            actor = zactor_new(client, (void*)leak);
        }
        else {
            zsys_error("unknown component description: %d: \"%s\"", ind, argv[ind]);
            return -1;
        }
        zpoller_add(poller, zactor_sock(actor));
        actors.push_back(actor);
    }

    int ndead = 0;
    while (!zsys_interrupted) {

        void* which = zpoller_wait(poller, -1);
        if (!which) {
            zsys_info("interrupted");
            break;
        }
        
        int ind = zsock_wait(which);
        if (ind < 0) {
            zsys_error("unexpected signal");
            continue;
        }

        if (actors[ind]) {
            zactor_destroy(&actors[ind]);
            ++ndead;
        }
        else {
            zsys_warning("component %d already destroyed", ind);
            continue;
        }
        if (ndead == actors.size()) {
            break;
        }
    }
    zpoller_destroy(&poller);
    return 0;
}
