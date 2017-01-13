/* -*- Mode:C++; c-file-style:"gnu"; indent-tabs-mode:nil; -*- */
/*
 * Copyright (c) 2016 NITK Surathkal
 *
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
 *
 * Author: Vivek Jain <jain.vivek.anand@gmail.com>
 *         Mohit P. Tahiliani <tahiliani@nitk.edu.in>
 *
 */

#include "ns3/core-module.h"
#include "ns3/network-module.h"
#include "ns3/internet-module.h"
#include "ns3/point-to-point-module.h"
#include "ns3/applications-module.h"
#include "ns3/point-to-point-layout-module.h"
#include "ns3/traffic-control-module.h"

#include <iostream>
#include <iomanip>
#include <map>

using namespace ns3;

int main (int argc, char *argv[])
{
  uint32_t    nLeaf = 10;
  uint32_t    maxPackets = 100;
  bool        modeBytes  = false;
  uint32_t    queueDiscLimitPackets = 1000;
  double      minTh = 5;
  double      maxTh = 15;
  uint32_t    pktSize = 512;
  std::string appDataRate = "10Mbps";
  std::string queueDiscType = "RED";
  uint16_t port = 5001;
  std::string bottleNeckLinkBw = "1Mbps";
  std::string bottleNeckLinkDelay = "50ms";

  CommandLine cmd;
  cmd.AddValue ("nLeaf",     "Number of left and right side leaf nodes", nLeaf);
  cmd.AddValue ("maxPackets","Max Packets allowed in the device queue", maxPackets);
  cmd.AddValue ("queueDiscLimitPackets","Max Packets allowed in the queue disc", queueDiscLimitPackets);
  cmd.AddValue ("queueDiscType", "Set Queue disc type to RED or BLUE", queueDiscType);
  cmd.AddValue ("appPktSize", "Set OnOff App Packet Size", pktSize);
  cmd.AddValue ("appDataRate", "Set OnOff App DataRate", appDataRate);
  cmd.AddValue ("modeBytes", "Set Queue disc mode to Packets <false> or bytes <true>", modeBytes);

  cmd.AddValue ("redMinTh", "RED queue minimum threshold", minTh);
  cmd.AddValue ("redMaxTh", "RED queue maximum threshold", maxTh);
  cmd.Parse (argc,argv);

  if ((queueDiscType != "RED") && (queueDiscType != "BLUE"))
    {
      std::cout << "Invalid queue disc type: Use --queueDiscType=RED or --queueDiscType=BLUE" << std::endl;
      exit (1);
    }

  Config::SetDefault ("ns3::OnOffApplication::PacketSize", UintegerValue (pktSize));
  Config::SetDefault ("ns3::OnOffApplication::DataRate", StringValue (appDataRate));

  Config::SetDefault ("ns3::Queue::Mode", StringValue ("QUEUE_MODE_PACKETS"));
  Config::SetDefault ("ns3::Queue::MaxPackets", UintegerValue (maxPackets));

  if (!modeBytes)
    {
      Config::SetDefault ("ns3::BlueQueueDisc::Mode", StringValue ("QUEUE_MODE_PACKETS"));
      Config::SetDefault ("ns3::BlueQueueDisc::QueueLimit", UintegerValue (queueDiscLimitPackets));
      Config::SetDefault ("ns3::RedQueueDisc::Mode", StringValue ("QUEUE_MODE_PACKETS"));
      Config::SetDefault ("ns3::RedQueueDisc::QueueLimit", UintegerValue (queueDiscLimitPackets));
    }
  else
    {
      Config::SetDefault ("ns3::BlueQueueDisc::Mode", StringValue ("QUEUE_MODE_BYTES"));
      Config::SetDefault ("ns3::BlueQueueDisc::QueueLimit", UintegerValue (queueDiscLimitPackets * pktSize));
      Config::SetDefault ("ns3::RedQueueDisc::Mode", StringValue ("QUEUE_MODE_BYTES"));
      Config::SetDefault ("ns3::RedQueueDisc::QueueLimit", UintegerValue (queueDiscLimitPackets * pktSize));
      minTh *= pktSize;
      maxTh *= pktSize;
    }

  if (queueDiscType == "RED")
    {
      Config::SetDefault ("ns3::RedQueueDisc::MinTh", DoubleValue (minTh));
      Config::SetDefault ("ns3::RedQueueDisc::MaxTh", DoubleValue (maxTh));
      Config::SetDefault ("ns3::RedQueueDisc::LinkBandwidth", StringValue (bottleNeckLinkBw));
      Config::SetDefault ("ns3::RedQueueDisc::LinkDelay", StringValue (bottleNeckLinkDelay));
      Config::SetDefault ("ns3::RedQueueDisc::MeanPktSize", UintegerValue (pktSize));
    }
  else
    {
      Config::SetDefault ("ns3::BlueQueueDisc::PMark", DoubleValue (0.0));
      Config::SetDefault ("ns3::BlueQueueDisc::Increment", DoubleValue (0.0025));
      Config::SetDefault ("ns3::BlueQueueDisc::Decrement", DoubleValue (0.00025));
      Config::SetDefault ("ns3::BlueQueueDisc::MeanPktSize", UintegerValue (pktSize));
    }

  // Create the point-to-point link helpers
  PointToPointHelper bottleNeckLink;
  bottleNeckLink.SetDeviceAttribute  ("DataRate", StringValue (bottleNeckLinkBw));
  bottleNeckLink.SetChannelAttribute ("Delay", StringValue (bottleNeckLinkDelay));

  PointToPointHelper pointToPointLeaf;
  pointToPointLeaf.SetDeviceAttribute    ("DataRate", StringValue ("10Mbps"));
  pointToPointLeaf.SetChannelAttribute   ("Delay", StringValue ("1ms"));

  PointToPointDumbbellHelper d (nLeaf, pointToPointLeaf,
                                nLeaf, pointToPointLeaf,
                                bottleNeckLink);

  // Install Stack
  InternetStackHelper stack;
  for (uint32_t i = 0; i < d.LeftCount (); ++i)
    {
      stack.Install (d.GetLeft (i));
    }
  for (uint32_t i = 0; i < d.RightCount (); ++i)
    {
      stack.Install (d.GetRight (i));
    }

  stack.Install (d.GetLeft ());
  stack.Install (d.GetRight ());
  TrafficControlHelper tchBottleneck;
  QueueDiscContainer queueDiscs;
  if (queueDiscType == "RED")
    {
      tchBottleneck.SetRootQueueDisc ("ns3::RedQueueDisc");
    }
  else
    {
      tchBottleneck.SetRootQueueDisc ("ns3::BlueQueueDisc");
    }
  tchBottleneck.Install (d.GetLeft ()->GetDevice (0));
  queueDiscs = tchBottleneck.Install (d.GetRight ()->GetDevice (0));

  // Assign IP Addresses
  d.AssignIpv4Addresses (Ipv4AddressHelper ("10.1.1.0", "255.255.255.0"),
                         Ipv4AddressHelper ("10.2.1.0", "255.255.255.0"),
                         Ipv4AddressHelper ("10.3.1.0", "255.255.255.0"));

  // Install on/off app on all right side nodes
  OnOffHelper clientHelper ("ns3::TcpSocketFactory", Address ());
  clientHelper.SetAttribute ("OnTime", StringValue ("ns3::UniformRandomVariable[Min=0.|Max=1.]"));
  clientHelper.SetAttribute ("OffTime", StringValue ("ns3::UniformRandomVariable[Min=0.|Max=1.]"));
  Address sinkLocalAddress (InetSocketAddress (Ipv4Address::GetAny (), port));
  PacketSinkHelper packetSinkHelper ("ns3::TcpSocketFactory", sinkLocalAddress);
  ApplicationContainer sinkApps;
  for (uint32_t i = 0; i < d.LeftCount (); ++i)
    {
      sinkApps.Add (packetSinkHelper.Install (d.GetLeft (i)));
    }
  sinkApps.Start (Seconds (0.0));
  sinkApps.Stop (Seconds (30.0));

  ApplicationContainer clientApps;
  for (uint32_t i = 0; i < d.RightCount (); ++i)
    {
      // Create an on/off app sending packets to the left side
      AddressValue remoteAddress (InetSocketAddress (d.GetLeftIpv4Address (i), port));
      clientHelper.SetAttribute ("Remote", remoteAddress);
      clientApps.Add (clientHelper.Install (d.GetRight (i)));
    }
  clientApps.Start (Seconds (1.0)); // Start 1 second after sink
  clientApps.Stop (Seconds (15.0)); // Stop before the sink

  Ipv4GlobalRoutingHelper::PopulateRoutingTables ();

  std::cout << "Running the simulation" << std::endl;
  Simulator::Run ();



  if (queueDiscType == "RED")
    {
      RedQueueDisc::Stats st = StaticCast<RedQueueDisc> (queueDiscs.Get (0))->GetStats ();
      if (st.unforcedDrop > st.forcedDrop)
        {
          std::cout << "Drops due to prob mark should be less than the drops due to hard mark" << std::endl;
          exit (1);
        }

      if (st.qLimDrop != 0)
        {
          std::cout << "There should be zero drops due to queue full" << std::endl;
          exit (1);
        }
      std::cout << "*** Stats from the bottleneck queue disc ***" << std::endl;
      std::cout << "\t " << st.unforcedDrop << " drops due to prob mark" << std::endl;
      std::cout << "\t " << st.forcedDrop << " drops due to hard mark" << std::endl;
      std::cout << "\t " << st.qLimDrop << " drops due to queue full" << std::endl;
      std::cout << "Destroying the simulation" << std::endl;
    }
  else
    {
      BlueQueueDisc::Stats st = StaticCast<BlueQueueDisc> (queueDiscs.Get (0))->GetStats ();
      std::cout << "*** Stats from the bottleneck queue disc ***" << std::endl;
      std::cout << "\t " << st.unforcedDrop << " drops due to prob mark" << std::endl;
      std::cout << "\t " << st.forcedDrop << " drops due to queue limit" << std::endl;
      std::cout << "Destroying the simulation" << std::endl;
    }


  Simulator::Destroy ();
  return 0;
}
