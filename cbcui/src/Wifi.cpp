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
  wireless_statusChanged(NOT_DETECTED);
  
  QObject::connect(ui_refreshButton, SIGNAL(pressed()), &wireless, SLOT(startScan()));
  
  QObject::connect(&wireless, SIGNAL(statusChanged(WirelessAdapterStatus)), this, SLOT(wireless_statusChanged(WirelessAdapterStatus)));
  QObject::connect(&wireless, SIGNAL(scanComplete(QStringList)), this, SLOT(wireless_scanComplete(QStringList)));
}
Wifi::~Wifi() { }

void Wifi::on_ui_connectButton_pressed() {
  QString ssid = ui_networkList->item(ui_networkList->currentRow())->text();
  wireless.startConnect(ssid);
}

void Wifi::wireless_statusChanged(WirelessAdapterStatus status) {
  switch (status) {
    case NOT_DETECTED:
      ui_adapterLabel->setText("No wireless adapter detected");
      ui_networkList->clear();
      ui_ssidLabel->setText("");
      ui_ipLabel->setText("");
      break;
      
    case NOT_UP:
      ui_adapterLabel->setText("Starting wireless adapter...");
      break;
      
    case NOT_CONNECTED:
      ui_adapterLabel->setText("Adapter OK! MAC: " + wireless.getMACAddress());
      ui_ssidLabel->setText("");
      ui_ipLabel->setText("");
      break;
      
    case SCANNING:
      ui_networkList->clear();
      ui_networkList->addItem("Scanning...");
      break;
      
    case CONNECTING:
      ui_adapterLabel->setText("Connecting...");
      break;
      
    case OBTAINING_IP:
      ui_adapterLabel->setText("Connected");
      ui_ssidLabel->setText(wireless.getSSID());
      ui_ipLabel->setText("Obtaining...");
      break;
  }
}

void Wifi::wireless_scanComplete(QStringList networks) {
  ui_networkList->clear();
  
  for (int i=0; i < networks.size(); ++i)
    ui_networkList->addItem(networks[i]);
}

