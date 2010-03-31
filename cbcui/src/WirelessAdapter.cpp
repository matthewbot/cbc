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

void WirelessAdapter::startConnect(QString ssid) {
  m_connectssid = ssid;
  m_startconnect = true;
}

void WirelessAdapter::run() {
  while (true) {
    QThread::msleep(1000);
    
    updateStatus();
    
    if (m_status == NOT_DETECTED)
      continue;
    else if (m_status == NOT_UP) {
      up();
      continue;
    }
      
    if (m_startscan) {
      doScan();
      m_startscan = false;
    }
    
    if (m_startconnect) {
      doConnect(m_connectssid);
      m_startconnect = false;
    }
  }
}

// used in a few functions
static const QRegExp essid_regexp("ESSID:\"(\\w+)\"");

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
    
  QProcess iwconfig;
  iwconfig.start("iwconfig rausb0");
  iwconfig.waitForFinished();
  QString iwout = iwconfig.readAllStandardOutput();
  
  if (essid_regexp.indexIn(iwout) != -1)
    m_ssid = essid_regexp.cap(1);
  else 
    m_ssid = "";
    
  if (m_ssid.length() == 0) {
    setStatus(NOT_CONNECTED);
    return;
  }
  
  static const QRegExp ip_regexp("inet addr:(\\S+)");
  if (ip_regexp.indexIn(out) != -1)
    m_ip = ip_regexp.cap(1);
  else
    m_ip = "";
  
  if (m_ip.length() == 0) {
    setStatus(NOT_CONNECTED);
    return;
  }
  
  setStatus(CONNECTED);
}

void WirelessAdapter::doScan() {
  WirelessAdapterStatus prev_stat = m_status;
  setStatus(SCANNING);

  QProcess iwlist;
  iwlist.start("iwlist rausb0 scan");
  iwlist.waitForFinished();
  QString out = iwlist.readAllStandardOutput();
  
  int pos=0;
  m_networks.clear();
  while ((pos = essid_regexp.indexIn(out, pos)) != -1) {
    m_networks += essid_regexp.cap(1);
    pos += essid_regexp.matchedLength();
  }
  
  scanComplete(m_networks);
  setStatus(prev_stat);
}

void WirelessAdapter::doConnect(const QString &ssid) {
  setStatus(CONNECTING);
  
  /* For now, you must hardcode a WEP key here
  QProcess::execute("iwpriv rausb0 set AuthMode=WEPAUTO");
  QProcess::execute("iwpriv rausb0 set EncrypType=WEP");
  QProcess::execute("iwpriv rausb0 set Key1=<<Insert your key>>");
  For you WPA2 users, you'll have to fish it out of the chumby scripts :P
  */
  
  int i;
  for (i=0;i<6;i++) {
    QProcess iwconfig; // check first, we may already be associated
    iwconfig.start("iwconfig rausb0");
    iwconfig.waitForFinished();
    QString out = iwconfig.readAllStandardOutput();
    
    if (essid_regexp.indexIn(out) != -1 && essid_regexp.cap(1) == ssid) {
      m_ssid = ssid;
      doObtainIP();
      return;
    }
    
    QProcess::execute("iwpriv rausb0 set SSID=" + ssid);
    QThread::msleep(500);
  }
  
  setStatus(NOT_CONNECTED);
}

void WirelessAdapter::doObtainIP() {
  setStatus(OBTAINING_IP);
  QProcess::execute("killall udhcpc");
  
  QProcess udhcpc;
  udhcpc.start("udhcpc -q -f -n -i rausb0");
  udhcpc.waitForFinished();
  if (udhcpc.exitCode() == 0)
    setStatus(CONNECTED);
  else {
    setStatus(NOT_CONNECTED);
  }
}

void WirelessAdapter::setStatus(WirelessAdapterStatus status) {
  if (m_status != status) {
    m_status = status;
    emit statusChanged(status);
  }
}

