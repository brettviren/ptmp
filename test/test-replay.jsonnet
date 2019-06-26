local g = import "zgraph.jsonnet";

local nmsgs=10000;

local addr1 = 'tcp://127.0.0.1:5678';
local addr2 = 'tcp://127.0.0.1:5679';

local source = g.csource("source",
                         g.port("src", 'PUSH', [['bind', addr1]]),
                        cliargs="-B 1000 -E 1000 -n %d" % nmsgs);

local proxy = g.cproxy("replay",
                       [
                           g.port('in', 'PULL', [['connect', addr1]]),
                           g.port('out', 'PUSH', [['bind', addr2]])
                       ],
                      cliargs="-c 30");

local sink = g.csink('sink',
                     g.port('in', 'PULL', [['connect', addr2]]),
                     cliargs="-n %d"%nmsgs
                    );

[source, proxy, sink]
