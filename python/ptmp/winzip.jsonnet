local ptmp = import "ptmp.jsonnet";
local input=std.extVar('input');
local output=std.extVar('output');
local detid=std.extVar('detid');

local pubsub_hwm=10000;

// window 
local tspan = 50/0.02;          // window span in data time
local tbuf = 5000/0.02;         // turn around buffer in data time

// zipper
local sync_time = 10;           // real time in ms to wait for tardy input streams

local nin = std.length(input);
local iota = std.range(0, nin-1);

local inprocs = ["inproc://thread%02d" % ind for ind in iota];

local windows = [ptmp.window('window-%02d'%ind,
                             isocket = ptmp.socket('connect', 'sub', input[ind], hwm=pubsub_hwm),
                             osocket = ptmp.socket('bind', 'push', inprocs[ind]),
                             cfg = {tspan:tspan, tbuf:tbuf, detid:detid, toffset:0})
                 for ind in iota];

local zipper = ptmp.zipper("zipper",
                           isocket = ptmp.socket('connect','pull', inprocs),
                           osocket = ptmp.socket('bind','pub', output, hwm=pubsub_hwm),
                           cfg = {sync_time:sync_time, tardy_policy:"drop"});

{
    
    proxies: windows + [zipper],
}
