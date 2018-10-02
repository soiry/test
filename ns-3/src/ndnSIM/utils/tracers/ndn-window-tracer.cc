/* -*- Mode:C++; c-file-style:"gnu"; indent-tabs-mode:nil -*- */
/*
 * Copyright (c) 2011 UCLA
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
 * Author:  Alexander Afanasyev <alexander.afanasyev@ucla.edu>
 */

#include "ndn-window-tracer.h"

#include "ns3/node.h"
#include "ns3/packet.h"
#include "ns3/config.h"
#include "ns3/callback.h"
#include "ns3/names.h"
#include "ns3/ndn-app.h"
#include "ns3/ndn-face.h"
#include "ns3/ndn-interest.h"
#include "ns3/ndn-content-object.h"
#include "ns3/simulator.h"
#include "ns3/node-list.h"
#include "ns3/log.h"
#include "ns3/ndnSIM/apps/ndn-consumer-window.h"

#include <boost/lexical_cast.hpp>

#include <fstream>

using namespace std;

NS_LOG_COMPONENT_DEFINE ("ndn.WindowTracer");

namespace ns3 {
namespace ndn {

boost::tuple< boost::shared_ptr<std::ostream>, std::list<Ptr<WindowTracer> > >
WindowTracer::InstallAll (const std::string &file, Time averagingPeriod/* = Seconds (0.5)*/)
{
  using namespace boost;
  using namespace std;
  
  std::list<Ptr<WindowTracer> > tracers;
  boost::shared_ptr<std::ofstream> outputStream (new std::ofstream ());
  outputStream->open (file.c_str (), std::ios_base::out | std::ios_base::trunc);

  if (!outputStream->is_open ())
    return boost::make_tuple (outputStream, tracers);

  for (NodeList::Iterator node = NodeList::Begin ();
       node != NodeList::End ();
       node++)
    {
      NS_LOG_DEBUG ("Node: " << (*node)->GetId ());

      Ptr<WindowTracer> trace = Create<WindowTracer> (outputStream, *node);
      trace->SetAveragingPeriod (averagingPeriod);
      tracers.push_back (trace);
    }

  if (tracers.size () > 0)
    {
      // *m_l3RateTrace << "# "; // not necessary for R's read.table
      tracers.front ()->PrintHeader (*outputStream);
      *outputStream << "\n";
    }

  return boost::make_tuple (outputStream, tracers);
}



WindowTracer::WindowTracer (boost::shared_ptr<std::ostream> os, Ptr<Node> node)
: m_nodePtr (node)
, m_os (os)
{
  m_node = boost::lexical_cast<string> (m_nodePtr->GetId ());

  Connect ();

  string name = Names::FindName (node);
  if (!name.empty ())
    {
      m_node = name;
    }
  Reset ();
}

WindowTracer::WindowTracer (boost::shared_ptr<std::ostream> os, const std::string &node)
: m_node (node)
, m_os (os)
{
  Connect ();
  Reset ();
}

WindowTracer::~WindowTracer ()
{
};

void
WindowTracer::Connect ()
{
  Config::Connect ("/NodeList/"+m_node+"/ApplicationList/*/WindowTrace",
                   MakeCallback (&WindowTracer::Window, this));
}

void
WindowTracer::SetAveragingPeriod (const Time &period)
{
  m_period = period;
  m_printEvent.Cancel ();
  m_printEvent = Simulator::Schedule (m_period, &WindowTracer::PeriodicPrinter, this);
}

void
WindowTracer::PeriodicPrinter ()
{
  Print (*m_os);
  Reset ();
  
  m_printEvent = Simulator::Schedule (m_period, &WindowTracer::PeriodicPrinter, this);
}

void
WindowTracer::PrintHeader (std::ostream &os) const
{
  os << "Time" << "\t"

     << "Node" << "\t"
     << "WindowSize";
}

void
WindowTracer::Reset ()
{
  m_window = 0;
}


//#define STATS(INDEX) stats->second.get<INDEX> ()

#define PRINTER(printName, fieldName) \
  os << time.ToDouble (Time::S) << "\t"                                 \
  << m_node << "\t"                                                     \
  << fieldName << "\n";


void
WindowTracer::Print (std::ostream &os) const
{
  Time time = Simulator::Now ();

  PRINTER ("WindowSize",   m_window);

}

void
WindowTracer::Window (std::string context,
                                  Ptr<ConsumerWindow> app)
{
  m_window = app->GetWindow ();
}

} // namespace ndn
} // namespace ns3
