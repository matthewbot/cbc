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

#ifndef __WIRELESSADAPTER_H__
#define __WIRELESSADAPTER_H__

#include <QObject>
#include <QThread>
#include <QMetaType>
#include <QString>
#include <QStringList>

// Qt's stupid signal/slot system wouldn't work with this inside the class :/
enum WirelessAdapterStatus {
  NOT_DETECTED,
  NOT_UP,
  NOT_CONNECTED,
  SCANNING,
  CONNECTING,
  CONNECTED
};
Q_DECLARE_METATYPE(WirelessAdapterStatus)

class WirelessAdapter : public QThread {
Q_OBJECT

public:
  WirelessAdapter();
  ~WirelessAdapter();
  
  inline WirelessAdapterStatus getStatus() { return m_status; }
  inline const QString &getMACAddress() { return m_mac; }
  
public slots:
  void startScan();
  
signals:
  void statusChanged(WirelessAdapterStatus status);  
  void scanComplete(QStringList networks);
  
private:
  virtual void run();
  void updateStatus();
  void up();
  void doScan();

  bool m_startscan;

  void setStatus(WirelessAdapterStatus WirelessAdapaterStatus);
  WirelessAdapterStatus m_status;
  
  QString m_mac;
  QStringList m_networks;
};

#endif
