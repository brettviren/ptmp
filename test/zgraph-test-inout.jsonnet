local g = import "zgraph.jsonnet";

local src = g.fsource("hitsrc1", "FELIX_BR_601.dump", 
                      g.port('out', 'PUSH', [['bind','tcp://127.0.0.1:5678']]));
[src]
