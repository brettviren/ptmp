local g = import "zgraph.jsonnet";

local addr1s = ["tcp://127.0.0.1:"+p for p in [5678,5679]];
local addr2 = "ipc://foo.ipc";

local a = g.node('A', [ g.port('out', 'PUSH',
                               [['bind', addr1s[0]], ['connect', addr2]]) ]);
local b = g.node('B', [ g.port('in',  'PULL',
                               [['connect',addr1s[1]]])]);
local c = g.node('C', [ g.port('in',  'PULL',
                               [['bind',addr2]])]);

local make_tap = function(name, addrs, types=['PULL','PUSH']) g.node(
    name, [g.port('in',  types[0], [['connect',addrs[0]]]),
           g.port('out', types[1], [['bind'   ,addrs[1]]])]);
                  

local tap = make_tap('tap', addr1s);

[a,b,c,tap]
