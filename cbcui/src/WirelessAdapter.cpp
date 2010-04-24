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

WirelessAdapter::WirelessAdapter() : m_startscan(false)
{
  updateStatus();
  start();
}
WirelessAdapter::~WirelessAdapter() { }

void WirelessAdapter::startScan() 
{
  m_startscan = true;
}

void WirelessAdapter::startConnect(WirelessConnectionSettings connsettings) 
{
  m_connsettings = connsettings;
  m_startconnect = true;
}

void WirelessAdapter::run() 
{
  while (true) {
    QThread::msleep(1000);
    
    WirelessAdapterStatus oldstatus = m_status;
    updateStatus();
    if (oldstatus != m_status)
      statusChanged();
    
    if (m_status.adapterstate == WirelessAdapterStatus::NOT_UP)
      up();
    
    if (m_status.adapterstate < WirelessAdapterStatus::UP)
      continue;
    
    if (m_startscan || oldstatus.adapterstate == WirelessAdapterStatus::NOT_UP) {
      doScan();
      m_startscan = false;
    }
    
    if (m_startconnect) {
      doConnect();
      m_startconnect = false;
    }
  }
}

static const QRegExp essid_regexp("ESSID:\"(\\w+)\"");

void WirelessAdapter::updateStatus()
{
  m_status = WirelessAdapterStatus();

  QProcess ifconfig;
  ifconfig.start("ifconfig rausb0");
  ifconfig.waitForFinished();
  if (ifconfig.exitCode() != 0) {
    m_status.adapterstate = WirelessAdapterStatus::NOT_DETECTED;
    return;
  }
  
  QString out = ifconfig.readAllStandardOutput();
  if (!out.contains("UP")) {
    m_status.adapterstate = WirelessAdapterStatus::NOT_UP;
    return;
  }
  m_status.adapterstate = WirelessAdapterStatus::UP;
  
  static const QRegExp mac_regexp("(?:..:){5}..");
  if (mac_regexp.indexIn(out) != -1)
    m_status.mac = mac_regexp.cap(0);
  else
    m_status.mac = "Unknown";
    
  QProcess iwconfig;
  iwconfig.start("iwconfig rausb0");
  iwconfig.waitForFinished();
  QString iwout = iwconfig.readAllStandardOutput();
  
  if (essid_regexp.indexIn(iwout) != -1)
    m_status.ssid = essid_regexp.cap(1);
  else {
    m_status.connectionstate = WirelessAdapterStatus::NOT_CONNECTED;
    return;
  }
  
  static const QRegExp ip_regexp("inet addr:(\\S+)");
  if (ip_regexp.indexIn(out) != -1)
    m_status.ip = ip_regexp.cap(1);
  else {
    m_status.connectionstate = WirelessAdapterStatus::NOT_CONNECTED;
    return;
  }
  
  m_status.connectionstate = WirelessAdapterStatus::CONNECTED;
  return;
}

void WirelessAdapter::up() 
{
  QProcess::execute("ifconfig rausb0 up");
}

void WirelessAdapter::doScan() 
{
  m_status.scanning = true;
  statusChanged();

  QProcess iwlist;
  iwlist.start("iwlist rausb0 scan");
  iwlist.waitForFinished();
  QString out = iwlist.readAllStandardOutput();
  
  static const QRegExp scan_regexp("(\\w[\\w\\s]+):\\s?\"?([\\w\\d]+)");
  
  int pos=0;
  m_scanresults.clear();
  
  QStringList lines = out.split("\n");
  for (int i=0; i<lines.size(); i++) {
    if (scan_regexp.indexIn(lines[i]) == -1) { continue; }
    const QString &key = scan_regexp.cap(1);
    const QString &value = scan_regexp.cap(2);
    
    if (key == "ESSID") {
      m_scanresults.append(ScanResult());
      m_scanresults.back().ssid = value;
    } else if (key == "Encryption key" && !m_scanresults.isEmpty()) {
      m_scanresults.back().encrypted = (value == "on");
    } else if (key == "Quality" && !m_scanresults.isEmpty()) {
      m_scanresults.back().quality = value.toInt();
    }
    
    pos += scan_regexp.matchedLength();
  }
  
  scanComplete();
  m_status.scanning = false;
  statusChanged();
}

