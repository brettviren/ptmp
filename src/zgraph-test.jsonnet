local g = import "zgraph.jsonnet";
local a = g.node('A',ports=[g.port('p1','PUSH')]);
local b = g.node('B',ports=[g.port('p1','PULL')]);
local c = g.node('C',ports=[g.port('p1','PULL')]);

local addr1 = "tcp://127.0.0.1:12345";
local addr2 = "ipc://foo.ipc";

local atts = [
    g.attach(a.name, a.ports[0], addr1, 'bind'),
    g.attach(a.name, a.ports[0], addr2, 'connect'),
    g.attach(b.name, b.ports[0], addr1, 'connect'),        
    g.attach(c.name, c.ports[0], addr2, 'bind'),        
];

g.graph("zgraph-test", [a,b,c], atts)

