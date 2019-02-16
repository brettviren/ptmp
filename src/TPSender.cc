#include "ptmp/api.h"
#include "internals.h"

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
    const int topic = 0;

    // fixme: move this to some place more reusable.
    zmsg_t* msg = zmsg_new();
    zframe_t* fid = zframe_new(&topic, sizeof(int));
    zmsg_append(msg, &fid);

    size_t siz = tps.ByteSize();
    zframe_t* pay = zframe_new(NULL, siz);
    tps.SerializeToArray(zframe_data(pay), zframe_size(pay));
    zmsg_append(msg, &pay);

    zmsg_send(&msg, m_sock->get());

    // fixme: check rc and throw
}
