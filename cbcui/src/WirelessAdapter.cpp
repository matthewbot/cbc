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

WirelessAdapter::WirelessAdapter() : m_status(NOT_DETECTED) {
  qRegisterMetaType<WirelessAdapterStatus>("WirelessAdapterStatus");
  start();
}
WirelessAdapter::~WirelessAdapter() { }

void WirelessAdapter::run() {
  while (true) {
    QThread::msleep(1000);
    
    updateStatus();
    
    if (m_status == NOT_UP)
      up();
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

void WirelessAdapter::setStatus(WirelessAdapterStatus status) {
  if (m_status != status) {
    m_status = status;
    emit statusChanged(status);
  }
}

