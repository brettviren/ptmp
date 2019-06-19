from collections import namedtuple


socket_type_names = "PAIR PUB SUB REQ REP DEALER ROUTER PULL PUSH XPUB XSUB STREAM".split()


SockSpec = namedtuple("SockSpec", "bc stype ztype endpoints")

def parse_socket_spec(spec):
    '''
    Create and return a socket in context based on a spec string of the form:

    [<bind|connect>,]<TYPE>,<ADDRESS>[,<ADDRESS>]
    '''
    parts = spec.split(",")
    bc = "connect"
    if parts[0].lower() in ["bind","connect"]:
        bc = parts.pop(0).lower()
    stype = parts.pop(0).upper()
    ztype = socket_type_names.index(stype)
    return SockSpec(bc=bc, stype=stype, ztype=ztype, endpoints=parts)

def proxy_add(p, ss, which="mon"):
    meth = getattr(p, "%s_%s"%(ss.bc, which))
    for ep in ss.endpoints:
        print (meth, ss)
        meth(ep)

def make_proxy(ssin, ssout, ssmon):
    '''
    Make a proxy and return its monitor socket.
    '''
    print("making proxy: %s" % (ssmon,))
    #from zmq.devices.proxydevice import ThreadProxy as Proxy
    #from zmq.devices.proxydevice import ProcessProxy as Proxy
    from zmq.devices.proxydevice import Proxy
    #from zmq.devices.monitoredqueuedevice import MonitoredQueue as Proxy
    p = Proxy(ssin.ztype, ssout.ztype, ssmon.ztype)
    proxy_add(p, ssin, "in")
    proxy_add(p, ssout, "out")
    proxy_add(p, ssmon, "mon")    
    print("starting proxy: %s" % (ssmon,))
    p.start()
    print("finished proxy: %s" % (ssmon,))
