#!/usr/bin/env python3
'''
Make plots with files dumped from /proc/<pid>/tastk/<taskid>/stat
'''
import numpy
from collections import namedtuple, defaultdict

# List columns starting with "state" which is column 3 in proc(5).
# Note, the "state" column is stored as its ASCII numerical value
# using ord().  Use chr() to restore to a letter.
#   proc numbers:3     4    5    6       7      8     9     10     11      12     13
proc_stat_names ="state ppid pgrp session tty_nr tpgid flags minflt cminflt majflt cmajflt "
#   proc numbers:14     15    16     17     18       19   20          21          22        23
proc_stat_names+="utime stime cutime cstime priority nice num_threads itrealvalue starttime vsize "
proc_stat_names+="rss rsslim startcode endcode startstack kstkesp kstkeip signal blocked sigignore "
proc_stat_names+="sigcatch "
proc_stat_names+="wchan nswap cnswap exit_signal processor rt_priority policy delayacct_blkio_ticks "
proc_stat_names+="guest_time cguest_time start_data end_data start_brk arg_start arg_end env_start env_end exit_code"
proc_stat_names = proc_stat_names.split()
assert(len(proc_stat_names) == 52 - 2)
# Add sample time extension, will be 0 if not prepended
proc_stat_names += ["sample_time"]

proc_stat_name2column={c:proc_stat_names.index(c) for c in proc_stat_names}

class ProcData(object):

    def __init__(self):
        self.pid=-1             # process id
        self.tid=-1             # task/thread id
        self.name=""
        self.data=numpy.ndarray((0,len(proc_stat_names)), dtype=numpy.int64)

    def __call__(self, line):
        '''
        Load a line of data.  

        Note, caller MUST take care to call on lines from same pid/tid if they are to be kept distinct.
        '''
        parts = line.strip().split()
        sample_time = 0
        if parts[3].startswith('('): # sample_time process_id [proc(5) values]
            sample_time = int(parts[0])
            self.pid = int(parts[1])
            self.tid = int(parts[2])
            self.name = parts[3]
            parts = parts[4:]

        elif parts[2].startswith('('): # process_id [proc(5) values]
            self.pid = int(parts[0])
            self.tid = int(parts[1])
            self.name = parts[2]
            parts = parts[3:]

        elif parts[1].startswith('('): # [proc(5) values]
            self.pid = self.tid = int(parts[0])
            self.name = parts[1]
            parts = parts[2:]

        else:
            return

        if len(self.name)>2:
            self.name = self.name[1:-1]  # strip off ()'s
        # convert state to ASCII number
        parts[0] = ord(parts[0])
        parts = list(map(int, parts))
        parts.append(sample_time)

        self.data = numpy.append(self.data, [parts], axis=0)

    def __getattr__(self, attr):
        ind = proc_stat_name2column[attr]
        return self.data[:,ind]


def pidskey(line):
    parts = line.strip().split()
    if parts[3].startswith('('):
        return (int(parts[1]), int(parts[2]))
    if parts[2].startswith('('):
        return (int(parts[0]), int(parts[1]))
    if parts[1].startswith('('):
        return (int(parts[0]), int(parts[0]))
    return

def load(dumpfile):
    '''
    Load a file of stat lines and return list of ProcData objects.
    '''
    res = defaultdict(ProcData)

    for line in open(dumpfile).readlines():
        key = pidskey(line)
        it = res[key]
        it(line)
    return res.values()
        
    
def pd_by_pid(pds):
    'Collect ProcData objects by process ID, return dictionary pid->list(ProcData objects)'
    ret = defaultdict(list)
    for k,v in dat.items():
        ret[k[0]].append(v)
    return ret
    
def main_pid(pds):
    'Give a list of ProcData, return only those representing "main" thread'
    return [pd for pd in pds if pd.pid == pd.tid]


def cpu_usage(pd):
    'Return tuple of arrays (t in seconds, per-sample CPU usage in %)'
    # note: this implicitly assumes pd.*time are in units of ticks with 100 ticks per second
    hz=100                      # fixme: assumption

    rt = pd.utime + pd.stime
    # include children
    # rt += pd.cutime + pd.cstime 
    rt /= hz
    dt = pd.sample_time[1:] - pd.sample_time[:-1]
    t = pd.sample_time[1:] - pd.sample_time[0]
    drt = rt[1:] - rt[:-1]
    return (t, 100.0*drt/dt)

#######
# cli #
#######

import click
import matplotlib.pyplot as plt

@click.group()
def cli():
    pass

@cli.command('kitchen-sink')
@click.option('-o','--output', help="Output file")
@click.argument('monlog')
def kitchen_sink(output, monlog):
    'Plot a bunch of stuff'
    pds = main_pid(load(monlog))

    for pd in pds:
        x,y = cpu_usage(pd)
        plt.plot(x,y, label=pd.name)
    plt.ylabel("CPU%")
    plt.xlabel("wall clock [s]")
    plt.legend()
    plt.savefig(output)

    

def main():
    cli()

if '__main__' == __name__:
    main()

    
    
    
