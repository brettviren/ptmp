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
        
    
def main_pid(pds):
    'Give a list of ProcData, return only those representing "main" thread'
    return [pd for pd in pds if pd.pid == pd.tid]

def collect_pid(pds):
    'Partition a ProcData into sublists with common pid'
    ret = defaultdict(list)
    for pd in pds:
        ret[pd.pid].append(pd)
    return list(ret.values())


def cpu_usage(pds, hz=100, which="both"):
    '''
    Return tuple of arrays (t in seconds, per-sample CPU usage in %).

    The utime and stime of all pds are summed up
    '''
    # note: this implicitly assumes pd.*time are in same "tick" units
    # tick / hz = seconds.

    cputime = numpy.zeros_like(pds[0].utime)
    for pd in pds:
        if which in "both utime":
            nsamples = pd.utime.size
            cputime[:nsamples] += pd.utime
        if which in "both stime":
            nsamples = pd.stime.size
            cputime[:nsamples] += pd.stime
    dcputime = cputime[1:] - cputime[:-1]

    if numpy.any(dcputime<0):
        print ("%s has negative delta cpu time" % (pds[0].name,))
        dcputime[dcputime<0] = 0


    sample_time = pds[0].sample_time
    dt = sample_time[1:] - sample_time[:-1]
    t = (sample_time[1:] - sample_time[0])/hz
    return (t, 100.0*dcputime/dt)

def rss_usage_kbyte(pd, kb_per_page=4, hz=100):
    '''
    Return tuple of arrays (t in seconds, per-sample RSS usage in
    kByte).
    '''
    rss = pd.rss * kb_per_page
    t = (pd.sample_time[1:] - pd.sample_time[0])/hz
    return (t, rss[1:])
    
#######
# cli #
#######

import click
import matplotlib.pyplot as plt
from matplotlib.backends.backend_pdf import PdfPages


@click.group()
def cli():
    pass

@cli.command('kitchen-sink')
@click.option('-o','--output', help="Output PDF file")
@click.argument('monlog')
def kitchen_sink(output, monlog):
    'Plot a bunch of stuff'
    all_pds = load(monlog)
    pd_sets = collect_pid(all_pds)

    popts=dict(linewidth=0.5)

    prog_groups = ["czmqat", "check_replay", "check_window", "check_sorted check_recv", "ptmp-spy"]

    with PdfPages(output) as pdf:

        for prog in prog_groups:

            for which in "utime stime both".split():

                fig, ax = plt.subplots(1,1)
                for pds in pd_sets:
                    if pds[0].name not in prog:
                        continue
                    x,y = cpu_usage(pds, which=which)
                    ax.plot(x,y, label=pds[0].name, **popts)
                ax.set_ylabel("CPU%% (%s)"%which)
                ax.set_xlabel("wall clock [s]")
                ax.legend(prop=dict(size=6))
                pdf.savefig(fig)

            plt.close('all')

        for prog in prog_groups:

            fig, ax = plt.subplots(1,1)
            for pds in pd_sets:
                if pds[0].name not in prog:
                    continue
                x,y = rss_usage_kbyte(pds[0])
                ax.plot(x,y, label=pds[0].name, **popts)
            ax.set_ylabel("RSS [kB]")
            ax.set_xlabel("wall clock [s]")
            ax.legend(prop=dict(size=6))
            pdf.savefig(fig)

        plt.close('all')

        for prog in prog_groups:
            fig, ax = plt.subplots(1,1)
            for pds in pd_sets:
                if pds[0].name not in prog:
                    continue
                for pd in pds:
                    ax.plot(pd.sample_time-pd.sample_time[0], pd.processor, label=pd.name, **popts)
            ax.set_ylabel("proc num")
            ax.set_xlabel("wall clock [s]")
            ax.legend(prop=dict(size=6))
            pdf.savefig(fig)

        plt.close('all')

def main():
    cli()

if '__main__' == __name__:
    main()

    
    
    
