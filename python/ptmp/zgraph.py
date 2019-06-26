#!/usr/bin/env python3

import os
import json
import _jsonnet as jsonnet

def load(fname):
    if fname.endswith(".jsonnet"):
        jpathdir = os.path.dirname(__file__)
        text = jsonnet.evaluate_file(fname, jpathdir=jpathdir)
    elif fname.endswith(".json"):
        text = open(fname).read()
    else:
        return
    return json.loads(text)



def dot(nodes):
    'Emit dot text corresponding to a list of zgraph nodes'
    lines = ['graph "zgra" {']
    lines += ['rankdir=LR']

    node_nodes = ['// node nodes', 'node[shape=box]']
    port_nodes = ['// port nodes', 'node[shape=circle]']
    atts_nodes = ['// atts nodes', 'node[shape=ellipse]']
    np_edges = ['edge[style=solid]']
    pa_edges = ['edge[style=dotted]']

    for node in nodes:
        nn = node['name']
        node_nodes += ['"%s"' % nn]
        for port in node['ports']:
            pn = port['name']
            pt = port['type']

            pnn = '%s_%s' % (nn, pn)

            port_nodes.append('"%s"[label="%s"]' % (pnn, pt))
            np_edges.append('"%s" -- "%s"[label="%s"]' % (nn, pnn, pn))

            for bc, addr in port['atts']:
                atts_nodes.append('"%s"' % addr)
                pa_edges.append('"%s" -- "%s"[label="%s"]' %(pnn, addr, bc))

    lines += node_nodes + port_nodes + atts_nodes + np_edges + pa_edges
    dottext = '\n\t'.join(lines)
    dottext += '\n}\n'
    return dottext


import click

@click.group()
def cli():
    pass


@cli.command('procfile')
@click.option('-o','--output', default="/dev/stdout", help="Output file or stdout")
@click.argument('infile')
def procfile(output, infile):
    'Emit a procfile for the given graph'
    nodes = load(infile)
    lines = list()
    for node in nodes:

        data = node['data']
        if data['app_type'] == 'subprocess':
            cmdline = data['cmdline'].format(ports=node['ports'], **data)
            lines.append("%s: %s" % (node['name'], cmdline))
    open(output, "w").write("\n".join(lines))

@cli.command('dotify')
@click.option('-o','--output', default="/dev/stdout", help="Output file or stdout")
@click.argument('infile')
def dotify(output, infile):
    'Emit a graphviz dot file for the given graph'
    dat = load(infile)
    open(output,"w").write(dot(dat))

def main():
    cli()

if '__main__' == __name__:
    main()

    
    
    
