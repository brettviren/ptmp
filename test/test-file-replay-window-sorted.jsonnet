

// jsonnet -V nmsgs=1000 --ext-src "input=["$(printf '"%s"\n' /data/fast/bviren/ptmp-dumps/2019-06-10/FELIX_BR_6*.dump | paste -sd, )"]" thisfile.jsonnet
local nmsgs = std.parseInt(std.extVar("nmsgs"));
local files = std.extVar("input");
local nfiles = std.length(files);
local file_iota = std.range(0,nfiles-1);

local g = import "zgraph.jsonnet";

local fsource_addr = function(num) 'tcp://127.0.0.1:%d'%(6000+num);
local replay_addr = function(num) 'tcp://127.0.0.1:%d'%(7000+num);
local window_addr = function(num) 'tcp://127.0.0.1:%d'%(8000+num);
local sorted_addr = function(num=0) 'tcp://127.0.0.1:%d'%(9000+num);

local fsources = [
    g.fsource("file%02d"%ind,
              files[ind],
              g.port("src", 'PUSH', [['bind', fsource_addr(ind)]]),
              cliargs="-B 1000 -E 1000")
    for ind in file_iota
    ];

local replays = [
    g.cproxy("replay%02d"%ind,
             [
                 g.port('in', 'PULL', [['connect', fsource_addr(ind)]]),
                 g.port('out', 'PUSH', [['bind', replay_addr(ind)]])
             ],
             cliargs="-c 30")
    for ind in file_iota
    ];

local windows = [
    g.cproxy("window%02d"%ind,
             [
                 g.port('in', 'PULL', [['connect', replay_addr(ind)]]),
                 g.port('out', 'PUSH', [['bind', window_addr(ind)]])
             ],
             cliargs="-c 30")
    for ind in file_iota
    ];

local sorted = g.csorted("sorted", nfiles,
                         [ g.port('in%02d'%ind,'PULL', [['connect', window_addr(ind)]]) for ind in file_iota ]
                         +
                         [ g.port('out', 'PUSH', [['bind', sorted_addr()]]) ],
                         cliargs="-c 30");


local sink = g.csink('sink',
                     g.port('in', 'PULL', [['connect', sorted_addr()]]),
                     cliargs="-n %d"%nmsgs
                    );

fsources + replays + windows + [sorted, sink]
