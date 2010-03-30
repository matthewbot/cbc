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

#include "Wifi.h"
#include <QDebug>

Wifi::Wifi(QWidget *parent) : Page(parent) {
  setupUi(this);
  
  QObject::connect(&wireless, SIGNAL(statusChanged(WirelessAdapterStatus)), this, SLOT(wireless_statusChanged(WirelessAdapterStatus)));
}
Wifi::~Wifi() { }

void Wifi::wireless_statusChanged(WirelessAdapterStatus status) {
  switch (status) {
    case NOT_DETECTED:
      ui_adapterLabel->setText("No wireless adapter detected");
      break;
      
    case NOT_UP:
      ui_adapterLabel->setText("Starting wireless adapter...");
      break;
      
    case NOT_CONNECTED:
      ui_adapterLabel->setText("Adapter OK! MAC: " + wireless.getMACAddress());
      break;
  }
}

