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

bool ptmp::TPReceiver::operator()(data::TPSet& tps, int toms)
{
    zmsg_t* msg = m_sock->msg(toms);
    if (!msg) {
        return false;
    }

    zframe_t* fid = zmsg_first(msg);
    if (!fid) {
        throw std::runtime_error("null id frame");
    }
    int topic = *(int*)zframe_data(fid);

    zframe_t* pay = zmsg_next(msg);
    if (!pay) {
        throw std::runtime_error("null payload frame");
    }
    tps.ParseFromArray(zframe_data(pay), zframe_size(pay));

    zmsg_destroy(&msg);
    return true;
}

