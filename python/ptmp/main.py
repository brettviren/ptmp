
socket_type_names = "PAIR PUB SUB REQ REP DEALER ROUTER PULL PUSH XPUB XSUB STREAM".split()

import click

@click.group()
@click.option('-n','--number', default=0, type=int, required=False,
              help="Number of messages to accept before quitting, default is run forever")
@click.option('-e','--socket-endpoints', required=True, type=str, multiple=True,
              help="Socket endpoint addresss in standard ZeroMQ format")
@click.option('-p','--socket-pattern', required=True, type=click.Choice(socket_type_names),
              help="ZeroMQ socket type")
@click.option('-a','--socket-attachment', required=True, type=click.Choice(["bind","connect"]),
              help="Whether socket should bind() or connect()")
@click.option('-s','--subscription', required=False, default="",
              help="Subscription if socket type is SUB")
@click.pass_context
def cli(ctx, number, socket_endpoints, socket_pattern, socket_attachment, subscription):
    import zmq

    ztx = zmq.Context()
    stype = getattr(zmq, socket_pattern)
    socket = ztx.socket(stype)
    satt = getattr(socket, socket_attachment)
    for endpoint in socket_endpoints:
        satt(endpoint)
    ctx.obj.update(ztx=ztx, socket=socket, number=number)
    pass

def spin(tocall, obj):
    'Call tocall inside loop in a standard way.  tocall(tpsent) -> bool.  if false, exit'
    from ptmp.ptmp_pb2 import TPSet
    tpset = TPSet()

    socket = obj["socket"]
    number = obj["number"]

    count = 0
    while True:
        if number > 0 and count == number:
            print ("reached count %d" % count)
            break;
        count += 1

        (topic, msg) = socket.recv_multipart()
        if len(topic) == 4 and 0 == int.from_bytes(topic, 'big'):
            pass
        else:
            print ("uknown first frame: %s" % str(topic))
        tpset.ParseFromString(msg)

        ok = tocall(tpset)
        if not ok:
            print ("spin cycle over")
            break;
        


@cli.command("dump-messages")
@click.pass_context
def dump_messages(ctx):

    def dumper(tpset):

        tbegs = [tp.tstart-tpset.tstart for tp in tpset.tps]
        tends = [tp.tstart+tp.tspan-tpset.tstart for tp in tpset.tps]
        tspans =[tp.tspan for tp in tpset.tps]
        chans = [tp.channel for tp in tpset.tps]
        
        min_tbeg = min(tbegs)
        max_tbeg = max(tbegs)
        min_tend = min(tends)
        max_tend = max(tends)
        
        print ("0x%x %ld %d #%d n:%4d tbeg=[%ld,%ld] tend=[%ld,%ld] <%ld> <%ld> ch=[%d,%d]" %
               (tpset.detid, tpset.tstart, tpset.tspan, tpset.count, len(tpset.tps),
                min_tbeg, max_tbeg, min_tend, max_tend, max_tend-min_tbeg,
                max(tspans),
                min(chans), max(chans)
               ))
        return True
    spin(dumper, ctx.obj)

    pass

@cli.command("draw-tps")
@click.option('-c', '--channel-range', nargs=2, type=int, required=True,
              help="the half-open channel range")
@click.option('-t', '--time-range', nargs=2, type=int, required=True,
              help="the time range")
@click.option('-T', '--time-bin', type=int, default=1,
              help="number of time ticks per bin")
@click.argument("outputfile")
@click.pass_context
def draw_tps(ctx, channel_range, time_range, time_bin, outputfile):
    from .plotters import ChanVsTime

    drawer = ChanVsTime(time_range, channel_range, time_bin)
    spin(drawer, ctx.obj)
    print ("called %d" % drawer.called)
    drawer.output(outputfile)


def main():
    cli(obj={})

if '__main__' == __name__:
    main()

    
