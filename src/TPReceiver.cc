#include "ptmp/api.h"
#include "ptmp/internals.h"


ptmp::TPReceiver::TPReceiver(const std::string& config)
    : m_sock(new ptmp::internals::Socket(config))
{
}

ptmp::TPReceiver::~TPReceiver()
{
    delete m_sock;
    m_sock = 0;
}

bool ptmp::TPReceiver::operator()(ptmp::data::TPSet& tps, int toms)
{
    zmsg_t* msg = m_sock->msg(toms);
    if (!msg) {
        return false;
    }
    ptmp::internals::recv(&msg, tps);
    return true;
}

