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

#ifndef NDN_WINDOW_TRACER_H
#define NDN_WINDOW_TRACER_H

#include "ns3/ptr.h"
#include "ns3/simple-ref-count.h"

#include <ns3/nstime.h>
#include <ns3/event-id.h>

#include <ns3/ndnSIM/apps/ndn-consumer-window.h>

#include <boost/tuple/tuple.hpp>
#include <boost/shared_ptr.hpp>
#include <map>
#include <list>

namespace ns3 {

class Node;
class Packet;

namespace ndn {

class ConsumerWindow;
/**
 * @ingroup ndn
 * @brief CCNx network-layer tracer for aggregate packet counts
 */
class WindowTracer : public SimpleRefCount<WindowTracer>
{
public:
  /**
   * @brief Trace constructor that attaches to the node using node pointer
   * @param os    reference to the output stream
   * @param node  pointer to the node
   */
  WindowTracer (boost::shared_ptr<std::ostream> os, Ptr<Node> node);
  
  /**
   * @brief Trace constructor that attaches to the node using node name
   * @param os        reference to the output stream
   * @param nodeName  name of the node registered using Names::Add
   */
  WindowTracer (boost::shared_ptr<std::ostream> os, const std::string &nodeName);

  /**
   * @brief Destructor
   */
  virtual ~WindowTracer ();

  virtual void
  PrintHeader (std::ostream &os) const;

  virtual void
  Print (std::ostream &os) const;
  /**
   * @brief Helper method to install tracers on all simulation nodes
   *
   * @param file File to which traces will be written
   * @param averagingPeriod How often data will be written into the trace file (default, every half second)
   *
   * @returns a tuple of reference to output stream and list of tracers. !!! Attention !!! This tuple needs to be preserved
   *          for the lifetime of simulation, otherwise SEGFAULTs are inevitable
   * 
   */
  static boost::tuple< boost::shared_ptr<std::ostream>, std::list<Ptr<WindowTracer> > >
  InstallAll (const std::string &file, Time averagingPeriod = Seconds (0.5));

protected: 
  // from L3Tracer

  virtual void
  Window  (std::string context, Ptr<ConsumerWindow> app);

protected:
  void
  Connect ();

  void
  SetAveragingPeriod (const Time &period);

  void
  Reset ();

  void
  PeriodicPrinter ();
  
protected:
  std::string m_node;
  Ptr<Node> m_nodePtr;
  boost::shared_ptr<std::ostream> m_os;

  Time m_period;
  EventId m_printEvent;
  uint32_t m_window;


};

inline std::ostream&
operator << (std::ostream &os, const WindowTracer &tracer)
{
  os << "# ";
  tracer.PrintHeader (os);
  os << "\n";
  tracer.Print (os);
  return os;
}

} // namespace ndn
} // namespace ns3

#endif // NDN_WINDOW_TRACER_H
