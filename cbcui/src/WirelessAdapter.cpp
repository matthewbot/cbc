/**************************************************************************
 *  Copyright 2008,2009 KISS Institute for Practical Robotics             *
 *                                                                        *
 *  This file is part of CBC Firmware.                                    *
 *                                                                        *
 *  CBC Firmware is free software: you can redistribute it and/or modify  *
 *  it under the terms of the GNU General Public License as published by  *
 *  the Free Software Foundation, either version 2 of the License, or     *
 *  (at your option) any later version.                                   *
 *                                                                        *
 *  CBC Firmware is distributed in the hope that it will be useful,       *
 *  but WITHOUT ANY WARRANTY; without even the implied warranty of        *
 *  MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the         *
 *  GNU General Public License for more details.                          *
 *                                                                        *
 *  You should have received a copy of the GNU General Public License     *
 *  along with this copy of CBC Firmware.  Check the LICENSE file         *
 *  in the project root.  If not, see <http://www.gnu.org/licenses/>.     *
 **************************************************************************/

// Author: Matthew Thompson (matthewbot@gmail.com)

#include "WirelessAdapter.h"
#include <QProcess>
#include <QRegExp>
#include <QDebug>

WirelessAdapter::WirelessAdapter() : m_startscan(false), m_status(NOT_DETECTED) {
  qRegisterMetaType<WirelessAdapterStatus>("WirelessAdapterStatus");
  start();
}
WirelessAdapter::~WirelessAdapter() { }

void WirelessAdapter::startScan() {
  m_startscan = true;
}

void WirelessAdapter::run() {
  while (true) {
    QThread::msleep(1000);
    
    updateStatus();
    
    if (m_status == NOT_UP)
      up();
    else if (m_status == NOT_CONNECTED || m_status == CONNECTED) {
      if (m_startscan) {
        doScan();
        m_startscan = false;
      }
    }
  }
}

void WirelessAdapter::up() 
{
  QProcess::execute("ifconfig rausb0 up");
}

void WirelessAdapter::updateStatus()
{
  QProcess ifconfig;
  ifconfig.start("ifconfig rausb0");
  ifconfig.waitForFinished();
  if (ifconfig.exitCode() != 0) {
    m_startscan = true; // when we get plugged in we'll do one autoscan
    setStatus(NOT_DETECTED);
    return;
  }
  
  QString out = ifconfig.readAllStandardOutput();

  if (!out.contains("UP")) {
    setStatus(NOT_UP);
    return;
  }
  
  static const QRegExp mac_regexp("(?:..:){5}..");
  if (mac_regexp.indexIn(out) != -1)
    m_mac = mac_regexp.cap(0);
  else
    m_mac = "Unknown";
    
  setStatus(NOT_CONNECTED);
}

void WirelessAdapter::doScan() {
  WirelessAdapterStatus prev_stat = m_status;
  setStatus(SCANNING);

  QProcess iwlist;
  iwlist.start("iwlist rausb0 scan");
  iwlist.waitForFinished();
  QString out = iwlist.readAllStandardOutput();
  
  static const QRegExp essid_regexp("ESSID:\"([^\"]+)\"");
  
  int pos=0;
  m_networks.clear();
  while ((pos = essid_regexp.indexIn(out, pos)) != -1) {
    m_networks += essid_regexp.cap(1);
    pos += essid_regexp.matchedLength();
  }
  
  scanComplete(m_networks);
  setStatus(prev_stat);
}

void WirelessAdapter::setStatus(WirelessAdapterStatus status) {
  if (m_status != status) {
    m_status = status;
    emit statusChanged(status);
  }
}

