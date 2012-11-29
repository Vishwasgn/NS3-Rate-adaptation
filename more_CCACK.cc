/* -*-  Mode: C++; c-file-style: "gnu"; indent-tabs-mode:nil; -*- */
/*
 * Copyright (c) 2009 The Boeing Company
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
 */

// 
// This script configures two nodes on an 802.11b physical layer, with
// 802.11b NICs in adhoc mode, and by default, sends one packet of 1000 
// (application) bytes to the other node.  The physical layer is configured
// to receive at a fixed RSS (regardless of the distance and transmit
// power); therefore, changing position of the nodes has no effect. 
//
// There are a number of command-line options available to control
// the default behavior.  The list of available command-line options
// can be listed with the following command:
// ./waf --run "wifi-simple-adhoc --help"
//
// For instance, for this configuration, the physical layer will
// stop successfully receiving packets when rss drops below -97 dBm.
// To see this effect, try running:
//
// ./waf --run "wifi-simple-adhoc --rss=-97 --numPackets=20"
// ./waf --run "wifi-simple-adhoc --rss=-98 --numPackets=20"
// ./waf --run "wifi-simple-adhoc --rss=-99 --numPackets=20"
//
// Note that all ns-3 attributes (not just the ones exposed in the below
// script) can be changed at command line; see the documentation.
//
// This script can also be helpful to put the Wifi layer into verbose
// logging mode; this command will turn on all wifi logging:
// 
// ./waf --run "wifi-simple-adhoc --verbose=1"
//
// When you are done, you will notice two pcap trace files in your directory.
// If you have tcpdump installed, you can try this:
//
// tcpdump -r wifi-simple-adhoc-0-0.pcap -nn -tt
//

#include "ns3/core-module.h"
#include "ns3/network-module.h"
#include "ns3/mobility-module.h"
#include "ns3/config-store-module.h"
#include "ns3/wifi-module.h"
#include "ns3/internet-module.h"
#include "ns3/header.h"
#define NS3_HACK

#include "/home/vishwas/bin/CCACK/SimplyNC.cc"





 // Created 4 nodes where 2 are sender and receiver and recv1 and recv2 are intermediate nodes.

int flag = 0;
uint8_t *temp;
NC_node *src[10];
float fwd_list[11][2];
int fowder_list[11];
ifstream rate_file; 
NS_LOG_COMPONENT_DEFINE ("WifiSimpleAdhoc");
Flow *globalpFlow ;
using namespace ns3;

float loss_rate[60][10];

void Generate_forwarder_Traffic (Ptr<Socket> socket, uint8_t * packet)
{
  
        int node_no;
        Ptr<Node> nd = socket->GetNode();/*getting node*/
       
	node_no = nd->GetId();
    

      
      
      
      cout<<"sending packet from " << node_no <<" from our module " << endl;
      //uint8_t packetdata[4] = {11,20,30,40};
      /*1 byte of the packet will have sourcr node no*/
      //packetdata[0] = node_no;
       
      socket->Send (Create<Packet> (packet,148));
      //socket->Send (p);
 
}

void ReceivePacket (Ptr<Socket> socket)
{
      // PacketCCACK_s pacInfo;
        Ptr<Node> nd = socket->GetNode();
    uint8_t pac = 0;
    uint8_t *pkt;
    Ptr<Packet> p = socket->Recv();
    uint8_t *buffer = new uint8_t[p->GetSize ()];
    p->CopyData(buffer,p->GetSize ());
    
    int nd_id = nd->GetId();
    if(nd_id > -1 && nd_id < 11)
{
    //cout <<"1 st byte " << (int)buffer[0] <<(int)buffer[1] <<endl;
    //UdpHeader hd;
    //p->PeekHeader(hd);
    //NS_LOG_UNCOND("Read packet header"<<hd);

      cout << "received packet at node " << nd->GetId() <<"size " << p->GetSize () << endl;   
     pac = src[nd_id]->checkReceivedCCACK(buffer, p->GetSize ());
      if(pac != 0 )
        {
                cout<<"sending another paCKET from"<< nd->GetId();
                //Generate_forwarder_Traffic(socket,packet);
                pkt = src[nd->GetId()]->sendByForwarder(0,9);
                Generate_forwarder_Traffic(socket,pkt);

                
        }

    // cout << "source address " << src[nd_id]->packet.srcAddress << endl;
//cout << "dest address " << src[nd_id]->packet.destAddress << endl;
//cout << "codevector[0] " << src[nd_id]->packet.codeVector[0] << endl;

   
    loss_rate[(int)buffer[0]][(int)nd->GetId()]++;
}
}