void WirelessAdapter::doConnect() {
  m_status.connectionstate = WirelessAdapterStatus::CONNECTING;
  statusChanged();
  
  QProcess::execute("iwpriv rausb0 set TxRate=6");
  QString &ssid = m_connsettings.ssid;
  QProcess::execute("iwpriv rausb0 set SSID=" + ssid);
  
  switch (m_connsettings.encryption) {
    case WirelessConnectionSettings::WEP:
      QProcess::execute("iwpriv rausb0 set AuthMode=WEPAUTO");
      QProcess::execute("iwpriv rausb0 set EncrypType=WEP");
      QProcess::execute("iwpriv rausb0 set Key1=" + m_connsettings.key);
      break;
      
    case WirelessConnectionSettings::WPA:
      QProcess::execute("iwpriv rausb0 set AuthMode=WPAPSK");
      QProcess::execute("iwpriv rausb0 set EncrypType=TKIP");
      QProcess::execute("iwpriv rausb0 set WPAPSK=" + m_connsettings.key);
      break;
      
    case WirelessConnectionSettings::WPA2:
      QProcess::execute("iwpriv rausb0 set AuthMode=WPA2PSK");
      QProcess::execute("iwpriv rausb0 set EncrypType=AES");
      QProcess::execute("iwpriv rausb0 set WPAPSK=" + m_connsettings.key);
      break;
      
    default:
      break;
  }
  
  int i;
  for (i=0;i<6;i++) {
    QProcess iwconfig; // check first, we may already be associated
    iwconfig.start("iwconfig rausb0");
    iwconfig.waitForFinished();
    QString out = iwconfig.readAllStandardOutput();
    
    if (essid_regexp.indexIn(out) != -1 && essid_regexp.cap(1) == ssid) {
      m_status.ssid = ssid;
      doObtainIP();
      return;
    }
    
    QProcess::execute("iwpriv rausb0 set SSID=" + ssid);
    QThread::msleep(500);
  }
  
  m_status.connectionstate = WirelessAdapterStatus::NOT_CONNECTED;
  statusChanged();
}

void WirelessAdapter::doObtainIP() {
  m_status.connectionstate = WirelessAdapterStatus::OBTAINING_IP;
  statusChanged();
  QProcess::execute("killall udhcpc");
  
  QProcess udhcpc;
  udhcpc.start("udhcpc -q -f -n -i rausb0");
  udhcpc.waitForFinished();
  
  if (udhcpc.exitCode() != 0) {
    m_status.connectionstate = WirelessAdapterStatus::NOT_CONNECTED;
    statusChanged();
    return;
  }
  
  QString out = udhcpc.readAllStandardOutput();
  
  static const QRegExp lease_regexp("Lease of ((?:\\d{1,3}\\.){3}\\d{1,3}) obtained");
  if (lease_regexp.indexIn(out) == -1) {
    m_status.connectionstate = WirelessAdapterStatus::NOT_CONNECTED;
    statusChanged();
    return;
  }
  
  m_status.connectionstate = WirelessAdapterStatus::CONNECTED;
  m_status.ip = lease_regexp.cap(1);
  statusChanged();
}

WirelessAdapterStatus::WirelessAdapterStatus()
: adapterstate(NOT_DETECTED),
  connectionstate(NOT_CONNECTED),
  scanning(false) { }
  
bool WirelessAdapterStatus::operator==(const WirelessAdapterStatus &other) const {
  if (adapterstate != other.adapterstate || connectionstate != other.connectionstate)
    return false;
    
  if (scanning != other.scanning)
    return false;
    
  if (ssid != other.ssid || ip != other.ip || mac != other.mac)
    return false;
    
  return true;
}

