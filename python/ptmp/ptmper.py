#!/usr/bin/env python3
'''
Do things related to the "ptmper" application
'''

from . import jsonnet

def dotify_port(node_name, node_data, io):
    lines = list()
    port = node_data.get(io, None)
    if not port:
        return lines
    sock = port["socket"]

    pn = '%s_%s%s' % (node_name, io, node_data.get('id',""))
    # socket
    lines.append('"%s"[shape=circle,label="%s"]' % (pn, sock["type"]))
    lines.append('"%s" -- "%s"[style=solid]' % (node_name, pn))
    
    # addresses
    for bc in ["bind", "connect"]:
        if not bc in sock:
            continue
        for addr in sock[bc]:
            lines.append('"%s"[shape=ellipse]' % addr)
            lines.append('"%s" -- "%s"[style=dotted,label="%s"]' % (pn, addr, bc))

    return lines

def dotify_nominal(node):
    lines = list()
    name = node['name']
    lines.append('"%s"[shape=box]' % name)
    lines += dotify_port(name, node['data'], "input")
    lines += dotify_port(name, node['data'], "output")
    return lines
    
def dotify_monitor(node):
    lines = list()
    nn = node['name']
    nd = node['data']
    lines.append(r'"%s"[shape=box,label="%s\n%s"]' % (nn, nn, nd['filename']))
    for tap in nd['taps']:
        tapname = '%s_tap%s' % (nn, tap['id'])
        lines.append('"%s"[shape=box]' % tapname)
        lines.append('"%s" -- "%s"[style=dashed]' % (nn, tapname))
        
        lines += dotify_port(tapname, tap, 'input')
        lines += dotify_port(tapname, tap, 'output')
    return lines

def dot(nodes):
    'Emit dot text corresponding to a list of zgraph nodes'
    lines = ['graph "ptmper" {']
    lines += ['rankdir=LR']

    for node in nodes:
        if node["type"] == "monitor":
            lines += dotify_monitor(node)
            continue
        lines += dotify_nominal(node)
    dottext = '\n\t'.join(lines)
    dottext += '\n}\n'
    return dottext


def load_file(infile, ext_str=None, ext_code=None):
    '''
    Load jsonnet or json file
    '''
    kwds=dict()
    if ext_str:
        kwds['ext_vars'] = {k:v for k,v in [s.split('=') for s in ext_str]}
    if ext_code:
        kwds['ext_codes'] = {k:v for k,v in [s.split('=') for s in ext_code]}

    return jsonnet.load(infile,**kwds)

import click


@click.group()
def cli():
    pass


@cli.command('dotify')
@click.option('-o','--output', default="/dev/stdout", help="Output file or stdout")
@click.option('-V','--ext-str', default=None, type=str, help="Jsonnet external var", multiple=True)
@click.option('-C','--ext-code', default=None, type=str, help="Jsonnet external code", multiple=True)
@click.argument('infile')
def dotify(output, ext_str, ext_code, infile):
    '''
    Emit a graphviz dot file for the given graph
    '''

    dat = load_file(infile, ext_str, ext_code)

    open(output,"w").write(dot(dat['proxies']))

import pandas as pd
import numpy as np
import matplotlib.pyplot as plt
from matplotlib.backends.backend_pdf import PdfPages

@cli.command('monplots')
@click.option('-o','--output', default="/dev/stdout", help="Output file or stdout")
@click.argument('monfile')
def monplots(output, monfile):
    mon = pd.read_csv(monfile, delim_whitespace=True)
    bytap = {n:mon[mon.tapid==n] for n in set(mon.tapid)} 

    dat = dict()
    for tid,mon in bytap.items():
        now = 1.0e-6*np.array(mon.now)
        tstart = 20.0e-9*np.array(mon.tstart)
        created = 1.0e-6*np.array(mon.created)
        nrecv = np.array(mon.nrecv)
        count = np.array(mon["count"])

        # these all have 1 less than mon and times are in units of seconds
        dat[tid] = dict(
            now = now[1:] - now[0],
            tstart = tstart[1:] - tstart[0],
            created = created[1:] - now[0],
            dnow = now[1:] - now[:-1],
            dtstart = tstart[1:] - tstart[:-1],
            dnrecv = nrecv[1:] - nrecv[:-1],
            dcount = count[1:] - count[:-1],
            nrecv = nrecv[1:] - nrecv[0],
            count = count[1:] - count[0],
        )
        
    popts = dict(linewidth=0.5)
    #fopts = dict(figsize=(30,10))
    fopts = dict()

    # this depends on what span of files were replayed
    groups = [("replay",range(200,205)),
              #("window",range(400,405)),
              #("sorted",[600]),
    ]

    with PdfPages(output) as pdf:


        for group_name, group in groups:

            def key(d):
                return d[key]


            def one(xkey, ykey, xlab, ylab, lab="%d", tit = "%s"):
                fig, ax = plt.subplots(1,1, **fopts)
                ax.set_title(tit % group_name)
                ax.set_xlabel(xlab)
                ax.set_ylabel(ylab)
                for tapid in group:
                    d = dat[tapid]
                    ax.plot(xkey(d), ykey(d), label=lab%tapid, **popts)
                ax.legend(prop=dict(size=6))
                pdf.savefig(fig)

            one(lambda d: d["tstart"], lambda d: d["count"], "data clock [s]", "count")
            one(lambda d: d["now"], lambda d: d["nrecv"], "wall clock [s]", "nrecv")
            one(lambda d: d["now"], lambda d: d["tstart"], "wall clock [s]", "data clock [s]")
            one(lambda d: d["now"], lambda d: d["now"]-d["tstart"], "wall clock [s]", "now - data clock [s]")
            one(lambda d: d["now"], lambda d: d["now"]-d["created"], "wall clock [s]", "now - created [s]")
            one(lambda d: d["now"], lambda d: d["nrecv"]-d["count"], "wall clock [s]", "nrecv - count")
            one(lambda d: d["now"], lambda d: d["dnrecv"]/d["dnow"], "wall clock [s]", "nrecv rate [Hz]")
            one(lambda d: d["tstart"], lambda d: d["dcount"]/d["dtstart"], "data clock [s]", "count rate [Hz]")
                         
            
            

def main():
    cli()

if '__main__' == __name__:
    main()

    
    
    
