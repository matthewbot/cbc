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
#include <QMessageBox>
#include <QFile>

#define DATA_SSID Qt::UserRole
#define DATA_ENCRYPTED (Qt::UserRole+1)

Wifi::Wifi(QWidget *parent) 
: Page(parent), 
  m_settings("/mnt/user/cbc_v2.config", QSettings::NativeFormat),
  m_autoconnect(true)
{
  setupUi(this);
  ui_networkList->clear();
  wireless_statusChanged(); 
  
  QObject::connect(ui_refreshButton, SIGNAL(pressed()), &wireless, SLOT(startScan()));
  
  QObject::connect(&wireless, SIGNAL(statusChanged()), this, SLOT(wireless_statusChanged()));
  QObject::connect(&wireless, SIGNAL(scanComplete()), this, SLOT(wireless_scanComplete()));
}
Wifi::~Wifi() { }

void Wifi::on_ui_connectButton_pressed() {
  QListWidgetItem *item = ui_networkList->currentItem();
  QString ssid = item->data(DATA_SSID).toString();
  bool encrypted = item->data(DATA_ENCRYPTED).toBool();
  doConnect(ssid, encrypted);
}

void Wifi::wireless_statusChanged() {
  const WirelessAdapterStatus &status = wireless.getStatus();
  
  if (status.adapterstate != WirelessAdapterStatus::UP) {
    m_autoconnect = true;
    ui_networkList->clear();
  } else if (status.scanning) {
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
    listitem->setData(DATA_ENCRYPTED, i->encrypted);
    ui_networkList->addItem(listitem);
  }
  
  if (m_autoconnect) {
    m_autoconnect = false;
    doAutoConnect();
  }
}

void Wifi::doConnect(const QString &ssid, bool encrypted) {
  WirelessConnectionSettings connsettings;
  connsettings.ssid = ssid;
  if (encrypted) {
    loadKey(ssid);
    
    int type = m_settings.value("Wifi/" + ssid + "_type", -1).toInt();
    
    if (type == -1) {
      QMessageBox::warning(this, "Connection error", 
        "No encryption key for '" + ssid + "' "
        "Place a proper key file called '" + ssid + ".txt' "
        "on a thumb drive, mount it, and try again");
      return;
    }
    connsettings.encryption = (WirelessConnectionSettings::EncryptionType)type;
    connsettings.key = m_settings.value("Wifi/" + ssid + "_key", "").toString();
  } else {
    connsettings.encryption = WirelessConnectionSettings::OPEN;
  }
  
  wireless.startConnect(connsettings);
  
  m_settings.setValue("Wifi/last_ssid", ssid);
  m_settings.setValue("Wifi/last_ssid_encrypted", encrypted);
  m_settings.sync();
}

void Wifi::loadKey(const QString &ssid) {
  QFile keyfile("/mnt/browser/usb/" + ssid + ".txt");
  if (!keyfile.open(QIODevice::ReadOnly | QIODevice::Text))
    return;
    
  QString typestr = keyfile.readLine().trimmed();
  WirelessConnectionSettings::EncryptionType type;
  if (typestr == "WEP") {
    type = WirelessConnectionSettings::WEP;
  } else if (typestr == "WPA") {
    type = WirelessConnectionSettings::WPA;
  } else if (typestr == "WPA2") {
    type = WirelessConnectionSettings::WPA2;
  } else {
    QMessageBox::warning(this, "Key file error", 
        "Unknown encryption type '" + typestr + "'");
    return;
  }
  
  QString key = keyfile.readLine().trimmed();
  
  m_settings.setValue("Wifi/" + ssid + "_type", (int)type);
  m_settings.setValue("Wifi/" + ssid + "_key", key);
  m_settings.sync();
}

void Wifi::doAutoConnect() {
  if (!m_settings.contains("Wifi/last_ssid"))
    return;

  QString last_ssid = m_settings.value("Wifi/last_ssid").toString();
  bool last_ssid_encrypted = m_settings.value("Wifi/last_ssid_encrypted").toBool();
  bool seen_last_ssid = false;
  
  const QList<ScanResult> &scanresults = wireless.getScanResults();
  for (QList<ScanResult>::const_iterator i = scanresults.begin(); i != scanresults.end(); ++i) {
    if (i->ssid == last_ssid) {
      seen_last_ssid = true;
      break;
    }
  }
  
  if (!seen_last_ssid)
    return;
    
  doConnect(last_ssid, last_ssid_encrypted);
}

