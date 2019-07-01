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

def main():
    cli()

if '__main__' == __name__:
    main()

    
    
    