static void GenerateTraffic (Ptr<Socket> socket, uint32_t pktSize, 
                             uint32_t pktCount, Time pktInterval )
{
  if (pktCount > 0)
    {
        int node_no;
        Ptr<Node> nd = socket->GetNode();/*getting node*/
       uint8_t *pLastPacket=NULL;
	node_no = nd->GetId();
    
        src[node_no]->updateLocalForwarders(globalpFlow,1,src[node_no]->local_fwdrs);
        if(flag == 0)
        {
        src[node_no]->newPacketBySource(9);
	pLastPacket= src[node_no]->sendBySource(globalpFlow);  
        temp = pLastPacket;      
        flag = 1;   
        }
      
      
      
      cout<<"sending packet from " << node_no <<"from our module- packet " <<pktCount << endl;
      //uint8_t packetdata[4] = {11,20,30,40};
      /*1 byte of the packet will have sourcr node no*/
      //packetdata[0] = node_no;
      socket->Send (Create<Packet> (temp,148));
      //socket->Send (p);
        cout<<"packet sent\n";
      Simulator::Schedule (pktInterval, &GenerateTraffic, 
                           socket, pktSize,pktCount-1, pktInterval);
    }
  else
    {
      socket->Close ();
    }
}

Flow * get_forwarder_list(int source)
{
   int x,y;
   float temp[2];
  /*reading from out put rate file*/
  for(x = 0; x < 10; x++)
  {
     rate_file >> fwd_list[x][0] ;/*copying node id */
     fwd_list[x][0]--;/*because in NS3 node starts from 0*/ 
     rate_file >> fwd_list[x][1];
     cout << fwd_list[x][0] << " "<<fwd_list[x][1]<< endl;
  }

  Flow *pFlow = src[source]->getFlow(9);
  
  for(x = 0; x < 10 ; x++)
  { 
    for(y = 0; y < 10 - x; y++)
    {
     if(fwd_list[y][1] < fwd_list[y+1][1])
     {
        temp[0] = fwd_list[y+1][0];
        temp[1] = fwd_list[y+1][1];
        fwd_list[y+1][0] = fwd_list[y][0];
        fwd_list[y+1][1] = fwd_list[y][1];
        fwd_list[y][0] = temp[0];
        fwd_list[y][1] = temp[1];
     }
    }
   }
       
   cout <<"printing sorted array"<< endl;
   for(x = 0; x < 10 ; x++)
  {
    cout << fwd_list[x][0] << " "<<fwd_list[x][1]<< endl;
    src[source]->updateGlobalForwarders(pFlow,fwd_list[x][0], 0);
 }  
 return pFlow;
}

void fill_fowder()
{
int no_of_fwds,x,y,fwder;
 Forwarder_s temp;

cout<<"adding forwarders\n";
for(x = 0; x < 10 ; x++)
{
 cout<<"node " << x << " ";
 rate_file >> no_of_fwds; /*this will copy node ids*/
 /*creatying forwders list*/
 //Flow *pFlow=src[x]->getFlow(9);
rate_file >> no_of_fwds; /*this will copy rate*/
 cout <<"no of fwders" << no_of_fwds;
rate_file >> no_of_fwds; /*this will copy number of forwaders for that node*/
cout <<"no of fwders" << no_of_fwds;
for(y = 0 ; y < no_of_fwds ; y++ )
{
  
  rate_file >> fwder;
  temp.id = fwder;
  temp.credit = 0; 

  src[x]->local_fwdrs.push_back(temp);

  cout << fwder << " ";
}

cout<<endl;
}


}

