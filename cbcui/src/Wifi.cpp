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

#define DATA_SSID Qt::UserRole

Wifi::Wifi(QWidget *parent) : Page(parent) {
  setupUi(this);
  ui_networkList->clear();
  wireless_statusChanged(); 
  
  QObject::connect(ui_refreshButton, SIGNAL(pressed()), &wireless, SLOT(startScan()));
  
  QObject::connect(&wireless, SIGNAL(statusChanged()), this, SLOT(wireless_statusChanged()));
  QObject::connect(&wireless, SIGNAL(scanComplete()), this, SLOT(wireless_scanComplete()));
}
Wifi::~Wifi() { }

void Wifi::on_ui_connectButton_pressed() {
  QString ssid = ui_networkList->item(ui_networkList->currentRow())->data(DATA_SSID).toString();
  
  WirelessConnectionSettings connsettings;
  connsettings.ssid = ssid;
  connsettings.encryption = WirelessConnectionSettings::OPEN;
  wireless.startConnect(connsettings);
}

void Wifi::wireless_statusChanged() {
  const WirelessAdapterStatus &status = wireless.getStatus();
  
  if (status.adapterstate != WirelessAdapterStatus::UP)
    ui_networkList->clear();
  else if (status.scanning) {
    ui_networkList->clear();
    ui_networkList->addItem("Scanning...");
  }
  
  if (status.connectionstate == WirelessAdapterStatus::NOT_CONNECTED)
    ui_ssidLabel->setText("");
  else if (status.connectionstate == WirelessAdapterStatus::CONNECTING)
    ui_ssidLabel->setText("Connecting...");
  else
    ui_ssidLabel->setText(status.ssid);
    
  if (status.connectionstate < WirelessAdapterStatus::OBTAINING_IP)
    ui_ipLabel->setText("");
  else if (status.connectionstate == WirelessAdapterStatus::OBTAINING_IP)
    ui_ipLabel->setText("Obtaining...");
  else
    ui_ipLabel->setText(status.ip);
  
  if (status.adapterstate == WirelessAdapterStatus::NOT_DETECTED)
    ui_adapterLabel->setText("No wireless adapter detected");
  else if (status.adapterstate == WirelessAdapterStatus::NOT_UP)
    ui_adapterLabel->setText("Starting wireless adapter...");
  else
    ui_adapterLabel->setText("Adapter OK! MAC: " + status.mac);
}

void Wifi::wireless_scanComplete() {
  ui_networkList->clear();
  
  const QList<ScanResult> &scanresults = wireless.getScanResults();
  for (QList<ScanResult>::const_iterator i = scanresults.begin(); i != scanresults.end(); ++i) {
    QString buf;
    QTextStream stream(&buf);
    
    stream << i->ssid;
    stream << " - " << i->quality << "/100";
    if (i->encrypted)
      stream << " (encrypted)";
    stream.flush();
    
    QListWidgetItem *listitem = new QListWidgetItem(buf);
    listitem->setData(DATA_SSID, i->ssid);
    ui_networkList->addItem(listitem);
  }
}

