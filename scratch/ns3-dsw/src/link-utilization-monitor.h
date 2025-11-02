#ifndef LINK_UTILIZATION_MONITOR_H
#define LINK_UTILIZATION_MONITOR_H

#include "ns3/object.h"
#include "ns3/nstime.h"
#include "ns3/data-rate.h"
#include "ns3/net-device.h"
#include "ns3/queue-disc.h" // 包含 QueueDisc
#include "ns3/node.h" // 包含 Node
#include "ns3/traffic-control-layer.h" // 包含 TrafficControlLayer
#include "ns3/core-module.h" // 包含 Ptr

#include <string>
#include <vector>
#include <fstream>
#include <iomanip> // 用于 std::setw 等

namespace ns3 {

/**
 * \ingroup traffic-control
 * \brief A monitor to poll QueueDisc statistics and report link utilization.
 *
 * This class polls the root QueueDisc statistics for registered P2P links
 * at a fixed interval. It calculates the bidirectional data rate (Mbps) and
 * utilization (%), printing a report to the console and logging
 * time-series data to an XML file.
 */
class LinkUtilizationMonitor : public Object
{
  public:
    /**
     * \brief Get the type ID.
     * \return the object TypeId
     */
    static TypeId GetTypeId();

    LinkUtilizationMonitor();
    ~LinkUtilizationMonitor() override;

    /**
     * \brief Set the polling interval.
     * \param interval The time between statistics polls.
     */
    void SetPollInterval(Time interval);

    /**
     * \brief Set the path for the XML output file.
     * \param path The filesystem path (e.g., "scratch/ns3-dsw/out/link_util.xml").
     */
    void SetXmlOutput(std::string path);

    /**
     * \brief Register a point-to-point link for monitoring.
     *
     * This method must be called for each link *after* the TrafficControlHelper
     * has installed the queue discs on the devices.
     *
     * \param linkId A unique ID for the link (from CSV).
     * \param nodeAId The ID of the first node (CSV 'a').
     * \param nodeBId The ID of the second node (CSV 'b').
     * \param devA The NetDevice on node A connected to node B.
     * \param devB The NetDevice on node B connected to node A.
     * \param rate The configured capacity (DataRate) of the link.
     */
    void RegisterLink(uint32_t linkId,
                      uint32_t nodeAId,
                      uint32_t nodeBId,
                      Ptr<NetDevice> devA,
                      Ptr<NetDevice> devB,
                      DataRate rate);

    /**
     * \brief Start the periodic polling.
     */
    void Start();

    /**
     * \brief Stop the periodic polling.
     */
    void Stop();

  protected:
    void DoDispose() override;

  private:
    /**
     * \brief Internal struct to track stats for one link.
     */
    struct LinkRecord
    {
        uint32_t linkId;
        uint32_t nodeAId;
        uint32_t nodeBId;
        DataRate rate;        //!< Link capacity
        std::string rateStr;  //!< Cached string representation of rate
        Ptr<QueueDisc> qd_A_to_B; //!< QueueDisc on Node A (sends to B)
        Ptr<QueueDisc> qd_B_to_A; //!< QueueDisc on Node B (sends to A)
        uint64_t prevTxBytes_A_to_B; //!< Previous Tx bytes (A->B)
        uint64_t prevTxBytes_B_to_A; //!< Previous Tx bytes (B->A)
    };

    /**
     * \brief The main polling function.
     */
    void PollStats();

    /**
     * \brief Writes the XML file header.
     */
    void WriteXmlHeader();

    /**
     * \brief Writes the XML file footer.
     */
    void WriteXmlFooter();

    /**
     * \brief Helper to get the TC layer from a device.
     * \param dev The device.
     * \return A pointer to the TrafficControlLayer or nullptr.
     */
    Ptr<TrafficControlLayer> GetTcLayer(Ptr<NetDevice> dev);

    std::vector<LinkRecord> m_links; //!< List of all monitored links
    Time m_interval;                 //!< Polling interval
    bool m_running;                  //!< Flag to control the polling loop
    bool m_xmlHeaderWritten;         //!< Flag to ensure footer is only written if header was
    std::ofstream m_xmlFile;         //!< Output file stream for XML
    std::string m_xmlFilePath;       //!< Path to the XML file
};

} // namespace ns3

#endif // LINK_UTILIZATION_MONITOR_H