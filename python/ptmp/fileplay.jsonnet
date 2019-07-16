/**
Generate ptmper configuration to replay FELIX_BR files.
*/

// Assume conventional file names but need to know APA number and data
// directory to generate them. 
local datadir = std.extVar("datadir");
local apa = std.parseInt(std.extVar("apa"));
// output address pattern string.  It's %d-interpolated with the link number in [0,9].
local output = std.extVar("output");

// fixme: binds tightly the file naming pattern.  alternative would be
// to provide JSON array of file names as external code, but it's a
// pain to form on the shell command line.
local files = ["%s/FELIX_BR_%d%02d"%[datadir,apa,file] for file in std.range(1,10)];

local inproc = function(link) 'inproc://file-apa%d-link%d' % [apa,link];

local ptmp = import "ptmp.jsonnet";

local make_czmqat = function(link) ptmp.czmqat(
    "czmqat-apa%d-link%d"%[apa,link],
    osocket = ptmp.socket('bind','push',inproc(link)),
    cfg = {ifile: files[link]}
);

local make_replay = function(link) ptmp.replay(
    'replay-apa%d-link%d'%[apa,link],
    isocket = ptmp.socket('connect','pull',inproc(link)),
    osocket = ptmp.socket('bind','pub', output%link),
    // default cfg is used.
);


local czmqats = [make_czmqat(link) for link in std.range(0,9)];
local replays = [make_replay(link) for link in std.range(0,9)];

{
    ["fileplay-apa%d.json"%apa]: {
        proxies: czmqats + replays
    }
}

        
