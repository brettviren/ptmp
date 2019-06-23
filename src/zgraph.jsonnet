{
    // Make a named socket port of a given type ('PUSH', 'SUB', etc).
    port(name, type):: {
        name: name, type: type,
    },

    // Create a ported graph node.  The name must be unique.  Provide
    // socket ports which are exposed are included as ports.  If the
    // pnode is composite then give its internal structure as a
    // subgraph.
    node(name, ports=[], nodes=[], edges=[], subgraph={}, data={}):: {
        name: name,
        ports: ports,
        data: data,
        subgraph: subgraph,
    },

    // Attach a node's port to an address via bind or connect.
    attach(node_name, node_port, addr, bc='connect', data={}):: {
        node: node_name,
        port: node_port,
        addr: addr,
        bc: bc,
        data: data,
    },
    bind(node_name, node_port, addr, data={}):: self.attach(node_name, node_port, addr, 'bind', data),
    connect(node_name, node_port, addr, data={}):: self.attach(node_name, node_port, addr, 'connect', data),

    // Make a graph (or a subgraph).
    graph(name, nodes=[], attachments=[], data={}):: {
        name: name, nodes: nodes, attachments: attachments, data: data,
    },

}
