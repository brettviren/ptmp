/**
This generates configuration for a simple sink.
*/
// The final output address and file
local input = std.extVar("input");
local output = std.extVar("output");
local ptmp = import "ptmp.jsonnet";
{
    "fileplay-sink.json": {
        proxies: [ptmp.czmqat("fileplay-sink",
                              isocket=(ptmp.socket('connect','sub',input)),
                              cfg={ofile:output})],
    },
}
