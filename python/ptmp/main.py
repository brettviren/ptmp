
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
    import numpy
    import matplotlib.pyplot as plt
    from matplotlib.colors import LogNorm

    class Drawer(object):
        def __init__(self, tr, cr, time_bin):
            self.tr = list(tr)
            self.cr = list(cr)
            self.called = 0;
            self.ntbins = int((self.tr[1] - self.tr[0])/time_bin)
            self.nchans = self.cr[1] - self.cr[0]
            self.arr = numpy.zeros((self.ntbins, self.nchans))

        def tbin(self, t):
            ibin = int(self.ntbins*(t-self.tr[0])/(self.tr[1]-self.tr[0]))
            ibin = max(ibin, 0)
            ibin = min(ibin, self.ntbins-1)
            return ibin
        def cbin(self, c):
            ibin = c - self.cr[0]
            ibin = max(ibin, 0)
            ibin = min(ibin, self.nchans-1)
            return ibin

        def __call__(self, tpset):
            if self.tr[0] == 0:
                self.tr[0] += tpset.tstart
                self.tr[1] += tpset.tstart
            if tpset.tstart < self.tr[0]:
                return True;
            if tpset.tstart > self.tr[1]:
                print ("reached end: %d" % tpset.tstart)
                return False
        
            for tp in tpset.tps:
                itbeg = self.tbin(tp.tstart)
                itend = self.tbin(tp.tstart+tp.tspan)
                nt = itend-itbeg
                if nt <= 0:
                    continue                
                val = tp.adcsum/nt
                ichan = self.cbin(tp.channel)
                self.arr[itbeg:itend,ichan] += val
            self.called += 1
            return True
        def plot(self, outputfile):
            extent = self.cr + [0, self.tr[1]-self.tr[0]]
            print (extent)
            cmap = plt.get_cmap('gist_rainbow')
            cmap.set_bad(color='white')
            #arr = numpy.ma.masked_where(self.arr<=1, self.arr)
            arr = self.arr
            plt.imshow(arr, aspect='auto', interpolation='none', cmap=cmap,
                       norm=LogNorm(),
                       extent=extent)
            plt.savefig(outputfile)
            
    drawer = Drawer(time_range, channel_range, time_bin)
    spin(drawer, ctx.obj)
    print ("called %d" % drawer.called)
    drawer.plot(outputfile)


def main():
    cli(obj={})

if '__main__' == __name__:
    main()

    