int main (int argc, char *argv[])
{
  //ErpOfdmRate24Mbps      
  //std::string phyMode ("DsssRate11Mbps");
  
 char phyMode[6][30] ={"ErpOfdmRate54Mbps","ErpOfdmRate12Mbps","ErpOfdmRate9Mbps","ErpOfdmRate6Mbps","DsssRate2Mbps","DsssRate1Mbps"};
  
  double rss = -80;  // -dBm
  uint32_t packetSize = 100; // bytes
  uint32_t numPackets = 2;
  double interval = 1.0; // seconds
  bool verbose = false;
  
  //int x,y;
  CommandLine cmd;

  rate_file.open ("output.txt");
  
  cmd.AddValue ("rss", "received signal strength", rss);
  cmd.AddValue ("packetSize", "size of application packet sent", packetSize);
  cmd.AddValue ("numPackets", "number of packets generated", numPackets);
  cmd.AddValue ("interval", "interval (seconds) between packets", interval);
  cmd.AddValue ("verbose", "turn on all WifiNetDevice log components", verbose);

  cmd.Parse (argc, argv);
  Time interPacketInterval = Seconds (interval);
  // Convert to time object
  //Time interPacketInterval = Seconds (interval);

  // disable fragmentation for frames below 2200 bytes
  Config::SetDefault ("ns3::WifiRemoteStationManager::FragmentationThreshold", StringValue ("2200"));
  // turn off RTS/CTS for frames below 2200 bytes
  Config::SetDefault ("ns3::WifiRemoteStationManager::RtsCtsThreshold", StringValue ("2200"));
  // Fix non-unicast data rate to be the same as that of unicast
  //std::string phyMode ("ErpOfdmRate24Mbps"); 
  cmd.AddValue ("phyMode", "Wifi Phy mode", phyMode[0]);
  Config::SetDefault ("ns3::WifiRemoteStationManager::NonUnicastMode", 
                      StringValue (phyMode[0]));


  NodeContainer c;
  c.Create (10);

  // The below set of helpers will help us to put together the wifi NICs we want
  WifiHelper wifi;
  if (verbose)
    {
      wifi.EnableLogComponents ();  // Turn on all Wifi logging
    }
  wifi.SetStandard (WIFI_PHY_STANDARD_80211b);

  YansWifiPhyHelper wifiPhy =  YansWifiPhyHelper::Default ();
  // This is one parameter that matters when using FixedRssLossModel
  // set it to zero; otherwise, gain will be added
  wifiPhy.Set ("RxGain", DoubleValue (0) ); 
  // ns-3 supports RadioTap and Prism tracing extensions for 802.11b
  wifiPhy.SetPcapDataLinkType (YansWifiPhyHelper::DLT_IEEE802_11_RADIO); 

  wifi.SetRemoteStationManager ("ns3::ConstantRateWifiManager",
                                "DataMode",StringValue (phyMode[0]),
                                "ControlMode",StringValue (phyMode[0]));

  YansWifiChannelHelper wifiChannel;
  wifiChannel.SetPropagationDelay ("ns3::RandomPropagationDelayModel");
  // The below FixedRssLossModel will cause the rss to be fixed regardless
  // of the distance between the two stations, and the transmit power
  //wifiChannel.AddPropagationLoss ("ns3::FixedRssLossModel","Rss",DoubleValue (rss));
  //uint8_t nRays = 3;
  //uint8_t nOscillators = 8;
        //JakesFadingLossModel
  //wifiChannel.AddPropagationLoss ("ns3::FriisPropagationLossModel");
  wifiChannel.AddPropagationLoss ("ns3::RandomPropagationLossModel","Variable", RandomVariableValue (ExponentialVariable (1.0))); 
  //wifiChannel.AddPropagationLoss ("ns3::JakesFadingLossModel"); 
  wifiPhy.SetChannel (wifiChannel.Create ());

  // Add a non-QoS upper mac, and disable rate control
  NqosWifiMacHelper wifiMac = NqosWifiMacHelper::Default ();

  // Set it to adhoc mode
  wifiMac.SetType ("ns3::AdhocWifiMac");
  NetDeviceContainer devices = wifi.Install (wifiPhy, wifiMac, c);

  // Note that with FixedRssLossModel, the positions below are not 
  // used for received signal strength. 
  MobilityHelper mobility;
  Ptr<ListPositionAllocator> positionAlloc = CreateObject<ListPositionAllocator> ();  
  positionAlloc->Add (Vector (0.0, 250.0, 0.0));//0
  positionAlloc->Add (Vector (250.0, 0.0, 0.0));//1
  positionAlloc->Add (Vector (250.0, 20.0, 0.0));//2
  positionAlloc->Add (Vector (250.0, 0.0, 0.0));//3
  positionAlloc->Add (Vector (500.0, 0.0, 0.0));//4
  positionAlloc->Add (Vector (0.0, 250.0, 100.0));//5
  positionAlloc->Add (Vector (250.0, 0.0, 200.0));//6
  positionAlloc->Add (Vector (250.0, 20.0, 300.0));//7
  positionAlloc->Add (Vector (250.0, 0.0, 400.0));//8
  positionAlloc->Add (Vector (500.0, 0.0, 500.0));//9
  mobility.SetPositionAllocator (positionAlloc);
  mobility.SetMobilityModel ("ns3::ConstantPositionMobilityModel");
  mobility.Install (c);

  InternetStackHelper internet;
  internet.Install (c);

  Ipv4AddressHelper ipv4;
  NS_LOG_INFO ("Assign IP Addresses.");
  ipv4.SetBase ("10.1.1.0", "255.255.255.0");
  Ipv4InterfaceContainer i = ipv4.Assign (devices);

  TypeId tid = TypeId::LookupByName ("ns3::UdpSocketFactory");
  Ptr<Socket> recvSink = Socket::CreateSocket (c.Get (2), tid);
  InetSocketAddress local = InetSocketAddress (Ipv4Address::GetAny (), 80);
  recvSink->Bind (local);
 
  recvSink->SetRecvCallback (MakeCallback (&ReceivePacket));
 //create another source 

  Ptr<Socket> source = Socket::CreateSocket (c.Get (0), tid);
  InetSocketAddress remote = InetSocketAddress (Ipv4Address ("255.255.255.255"), 80);
  source->SetAllowBroadcast (true);
  source->Connect (remote);
/*creating our node instance*/
 src[0]  = new NC_node(source->GetNode ()->GetId ()) ;
 

  Ptr<Socket> source1 = Socket::CreateSocket (c.Get (1), tid);
  InetSocketAddress remote1 = InetSocketAddress (Ipv4Address ("255.255.255.255"), 80);
  source1->SetAllowBroadcast (true);
  source1->Connect (remote1);  
/*creating our node instance*/
 src[1]  = new NC_node(source1->GetNode ()->GetId ()) ;

    Ptr<Socket> source2 = Socket::CreateSocket (c.Get (2), tid);
  InetSocketAddress remote2 = InetSocketAddress (Ipv4Address ("255.255.255.255"), 80);
  source2->SetAllowBroadcast (true);
  source2->Connect (remote2);
/*creating our node instance*/
 src[2]  = new NC_node(source2->GetNode ()->GetId ()) ;

      Ptr<Socket> source3 = Socket::CreateSocket (c.Get (3), tid);
  InetSocketAddress remote3 = InetSocketAddress (Ipv4Address ("255.255.255.255"), 80);
  source3->SetAllowBroadcast (true);
  source3->Connect (remote3);
/*creating our node instance*/
 src[3]  = new NC_node(source3->GetNode ()->GetId ()) ;

      Ptr<Socket> source4 = Socket::CreateSocket (c.Get (4), tid);
  InetSocketAddress remote4 = InetSocketAddress (Ipv4Address ("255.255.255.255"), 80);
  source4->SetAllowBroadcast (true);
  source4->Connect (remote4);
/*creating our node instance*/
 src[4]  = new NC_node(source4->GetNode ()->GetId ()) ;

Ptr<Socket> source5 = Socket::CreateSocket (c.Get (5), tid);
  InetSocketAddress remote5 = InetSocketAddress (Ipv4Address ("255.255.255.255"), 80);
  source5->SetAllowBroadcast (true);
  source5->Connect (remote5);
/*creating our node instance*/
 src[5]  = new NC_node(source5->GetNode ()->GetId ()) ;

Ptr<Socket> source6 = Socket::CreateSocket (c.Get (6), tid);
  InetSocketAddress remote6 = InetSocketAddress (Ipv4Address ("255.255.255.255"), 80);
  source6->SetAllowBroadcast (true);
  source6->Connect (remote6);
/*creating our node instance*/
 src[6]  = new NC_node(source6->GetNode ()->GetId ()) ;

Ptr<Socket> source7 = Socket::CreateSocket (c.Get (7), tid);
  InetSocketAddress remote7 = InetSocketAddress (Ipv4Address ("255.255.255.255"), 80);
  source7->SetAllowBroadcast (true);
  source7->Connect (remote7);
/*creating our node instance*/
 src[7]  = new NC_node(source7->GetNode ()->GetId ()) ;

Ptr<Socket> source8 = Socket::CreateSocket (c.Get (8), tid);
  InetSocketAddress remote8 = InetSocketAddress (Ipv4Address ("255.255.255.255"), 80);
  source8->SetAllowBroadcast (true);
  source8->Connect (remote8);
/*creating our node instance*/
 src[8]  = new NC_node(source8->GetNode ()->GetId ()) ;
    
Ptr<Socket> source9 = Socket::CreateSocket (c.Get (9), tid);
  InetSocketAddress remote9 = InetSocketAddress (Ipv4Address ("255.255.255.255"), 80);
  source9->SetAllowBroadcast (true);
  source9->Connect (remote9);
/*creating our node instance*/
 src[9]  = new NC_node(source9->GetNode ()->GetId ()) ;

//add one more reciever 
  Ptr<Socket> recvSink1 = Socket::CreateSocket (c.Get (1), tid);
  InetSocketAddress local1 = InetSocketAddress (Ipv4Address::GetAny (), 80);
  recvSink1->Bind (local1);
  recvSink1->SetRecvCallback (MakeCallback (&ReceivePacket));

  //add one more reciever 
  Ptr<Socket> recvSink2 = Socket::CreateSocket (c.Get (3), tid);
  InetSocketAddress local2 = InetSocketAddress (Ipv4Address::GetAny (), 80);
  recvSink2->Bind (local2);
  recvSink2->SetRecvCallback (MakeCallback (&ReceivePacket));
    
  //add one more reciever 
  Ptr<Socket> recvSink3 = Socket::CreateSocket (c.Get (4), tid);
  InetSocketAddress local3 = InetSocketAddress (Ipv4Address::GetAny (), 80);
  recvSink3->Bind (local3);
  recvSink3->SetRecvCallback (MakeCallback (&ReceivePacket)); 

  Ptr<Socket> recvSink0 = Socket::CreateSocket (c.Get (0), tid);
  InetSocketAddress local0 = InetSocketAddress (Ipv4Address::GetAny (), 80);
  recvSink0->Bind (local0);
  recvSink0->SetRecvCallback (MakeCallback (&ReceivePacket)); 
   
 //add one more reciever 
  Ptr<Socket> recvSink5 = Socket::CreateSocket (c.Get (5), tid);
  InetSocketAddress local5 = InetSocketAddress (Ipv4Address::GetAny (), 80);
  recvSink5->Bind (local5);
  recvSink5->SetRecvCallback (MakeCallback (&ReceivePacket));

//add one more reciever 
  Ptr<Socket> recvSink6 = Socket::CreateSocket (c.Get (6), tid);
  InetSocketAddress local6 = InetSocketAddress (Ipv4Address::GetAny (), 80);
  recvSink6->Bind (local6);
  recvSink6->SetRecvCallback (MakeCallback (&ReceivePacket));

//add one more reciever 
  Ptr<Socket> recvSink7 = Socket::CreateSocket (c.Get (7), tid);
  InetSocketAddress local7 = InetSocketAddress (Ipv4Address::GetAny (), 80);
  recvSink7->Bind (local7);
  recvSink7->SetRecvCallback (MakeCallback (&ReceivePacket));

//add one more reciever 
  Ptr<Socket> recvSink8 = Socket::CreateSocket (c.Get (8), tid);
  InetSocketAddress local8 = InetSocketAddress (Ipv4Address::GetAny (), 80);
  recvSink8->Bind (local8);
  recvSink8->SetRecvCallback (MakeCallback (&ReceivePacket));

//add one more reciever 
  Ptr<Socket> recvSink9 = Socket::CreateSocket (c.Get (9), tid);
  InetSocketAddress local9 = InetSocketAddress (Ipv4Address::GetAny (), 80);
  recvSink9->Bind (local9);
  recvSink9->SetRecvCallback (MakeCallback (&ReceivePacket)); 
 

  // Tracing
  wifiPhy.EnablePcap ("wifi-simple-adhoc", devices);

  // Output what we are doing
  //NS_LOG_UNCOND ("Testing " << numPackets  << " packets sent with receiver rss " << rss );
 globalpFlow = get_forwarder_list(0);
  fill_fowder();

  
  Simulator::ScheduleWithContext (source->GetNode ()->GetId (),
                                  Seconds (1.0), &GenerateTraffic, 
                                  source, packetSize, numPackets, interPacketInterval);




  
    //}
  Simulator::Run ();



  return 0;
}

