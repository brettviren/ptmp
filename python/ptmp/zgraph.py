#!/usr/bin/env python3

import json
import _jsonnet as jsonnet

def load(fname):
    if fname.endswith(".jsonnet"):
        text = jsonnet.evaluate_file(fname)
    elif fname.endswith(".json"):
        text = open(fname).read()
    else:
        return
    return json.loads(text)


def dot(g):
    lines = ['graph "%s" {' % g['name']]
    lines += ['rankdir=LR']
    node_lines = ['node[shape=box]']
    for node in g['nodes']:
        node_lines += ['%s' % node['name']]
        
    port_lines = ['node[shape=circle]']
    edge_lines = ['edge[style=solid]']
    for node in g['nodes']:
        for port in node['ports']:
            node_name = node['name']
            port_name = '%s_%s' % (node_name, port['name'])
            line = '"%s"[label="%s"]' % (port_name, port['type'])
            port_lines.append(line)
            edge_lines.append('"%s"--"%s"' % (node_name, port_name))

    addr_lines = ['node[shape=ellipse]']
    att_lines = ['edge[style=solid]']
    for att in g['attachments']:
        node_name = att['node']
        port_name = '%s_%s' % (node_name, att['port']['name'])
        addr_name = att['addr']
        addr_lines.append('"%s"' % addr_name)
        att_lines += ['"%s" -- "%s"[label="%s"]' % (port_name, addr_name, att['bc'])]

    dottext = '\n\t'.join(lines + node_lines + port_lines + addr_lines + edge_lines + att_lines)
    dottext += '\n}\n'
    return dottext
