#!/usr/bin/env python3
import zmq
import json
import time

context = zmq.Context()

isock = context.socket(zmq.SUB)
osock = context.socket(zmq.STREAM)

# hard code for now
isock.connect("tcp://127.0.0.1:6665")
isock.setsockopt(zmq.SUBSCRIBE, b"")
osock.connect("tcp://127.0.0.1:2003")
oid = osock.identity
print ("stream socket id: %s", str(oid))

json_message_type = 0x4a534f4e

while True:
    dat = isock.recv_multipart()
    msg_type = int(dat[0])
    if msg_type != json_message_type:
        print("Uknown message type")
        continue;

    jdat = json.loads(dat[1])
    if (jdat is None):
        print("Got null payload, sender his still there.  Hi sender!")
        continue
            
    now = time.time()
    secs = int(now)

    #  (m_fiber_no << 16) | (m_slot_no << 8) | m_crate_no
    for detid, stats in jdat.items():
        detid = int(detid,16)
        fiber = (detid&0xffff0000) >> 16
        slot = (detid&0x0000ff00) >> 8
        crate = detid&0x000000ff
        base = "ptmp.tpstats.c%d.s%d.f%d" % (crate, slot, fiber)
        first = True
        for key,val in stats.items():
            msg = "%s.%s %s %s\n" % (base, key, val, secs)
            osock.send(oid, flags=zmq.SNDMORE)
            osock.send_string(msg)
            if first:
                print (msg)
                first=False
