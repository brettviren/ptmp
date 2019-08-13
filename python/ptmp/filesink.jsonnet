/**
This generates configuration for a simple sink.
*/
local context = std.extVar('context');

local all_params = import "params.jsonnet";
local params = all_params[context];

local ptmp = import "ptmp.jsonnet";


// The final output address and file
local input = std.extVar("input");
local output = std.extVar("output");
local ptmp = import "ptmp.jsonnet";

local tp_addrs = std.prune(std.flattenArrays(params.addresses.tps));
local tc_addrs = std.prune(params.addresses.tc);


{
    "sink-tps.json": {
        name: "tp-sink",
        proxies: [ptmp.czmqat("sink-tps",
                              isocket = ptmp.socket('connect','sub', tp_addrs),
                              cfg={name:"tp-sink-czmqat",ofile:params.outfiles.tps})],
    },
    "sink-tcs.json": {
        name: "tc-sink",
        proxies: [ptmp.czmqat("sink-tcs",
                              isocket = ptmp.socket('connect','sub', tc_addrs),
                              cfg={name:"tc-sink-czmqat",ofile:params.outfiles.tcs})],
    },
    "sink-tds.json": {
        name: "td-sink",
        proxies: [ptmp.czmqat("sink-tds",
                              isocket = ptmp.socket('connect','sub', params.addresses.td),
                              cfg={name:"tp-sink-czmqat",ofile:params.outfiles.tds})],
    },

}
