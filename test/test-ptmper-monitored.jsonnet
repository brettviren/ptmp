local input = std.extVar("input");
local output = std.extVar("output");
local ptmp = import "ptmp.jsonnet";

{
    "test-ptmper-monitored.json" :
    ptmp.ptmper(ptmp.files_sorted_monitored(input, output, nmsgs=200000, rewrite_count=1),
                ttl=30, reprieve=1),
    "test-ptmper-monitored-just-files.json" :
    ptmp.ptmper(ptmp.just_files(input, output, nmsgs=200000),
                ttl=30, reprieve=1),
}

