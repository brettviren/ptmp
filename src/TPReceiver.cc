#include "ptmp/api.h"
#include "internals.h"


ptmp::TPReceiver::TPReceiver(const std::string& config)
    : m_sock(new ptmp::internals::Socket(config))
{
}

ptmp::TPReceiver::~TPReceiver()
{
    delete m_sock;
    m_sock = 0;
}

void ptmp::TPReceiver::operator()(data::TPSet& tps)
{
    zmsg_t* msg = zmsg_recv(m_sock->get()); // blocks
    if (!msg) {
        std::cerr << "got no message\n";
        return;
    }

    zframe_t* fid = zmsg_first(msg);
    if (!msg) {
        std::cerr << "got no id frame\n";
        return;
    }
    int topic = *(int*)zframe_data(fid);

    zframe_t* pay = zmsg_next(msg);
    if (!msg) {
        std::cerr << "got no payload frame\n";
        return;
    }
    tps.ParseFromArray(zframe_data(pay), zframe_size(pay));

    zmsg_destroy(&msg);
}

