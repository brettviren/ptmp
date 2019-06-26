#!/usr/bin/env python3

from . import jsonnet

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

@cli.command('procfile')
@click.option('-o','--output', default="/dev/stdout", help="Output file or stdout")
@click.option('-V','--ext-str', default=None, type=str, help="Jsonnet external var", multiple=True)
@click.option('-C','--ext-code', default=None, type=str, help="Jsonnet external code", multiple=True)
@click.argument('infile')
def procfile(output, ext_str, ext_code, infile):
    '''
    Emit a procfile for the given graph
    '''

    nodes = load_file(infile, ext_str, ext_code)

    lines = list()
    for node in nodes:
        data = node['data']
        if data['app_type'] == 'subprocess':
            cmdline = data['cmdline'].format(ports=node['ports'], **data)
            lines.append("%s: %s" % (node['name'], cmdline))

    open(output, "w").write("\n".join(lines))

@cli.command('dotify')
@click.option('-o','--output', default="/dev/stdout", help="Output file or stdout")
@click.option('-V','--ext-str', default=None, type=str, help="Jsonnet external var", multiple=True)
@click.option('-C','--ext-code', default=None, type=str, help="Jsonnet external code", multiple=True)
@click.argument('infile')
def dotify(output, ext_str, ext_code, infile):
    '''
    Emit a graphviz dot file for the given graph
    '''

    nodes = load_file(infile, ext_str, ext_code)

    open(output,"w").write(dot(nodes))

def main():
    cli()

if '__main__' == __name__:
    main()

    
    
    
