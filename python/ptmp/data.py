from .ptmp_pb2 import *

class MsgV0(object):
    def __init__(self, frames):
        self.tpset = TPSet()
        self.tpset.ParseFromString(frames[1])
    @property
    def tstart(self):
        return self.tpset.tstart
    @property
    def created(self):
        return self.tpset.created
    @property
    def count(self):
        return self.tpset.count
    @property
    def ntps(self):
        return len(self.tpset.tps)

def intern_message(frames):
    if len(frames[0]) != 4:
        raise ValueError("bad size for frame 0: %d" % len(frames[0]))
    msgid = int.from_bytes(frames[0], 'big')
    msg_classes = [MsgV0]
    msg_class = msg_classes[msgid] # may throw index error
    return msg_class(frames)

