/* -*- Mode:C++; c-file-style:"gnu"; indent-tabs-mode:nil; -*- */
/*
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License version 2 as
 * published by the Free Software Foundation;
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program; if not, write to the Free Software
 * Foundation, Inc., 59 Temple Place, Suite 330, Boston, MA  02111-1307  USA
 */

#include "ns3/core-module.h"
#include "ns3/point-to-point-module.h"
#include "ns3/network-module.h"
#include "ns3/applications-module.h"
#include "ns3/wifi-module.h"
#include "ns3/flow-monitor-module.h"
#include "ns3/mobility-module.h"
#include "ns3/csma-module.h"
#include "ns3/internet-module.h"
#include "ns3/netanim-module.h"
#include "ns3/gnuplot.h" // to create plots using GNUPlot.

// Topology for testing the effect of one user changing demand in upload.
// Simple router connected to 3 wifi devices of which 1 is downloading and two are uploading.
// One of the uploader demands is looped.

using namespace ns3;

double* simulation(std::string datarate)
{
  uint32_t nWifi = 3;

  NodeContainer p2pNodes;
  p2pNodes.Create (2);
  NodeContainer n0n1 = NodeContainer (p2pNodes.Get (0), p2pNodes.Get (1));

  PointToPointHelper p2p;
  p2p.SetDeviceAttribute ("DataRate", StringValue (datarate));
  p2p.SetChannelAttribute ("Delay", StringValue ("2ms"));

  NetDeviceContainer d0d1 = p2p.Install (n0n1);

  NodeContainer wifiStaNodes;
  wifiStaNodes.Create (nWifi);
  NodeContainer wifiApNode = p2pNodes.Get (0);

  YansWifiChannelHelper channel = YansWifiChannelHelper::Default ();
  YansWifiPhyHelper phy = YansWifiPhyHelper::Default ();
  phy.SetChannel (channel.Create ());

  WifiHelper wifi;
  wifi.SetRemoteStationManager ("ns3::AarfWifiManager");

  WifiMacHelper mac;
  Ssid ssid = Ssid ("ns-3-ssid");
  mac.SetType ("ns3::StaWifiMac",
               "Ssid", SsidValue (ssid),
               "ActiveProbing", BooleanValue (false));

  NetDeviceContainer staDevices;
  staDevices = wifi.Install (phy, mac, wifiStaNodes);

  mac.SetType ("ns3::ApWifiMac",
               "Ssid", SsidValue (ssid));

  NetDeviceContainer apDevices;
  apDevices = wifi.Install (phy, mac, wifiApNode);

  MobilityHelper mobility;

  mobility.SetMobilityModel ("ns3::ConstantPositionMobilityModel");
  mobility.Install (wifiStaNodes);
  mobility.Install (p2pNodes);

  InternetStackHelper stack;
  stack.Install (wifiStaNodes);
  stack.Install (p2pNodes);

  Ipv4AddressHelper address;

  address.SetBase ("10.1.1.0", "255.255.255.0");
  Ipv4InterfaceContainer p0p1;
  p0p1 = address.Assign (d0d1);

  address.SetBase ("10.1.3.0", "255.255.255.0");
  Ipv4InterfaceContainer wifiInterfaces;
  wifiInterfaces = address.Assign (staDevices);
  address.Assign (apDevices);

  Ipv4GlobalRoutingHelper::PopulateRoutingTables ();

  // Create the OnOff application for clearly defined demand in traffic
    uint16_t port = 9;   // Discard port (RFC 863)
    OnOffHelper onoff ("ns3::UdpSocketFactory",
                       Address (InetSocketAddress (p0p1.GetAddress (1), port)));
    onoff.SetConstantRate (DataRate ("500kb/s"));
    ApplicationContainer apps = onoff.Install (wifiStaNodes.Get (1));
    apps.Start (Seconds (1.0));
    apps.Stop (Seconds (10.0));


    // Create a similar flow with different specs
    onoff.SetAttribute ("Remote",
                        AddressValue (InetSocketAddress (wifiInterfaces.GetAddress (0), port)));
    apps = onoff.Install (p2pNodes.Get (1));
    apps.Start (Seconds (1.0));
    apps.Stop (Seconds (10.0));

    // Create a similar flow with different specs
    onoff.SetAttribute("DataRate", StringValue ("1000kb/s"));
    onoff.SetAttribute ("Remote",
                        AddressValue (InetSocketAddress (wifiInterfaces.GetAddress (1), port)));
    apps = onoff.Install (p2pNodes.Get (1));
    apps.Start (Seconds (1.0));
    apps.Stop (Seconds (10.0));

  Simulator::Stop (Seconds (10.0));

  AnimationInterface anim ("a_kees.xml");
  anim.SetConstantPosition(wifiStaNodes.Get(0), 3.0, 2.0);
  anim.SetConstantPosition(wifiStaNodes.Get(1), 2.0, 4.0);
  anim.SetConstantPosition(wifiStaNodes.Get(2), 3.0, 6.0);
  anim.SetConstantPosition(p2pNodes.Get(0), 8.0, 4.0);
  anim.SetConstantPosition(p2pNodes.Get(1), 13.0, 4.0);

  FlowMonitorHelper flowmon;
  Ptr<FlowMonitor> monitor = flowmon.InstallAll ();

  Simulator::Run ();

  monitor->CheckForLostPackets ();
  Ptr<Ipv4FlowClassifier> classifier = DynamicCast<Ipv4FlowClassifier> (flowmon.GetClassifier ());
  FlowMonitor::FlowStatsContainer stats = monitor->GetFlowStats ();
  static double throughput[] = {0,0,0};
  int j = 0;
  for (std::map<FlowId, FlowMonitor::FlowStats>::const_iterator i = stats.begin (); i != stats.end (); ++i)
    {
      if (i->first > 0)
        {
          throughput[j] = i->second.rxBytes * 8.0 / 9.0 / 1000 / 1000;
          j++;
        }
    }

  Simulator::Destroy ();
  return throughput;
}



