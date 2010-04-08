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

#ifndef __WIFI_H__
#define __WIFI_H__

#include <QSettings>
#include "ui_Wifi.h"
#include "Page.h"
#include "WirelessAdapter.h"

class Wifi : public Page, private Ui::Wifi
{
  Q_OBJECT
public:
  Wifi(QWidget *parent = 0);
  virtual ~Wifi();
  
public slots:
  void wireless_statusChanged();
  void wireless_scanComplete();
  void on_ui_connectButton_pressed();
  
private:
  void doConnect(const QString &ssid, bool encrypted);
  void loadKey(const QString &ssid);
  void doAutoConnect();

  WirelessAdapter wireless;
  QSettings m_settings;
  bool m_autoconnect;
};

#endif
