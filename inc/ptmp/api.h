/** The high level PTMP API
 */

#ifndef PTMP_API_H
#define PTMP_API_H

#include <string>
#include <vector>

#include "ptmp/data.h"
#include "ptmp/internals.h"

namespace ptmp {

    /**
       The PTMP high level API consists of a number of C++ classes
       that provide high level access to sockets or act as miniature
       network applications.  They all take a number of socket
       description objects.  

       Example socket description object:

       ~~~{.json}
       {
          "socket": {
              "bind": [
                  "tcp://127.0.0.1:7609",
                  "tcp://127.0.0.1:7607",
                  "tcp://127.0.0.1:7605",
                  "tcp://127.0.0.1:7603",
                  "tcp://127.0.0.1:7601"
              ],
              "hwm": 1000,
              "type": "PULL"
          }
       }
       ~~~

       Instead or in addition to `bind` a `connect` attribute may be
       given.  Typicaly any socket may `bind` or `connect`, however
       not all PTMP objects may support both simultaneously.
 
     */

    /// An interface to a sending socket
    class TPSender {
        internals::Socket* m_sock;
    public:
        /// Create a TPSender with a configuration string.
        TPSender(const std::string& config);
        ~TPSender();

        /// Send one TPSet.  
        void operator()(const data::TPSet& tps);

        TPSender() =default;
        TPSender(const TPSender&) =delete;
        TPSender& operator=(const TPSender&) =delete;            
    };

    /// An interface to a receiving socket.
    class TPReceiver {
        internals::Socket* m_sock;
    public:
        /// Create a TPReceiver with a configuration string.
        TPReceiver(const std::string& config);
        ~TPReceiver();

        /**
           Receive next TPSet, by filling.  If a timeout occurs, or
           otherwise a break is received, this will return false.  If
           a message is received but is corrupted this will throw
           runtime_error.
        */
        bool operator()(data::TPSet& tps, int timeout_msec=-1);

        TPReceiver() =default;
        TPReceiver(const TPReceiver&) =delete;
        TPReceiver& operator=(const TPReceiver&) =delete;            
    };


    /** Base class for any "free agent" classes.  Such classes are
     * constructed and then simply kept alive as long as needed.  They
     * are expected to do their work in background threads.
     */
    class TPAgent {
    public:
        virtual ~TPAgent();
    };

    /** A factory method for agents */
    TPAgent* agent_factory(const std::string& alias, const std::string& config);


    /**
       A "free agent" that accepts TPSets from N TPSenders on input
       and produces an ordered output stream of their TPSets on
       output.  Synchronizing is done on the "tstart" time of the
       TPSets.
    */
    class TPSorted : public TPAgent {
    public:
        /**
           Create a TPSorted with one config for input sockets and one
           for output.  Note, each output message is sent to all
           output sockets.  The config is two objects each like
           TPSender/TPReceiver held by a key "sender" and "receiver".
        */
        TPSorted(const std::string& config);

        // The TPSorted will run a thread needs to be kept in scope
        // but otherwise doesn't have a serviceable API.  Destroy it
        // when done.
        virtual ~TPSorted();
    private:
        zactor_t* m_actor;
    };

    class TPZipper : public TPAgent {
    public:
        TPZipper(const std::string& config);
        virtual ~TPZipper();
    private:
        zactor_t* m_actor;
    };

    /**
       A "free agent" that accepts TPSets on an input socket and
       "replays" them to an output socket.  The replay is done in a
       pseudo-real-time manner by using the system clock and the
       message "tstart" value to pace their output.  The replayer
       attempts to keep the real time between messages matching the
       differences in subsequent "tstart" values.  If the input feed
       falls behind then a tardy message will be sent immediately on
       receipt which will not maintain the exact pacing.  Although the
       input socket is configurable it is best to use either PULL or
       PAIR unless message loss is preferable to blocking the upstream
       sender.  The replay may be governed by a multiplicative "speed"
       to cause a "fast forward" or a "slow motion" output.
    */
    class TPReplay : public TPAgent {
    public:
        /**
           Create a TPReplay with one config for input sockets and one
           for output.  Note, each output message is sent to all
           output sockets.  The config is two objects each like
           TPSender/TPReceiver held by a key "sender" and "receiver".
           An optional "speed" atribute provides the number of
           hardware data clock ticks per microsecond (eg, default of
           50 for PDSP).
        */

            
        TPReplay(const std::string& config);

