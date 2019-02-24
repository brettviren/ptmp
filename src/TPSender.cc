#include "ptmp/api.h"
#include "ptmp/internals.h"

#include <czmq.h>


ptmp::TPSender::TPSender(const std::string& config)
    : m_sock(new ptmp::internals::Socket(config))
{
}

ptmp::TPSender::~TPSender()
{
    delete m_sock;
    m_sock = 0;
}

void ptmp::TPSender::operator()(const data::TPSet& tps)
{
    ptmp::internals::send(m_sock->get(), tps);
}
