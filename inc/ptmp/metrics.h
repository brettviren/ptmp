#ifndef PTMP_METRICS_H
#define PTMP_METRICS_H

#include "ptmp/data.h"

#include "json.hpp"


namespace ptmp {
    namespace metrics {
        
        // JSON string-oriented message type. 
        const int json_message_type = 0x4a534f4e; // 1246973774 in decimal

        // Graphite line-oriented message type.
        const int glot_message_type = 0x474c4f54; // 1196183380 in decimal

        std::string glot_stringify(nlohmann::json& jdat, const std::string& prefix,
                                   ptmp::data::real_time_t now_us);


        /** Helper class to emit metrics.
         *
         * This holds a socket out which proper PTMP metric messages are sent.
         *
         * Application code or PTMP actors may create and hold an
         * instance and later call it.  As this class has a socket,
         * it's use should be kept to the thread in which it was
         * created.
         *
         * The configuration attributes are:
         *
         * - output :: usual socket description.  
         * - proto :: Output protocol to use.  Supported are: "JSON" and "GLOT".
         * - prefix :: hierarchy branch path which is appended to a hard-wired "ptmp.".  It may contain intermediate "."'s.
         *
         * It is recomended that when this class is used as part of
         * another PTMP component that this configuration object is
         * provided by a top level "metrics" attribute of the overall
         * components configuration.
         */
        class Metric {

        public:
            struct Proto {
                virtual void send(nlohmann::json&, ptmp::data::real_time_t now_us) = 0;
                virtual std::string pathify(std::string) = 0;
            };

            // Construct and configure a PTMP metric source, creating a socket.
            Metric(const std::string& config);

            // Destroy metric socket.
            ~Metric();

            // Emit an structured object of metrics.  If now_us is zero,
            // it will be set to the current system time in microseconds.
            // A "now" attribute in any part of the metricts structure
            // will override.  The metrics structure is emitted in an
            // object rooted on the attribute given by "/ptmp/<prefix>".
            void operator()(nlohmann::json& metrics, ptmp::data::real_time_t now_us=0);

            // Emit a one-off metric.  This sends an individual metric in
            // it's own message and might be good for reporting isolated,
            // exceptional events.  If multiple metrics are to be
            // consideried together, better to use the above call.
            // The path may be "."-delimited.
            template<typename TYPE>
            void operator()(const std::string& path, const TYPE& value, 
                            ptmp::data::real_time_t now_us=0) {
                using json = nlohmann::json;
                json j;
                j[json::json_pointer(m_proto->pathify(path))] = value;
                (*this)(j, now_us);
            }
        private:
            Proto* m_proto;
            
        };

    }
}



#endif

