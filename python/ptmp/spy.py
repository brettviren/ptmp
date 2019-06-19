#!/usr/bin/env python3
'''
ptmpy is a Python-based command line interface to PTMP
'''
import time
import click
import numpy

from .helpers import *
from .data import *

@click.group()
@click.pass_context
def cli(ctx):
    pass

# Context manager which does the spying for timing
class spy_timing(object):
    def __init__(self, output):
        self._output = output
        self._arrs = dict()
    def arr(self, num):
        return self._arrs.get("tap%d"%num, numpy.array(([0]*5,), dtype="int64"))

    def __call__(self, msg, tapnum):
        # append info from one message
        arr = self.arr(tapnum)
        num = msg.count
        t,c = msg.tstart, msg.created
        n = time.time()*1000000
        ntps = msg.ntps
        self._arrs["tap%d"%tapnum] = numpy.vstack((arr, (num,t,c,n,ntps)))

    def __enter__(self):
        return self

    def __exit__(self, type, value, traceback):
        print ("saving %d arrays to %s:" % (len(self._arrs), self._output))
        for name, arr in self._arrs.items():
            print("\t%s: %s" % (name, arr.shape))

        numpy.savez(self._output, **self._arrs)
        pass

@cli.command("tap")
@click.option('-o', '--output', default="spy.npz",
              help="output file holding plots")
@click.option('-s', '--spy-method', default="timing",
              help="the spy method")
@click.argument("socket-spec-pairs",nargs=-1)
def tap(output, spy_method, socket_spec_pairs):
    '''
    Spy on messages by tapping into their streams.  Taps are defined
    as pairs of socket-spec-pairs.  A socket spec is of the form:

    [<bind|connect>,]<TYPE>,<ADDRESS>[,<ADDRESS>]

    A pair of socket specs are delimited with a ";"

    <IN>;<OUT>

    Note, you likely must quote socket spec pairs to protect the ";"
    from being interpreted by your shell.

    Messages are sent from <IN> to <OUT> and along the way interogated
    by the spy method.

    The results of the spy method are collected and output into the
    given file.
    '''
    taptype="PUSH"
    spytype="PULL"

    import zmq
    from multiprocessing import Process

    ztx = zmq.Context()
    poller = zmq.Poller()
    taps = dict()
    procs = list()
    for tapnum, ssp in enumerate(socket_spec_pairs):
        #tapep = "inproc://tap%d"%tapnum
        tapep = "tcp://127.0.0.1:777%d"%tapnum
        spy_sock = ztx.socket(socket_type_names.index(spytype))
        spy_sock.bind(tapep)
        taps[spy_sock] = tapnum
        poller.register(spy_sock, zmq.POLLIN)

        sss = [parse_socket_spec(ss) for ss in ssp.split(";")]
        sss.append(parse_socket_spec("connect,%s,%s"%(taptype, tapep)))
        print ('making proxy %d: "%s"' % (tapnum, ssp))
        p = Process(target=lambda : make_proxy(*sss))
        p.start()
        procs.append(p)

    time.sleep(1)
    print ("made %d taps" % len(procs))
        
    count = 1000

    spy_class = eval("spy_%s" % spy_method)
    with spy_class(output) as spier:
        assert(spier)
        print ("starting to spy")
        while count:
            #count -= 1
            for sock, evt in poller.poll():
                #print ("got event %s, %d" % (evt, count))
                if evt != zmq.POLLIN:
                    continue
                tapnum = taps[sock]
                frames = sock.recv_multipart()
                if not frames:
                    print("null frames")
                    break
                spier(intern_message(frames), tapnum)
            
            

def main():
    cli()

if '__main__' == __name__:
    main()

    