int main (int argc, char *argv[])
{
	int datarate = 500;
	std::stringstream ss; // used for conversion from integer to input string of correct format
	double *throughput;
  int DR_start = 1; //DR = datarate
  int DR_stepsize = 250;
  int DR_no_steps = 9;

  std::string fileNameWithNoExtension = "up2down1";
  std::string graphicsFileName        = fileNameWithNoExtension + ".png";
  std::string plotFileName            = fileNameWithNoExtension + ".plt";
  std::string plotTitle               = "Throughput vs. datarate";

  // Instantiate the datasets, set their titles, and make sure the points are
  // plotted along with connecting lines.
  Gnuplot2dDataset flow1;
  flow1.SetTitle ("1000kb/s up");
  flow1.SetStyle (Gnuplot2dDataset::LINES_POINTS);

  Gnuplot2dDataset flow2;
  flow2.SetTitle ("500kb/s up");
  flow2.SetStyle (Gnuplot2dDataset::LINES_POINTS);

  Gnuplot2dDataset flow3;
  flow3.SetTitle ("1000kb/s down");
  flow3.SetStyle (Gnuplot2dDataset::LINES_POINTS);

  double x;
  double y;

  for(int i=0; i<DR_no_steps; i++){
    datarate = DR_start + DR_stepsize*i;
    ss.str("");
    ss << datarate << "kbps";
  	std::string datarate_n2n3 = ss.str();
  	std::cout<< "\n" << "Datarate: " << datarate_n2n3 << "\n";

    throughput = simulation(datarate_n2n3);
  	std::cout<< "throughput 0: " << throughput[0] << "\n";
    std::cout<< "throughput 1: " << throughput[1] << "\n";
    std::cout<< "throughput 2: " << throughput[2] << "\n";
    //throughput_array[i]=throughput;

    // Add datapoints.
    x = datarate;
    y = throughput[0];
    flow1.Add (x, y);

    y = throughput[1];
    flow2.Add (x, y);

    y = throughput[2];
    flow3.Add (x, y);
  }

//===========================================================================
// This creates a 2-D plot file.
//===========================================================================


  // Instantiate the plot and set its title.
  Gnuplot plot (graphicsFileName);
  plot.SetTitle (plotTitle);

  // Make the graphics file, which the plot file will create when it
  // is used with Gnuplot, be a PNG file.
  plot.SetTerminal ("png");

  // Set the labels for each axis.
  plot.SetLegend ("Datarate (kbps)", "Throughput (Mbps)");

  ss.str("");
  ss << "set xrange [" << DR_start << ":" << DR_start+DR_stepsize*DR_no_steps << "]";
  std::string x_range = ss.str();

  // Set the range for the x axis.
  plot.AppendExtra (x_range);

  // Add the datasets to the plot.
  plot.AddDataset (flow1);
  plot.AddDataset (flow2);
  plot.AddDataset (flow3);

  // Open the plot file.
  std::ofstream plotFile (plotFileName.c_str());

  // Write the plot file.
  plot.GenerateOutput (plotFile);

  // Close the plot file.
  plotFile.close ();

  return 0;
}

