import numpy
import matplotlib.pyplot as plt
from matplotlib.colors import LogNorm

class ChanVsTime(object):
    def __init__(self, tr, cr, time_bin):
        self.tr = list(tr)
        self.cr = list(cr)
        self.called = 0;
        self.ntbins = int((self.tr[1] - self.tr[0])/time_bin)
        self.nchans = self.cr[1] - self.cr[0]
        self.arr = numpy.zeros((self.ntbins, self.nchans))

    def tbin(self, t):
        return int(self.ntbins*(t-self.tr[0])/(self.tr[1]-self.tr[0]))
    def tclamp(self, ibin):
        ibin = max(ibin, 0)
        ibin = min(ibin, self.ntbins-1)
        return ibin
        
    def cbin(self, c):
        return c - self.cr[0]
    def cclamp(self, ibin):
        ibin = max(ibin, 0)
        ibin = min(ibin, self.nchans-1)
        return ibin


    def __call__(self, tpset):
        if self.tr[0] == 0:
            self.tr[0] += tpset.tstart
            self.tr[1] += tpset.tstart
        if tpset.tstart < self.tr[0]:
            return True;
        if tpset.tstart > self.tr[1]:
            print ("reached end: %d" % tpset.tstart)
            return False

        for tp in tpset.tps:
            itbeg = self.tbin(tp.tstart)
            itend = self.tbin(tp.tstart+tp.tspan)
            nt = itend-itbeg
            if nt <= 0:
                print ("skipping backwards tp: %s"%str(tp))
                continue                
            val = tp.adcsum/nt
            itbeg = self.tclamp(itbeg);
            itend = self.tclamp(itend);
            ichan = self.cclamp(self.cbin(tp.channel))
            self.arr[itbeg:itend,ichan] += val
        self.called += 1
        return True

    def output(self, outputfile):
        cmap = plt.get_cmap('gist_rainbow')
        cmap.set_bad(color='white')
        arr = numpy.ma.masked_where(self.arr<=1, self.arr)
        #arr = self.arr

        # Transpose to get chan vs time
        arr = arr.T
        extent = [0, self.tr[1]-self.tr[0], self.cr[1], self.cr[0]]
        print (extent)
        plt.imshow(arr, aspect='auto', 
                   #norm=LogNorm(),
                   extent=extent,
                   interpolation='none', cmap=cmap,
        )
        plt.savefig(outputfile)
            
