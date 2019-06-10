
socket_type_names = "PAIR PUB SUB REQ REP DEALER ROUTER PULL PUSH XPUB XSUB STREAM".split()

import click

@click.group()
def cli():
    pass

@cli.command("dump-messages")
@click.option('-e','--socket-endpoints', required=True, type=str, multiple=True,
              help="Socket endpoint addresss in standard ZeroMQ format")
@click.option('-p','--socket-pattern', required=True, type=click.Choice(socket_type_names),
              help="ZeroMQ socket type")
@click.option('-a','--socket-attachment', required=True, type=click.Choice(["bind","connect"]),
              help="Whether socket should bind() or connect()")
@click.option('-s','--subscription', required=False, default="",
              help="Subscription if socket type is SUB")
def dump_messages(socket_endpoints, socket_pattern, socket_attachment, subscription):
    import zmq
    from ptmp.ptmp_pb2 import TPSet

    ctx = zmq.Context()
    stype = getattr(zmq, socket_pattern)
    socket = ctx.socket(stype)
    satt = getattr(socket, socket_attachment)
    for endpoint in socket_endpoints:
        satt(endpoint)

    tpset = TPSet()

    while True:
        (topic, msg) = socket.recv_multipart()
        if len(topic) == 4 and 0 == int.from_bytes(topic, 'big'):
            pass
        else:
            print ("uknown first frame: %s" % str(topic))
        tpset.ParseFromString(msg)

        tbegs = [tp.tstart-tpset.tstart for tp in tpset.tps]
        tends = [tp.tstart+tp.tspan-tpset.tstart for tp in tpset.tps]
        tspans =[tp.tspan for tp in tpset.tps]
        
        min_tbeg = min(tbegs)
        max_tbeg = max(tbegs)
        min_tend = min(tends)
        max_tend = max(tends)
        
        print ("%d 0x%x %ld %d tbeg=[%ld,%ld] tend=[%ld,%ld] <%ld> <%ld>" %
               (tpset.count, tpset.detid, tpset.tstart, tpset.tspan,
                min_tbeg, max_tbeg, min_tend, max_tend, max_tend-min_tbeg,
                max(tspans)
               ))
        
    pass

def main():
    cli(obj=dict())

if '__main__' == __name__:
    main()

    
