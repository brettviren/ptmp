#include <czmq.h>

int main()
{
    zactor_t* proxy = zactor_new(zproxy, NULL);
    assert(proxy);
    int rc;
    zstr_sendx (proxy, "VERBOSE", NULL);
    zsock_wait (proxy);

    rc = zstr_sendx(proxy, "FRONTEND", "PULL", ">tcp://127.0.0.1:5678", NULL);
    assert(rc == 0);

    rc = zsock_wait(proxy);
    assert(rc >= 0);

    rc = zstr_sendx(proxy, "BACKEND", "PUSH", "@tcp://127.0.0.1:5679", NULL);
    assert(rc == 0);

    rc = zsock_wait(proxy);
    assert(rc >= 0);

    zsock_t* tap = zsock_new_pull("inproc://capture");
    assert(tap);

    // why oh why is "PUSH" hard coded?
    rc = zstr_sendx(proxy, "CAPTURE", "inproc://capture", NULL);
    assert(rc == 0);

    rc = zsock_wait(proxy);
    assert(rc >= 0);

    zsock_t* faucet = zsock_new_push("@tcp://127.0.0.1:5678");
    assert(faucet);
    
    zsock_t* sink = zsock_new_pull(">tcp://127.0.0.1:5679");
    assert(sink);


    zstr_sendx(faucet, "hello", NULL);
    char* hello1=0;
    char* hello2=0;
    zstr_recvx(sink, &hello1, NULL);
    zstr_recvx(tap, &hello2, NULL);
    assert(streq(hello1,"hello"));
    assert(streq(hello2,"hello"));

    zactor_destroy(&proxy);
    zsock_destroy(&faucet);
    zsock_destroy(&sink);
    zsock_destroy(&tap);

    return 0;
}
