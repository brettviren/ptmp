/**
Generate ptmper configuration to replay FELIX_BR files.
*/

// Assume conventional file names but need to know APA number and data
// directory to generate them. 
local context = std.extVar("context");

local all_params = import "params.jsonnet";
local params = all_params[context];
local ptmp = import "ptmp.jsonnet";

local make_apa = function(apa) {

    local inproc = function(link) "inproc://czmqat-replay-apa%d-link%d" % [apa, link],

    local make_czmqat = function(link) ptmp.czmqat(
        "czmqat-apa%d-link%d"%[apa,link],
        osocket = ptmp.socket('bind','push',inproc(link)),
        cfg = {ifile: params.dumpfiles[apa][link]}
    ),
    local czmqats = [make_czmqat(link) for link in std.range(0,9)],

    local make_replay = function(link) ptmp.replay(
        'replay-apa%d-link%d'%[apa,link],
        isocket = ptmp.socket('connect','pull',inproc(link)),
        osocket = ptmp.socket('bind','pub', params.addresses.tps[apa][link])
        // default cfg is used.
    ),
    local replays = [make_replay(link) for link in std.range(0,9)],
    
    res: czmqats + replays
}.res;

{
    ["fileplay-apa%d.json"%apa]: { proxies: make_apa(apa), }
    for apa in params.apas
}

        
