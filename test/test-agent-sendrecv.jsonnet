// compile this to a JSON to give to ptmp-agent to implement check_sendrecv.

local actor(funcname, sock) = {
    "function": funcname,
    sockets : [sock],
};

local socket(stype, bc, endpoint) = {
    type: stype,
    [bc]: [endpoint],
};

local config(stypes, endpoint) =  {
    plugins: ["ptmp"],          // actually, redundant
    actors: [
        actor('ptmp_testing_recver', socket(stypes[1], 'bind', endpoint)),
        actor('ptmp_testing_sender', socket(stypes[0], 'connect', endpoint)),
   ],
};

local configs = [
    {stypes:stypes, trans:ep[0], addr:ep[1]},
    for stypes in [['PAIR','PAIR'], ['PUB','SUB'], ['PUSH','PULL']]
    for ep in [
        ['inproc','//test-agent'],
        ['ipc','//test-agent.ipc'],
        ['tcp','//127.0.0.1:23456']
    ]];

{
    ["test-%s-%s-%s.json"%[
        std.asciiLower(one.stypes[0]),
        std.asciiLower(one.stypes[1]),
        one.trans]
    ]: config(one.stypes, one.trans+one.addr),
    for one in configs
}