        /**
           The TPSorted will run a thread so needs to be kept in scope
           but otherwise doesn't have a serviceable API.  Destroy it
           when done and it will disconnect its sockets.
        */
        virtual ~TPReplay();
    private:
        zactor_t* m_actor;
        
    };

    /// A "free agent" that applies hw clock windowing to a stream of
    /// TPSets at the TP level.
    class TPWindow : public TPAgent {
    public:
        /**
           Create a TPWindower with config covering input and output
           sockets as well as windowing parameters.  Windowing is
           applied to the tstart and tspan values of the TrigPrim in
           the TPSets.  Thus, output TPSets represent a rewriting.  No
           TrigPrims are rewritten.  A TrigPrim is added to a windowed
           TPSet based solely on its tstart.  This means that any
           activity spanning a window boundary will present a ragged
           edge.  A future "TPSlice" may be invented.
        
           The configuration parameters are:
        
           - input.socket :: usual socket configuation for input

           - output.socket :: usual socket configuation for output
        
           - detid :: if nonnegative, use this as output detid, -1 use detid from input
        
           - toffset :: start window this many HW clock ticks from 0.
        
           - tspan :: the span of the window in the HW clock.
        
           - tbuffer :: the minimum time span of a buffer defined on
           TrigPrim::tstart values which is maintained before taking
           wasy "tspan" worth of time to build output TPSet.
        */
        TPWindow(const std::string& config);

        /**
           The TPWindow will run a thread so needs to be kept in scope
           but otherwise doesn't have a serviceable API.  Destroy it
           when done and it will disconnect its sockets.
        */
        virtual ~TPWindow();

        // a self-test
        static void test();

    private:
        zactor_t* m_actor;

    };

    // A "free agent" that will act as czmqat
    class TPCat : public TPAgent {
    public:
        /** Create and configure a netcat type agent.  

            Top level config attributes

            - number :: number of messages to process, default or negative means process forever.

            - delayus :: delay in microseconds before emitting. Actual delay may not be at microsecond resolution.

            - isock :: if given, socket description object from which to receive messages

            - osock :: if given, socket description object from which to send messages

            - ifile :: if given, name of file from which to read messages

            - ofile :: if given, name of file from which to write messages

         */
        TPCat(const std::string& config);

        /** 
            Destroy the cat.
         */
        virtual ~TPCat();

    private:
        zactor_t* m_actor;
    };

    // A "free agent" that passes messages between two sockets and
    // monitors their content.  Messages are output exactly as they
    // are input.
    class TPMonitor : public TPAgent {
    public:
        /**
           Create and configure a monitor.  Top level attributes

           - filename :: file in which to dump monitor data

           - taps :: array of tap description objects

           A `tap` description is an object with these attributes:

           - id :: an integer used to identify results from this tap
             in the output file.

           - input :: a socket description object for the tap's input.  

           - output :: a socket description object for the tap's output.  

           The output file is a series of TPMonitor::Summary structs. 
        */
        struct Summary {

            /** The first three attributes are generated by TPMonitor,
                the remainer are derived from a TPSet.
            */

            /// The tap ID from corresponding to the `id` given in the
            /// configuration.
            uint32_t tapid;
            
            /// The system time that the monitor received the TPSet (us
            /// from unix epoch)
            int64_t trecv;

            /// Count the number of received TPSets.
            uint32_t nrecv;

            /// the TPSet.count
            uint32_t count;

            /// The TPSet.detid
            uint32_t detid;

            /// The TPSet.created (microseconds from unix epoch)
            int64_t created;

            /// The TPSet.tstart (hardware clock)
            uint64_t tstart;

            /// The TPSet.tspan (hardware clock)
            uint32_t tspan;

            /// The TPSet.tsp_size
            uint32_t ntps;
        };

        TPMonitor(const std::string& config);

        /** 
            The TPMonitor runs threads in the background and must be
            kept in scope for as long as needed.  Since it imposes
            itself between streams, its destruction will disrupt their
            flow.
        */
        virtual ~TPMonitor();

    private:
        zactor_t* m_actor;

    };


    // a libzmq based proxy
    class TPMonitorz : public TPAgent {
    public:
        TPMonitorz(const std::string& config);
        virtual ~TPMonitorz();

    private:
        zactor_t* m_actor;

    };

    // A czmq based proxy
    class TPMonitorcz : public TPAgent {
    public:
        TPMonitorcz(const std::string& config);
        virtual ~TPMonitorcz();

    private:
        zactor_t* m_actor;

    };

}
#endif
