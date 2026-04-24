// gethostbyname
// beginMulticastPacket
// beginMulticast
//  igmp Multicast IP addresses: 224.0.0.0 and 239.255.255.255

#include "udp_server.h"
#include "esphome/core/log.h"
#include "esphome/core/defines.h"
#include "esphome/components/wifi/wifi_component.h"

#ifdef ESP32
#include <WiFi.h>
// #include <SPIFFS.h>
#elif defined(ESP8266)
#include <ESP8266WiFi.h>
#endif

#include <lwip/init.h>
#include <lwip/ip_addr.h>
#include <lwip/ip4_addr.h>
#include <lwip/igmp.h>

#ifdef USE_ESP32
#include <WiFi.h>
#endif

#ifdef USE_ESP8266
#include <ESP8266WiFi.h>
#include <WiFiUdp.h>
#endif

#ifdef USE_STORAGE
#include "esphome/components/storage/store.h"
#endif

namespace esphome
{
  namespace udp_server
  {

    std::string UdpServer::getModelName()
    {
      std::string modelName = "unknown";
#ifdef USE_STORAGE
      if (storage::store != nullptr)
      {
        modelName = storage::store->get_model();
      }
#endif
      return modelName;
    }

    uint8_t UdpServer::getModelNumber()
    {
      uint8_t modelNumber = DEVICE_MODEL_UNKNOWN;
#ifdef USE_STORAGE
      if (storage::store != nullptr)
      {
        modelNumber = storage::store->get_model_num();
        // storage::store->getBuildNumber(packet.build[0], packet.build[1]);
      }
#endif
      return modelNumber;
    }

    void UdpServer::sendSysInfo()
    {
      PacketSysInfo packet;
      get_mac_address_raw(packet.mac);
      IPAddress ip = WiFi.localIP();
      for (int i = 0; i < 4; i++)
        packet.ip[i] = ip[i];
      packet.channel = channel_;
      packet.build[0] = 0;
      packet.build[1] = 0;
#ifdef USE_STORAGE
      if (storage::store != nullptr)
      {
        storage::store->getBuildNumber(packet.build[0], packet.build[1]);
      }
#endif
      packet.time = millis();
      packet.model = this->getModelNumber();
      std::string modelName = this->getModelName();
      auto name = str_sprintf("%s %02X%02X%02X", modelName.c_str(), packet.mac[3], packet.mac[4], packet.mac[5]).c_str();
      strncpy(packet.name, name, sizeof(packet.name) - 1);
      sendMessage(true, PacketKind::SYS_INFO, reinterpret_cast<const uint8_t *>(&packet), sizeof(packet));
    }

    void UdpServer::sendStatusInfo()
    {
      PacketStatus packet = fillStatus();
      packet.event = KindStatusEvent::INTERVAL;
      sendStatus(packet);
    }

    void UdpServer::sendPingReq()
    {
      PacketPing packet;
      get_mac_address_raw(packet.mac);
      sendMessage(false, PacketKind::PING_REQ, reinterpret_cast<const uint8_t *>(&packet), sizeof(packet));
    }

    void UdpServer::sendPingRes()
    {
      PacketPing packet;
      get_mac_address_raw(packet.mac);
      sendMessage(false, PacketKind::PING_RES, reinterpret_cast<const uint8_t *>(&packet), sizeof(packet));
    }

    void UdpServer::sendIdentityInfo()
    {
      PacketIdentity packet = fillIdentity();
      IPAddress ip = WiFi.localIP();
      for (int i = 0; i < 4; i++)
        packet.ip[i] = ip[i];
      packet.time = millis();
      packet.model = this->getModelNumber();
      std::string modelName = this->getModelName();
      auto name = str_sprintf("%s %02X%02X%02X", modelName.c_str(), packet.mac[3], packet.mac[4], packet.mac[5]).c_str();
      strncpy(packet.name, name, sizeof(packet.name) - 1);
      sendIdentity(packet);
    }

    void UdpServer::sendControl(PacketControl packet)
    {
      get_mac_address_raw(packet.mac);
      sendMessage(false, PacketKind::CONTROL, reinterpret_cast<const uint8_t *>(&packet), sizeof(packet));
    }

    void UdpServer::sendStatus(PacketStatus packet)
    {
      get_mac_address_raw(packet.mac);
      sendMessage(false, PacketKind::STATUS, reinterpret_cast<const uint8_t *>(&packet), sizeof(packet));
    }

    void UdpServer::sendIdentity(PacketIdentity packet)
    {
      get_mac_address_raw(packet.mac);
      sendMessage(false, PacketKind::IDENTITY, reinterpret_cast<const uint8_t *>(&packet), sizeof(packet));
    }

    void UdpServer::sendReconfig(PacketReconfig packet)
    {
      get_mac_address_raw(packet.mac);
      sendMessage(false, PacketKind::RECONFIG, reinterpret_cast<const uint8_t *>(&packet), sizeof(packet));
    }

    PacketStatus UdpServer::fillStatus()
    {
      PacketStatus packet;
      get_mac_address_raw(packet.mac);
      packet.event = KindStatusEvent::INTERVAL;
      packet.radiation = storage::RadiationMode::NONE;
      packet.radiationSource = KindRadiationSource::SOURCE_UNKNOWN;
      packet.lamp = KindLampMode::OFF;
      packet.motion = false;
      packet.motionSource = KindMotionSource::UNKNOWN;
      this->status_fill_callback_.call(packet);
      return packet;
    }

    PacketIdentity UdpServer::fillIdentity()
    {
      PacketIdentity packet;
      get_mac_address_raw(packet.mac);
      this->identity_fill_callback_.call(packet);
      return packet;
    }

    UdpServer::UdpServer()
    {
      udpServer = this;
      // this->GlobalDevices = new DeviceList();
    }

    UdpServer::~UdpServer()
    {
      if (udp_main_)
      {
        udp_main_->stop();
      }
      if ((channel_ != 0) && udp_channel_)
      {
        udp_channel_->stop();
      }
    }

    void UdpServer::dump_config()
    {
      ESP_LOGCONFIG(TAG, "Udp Server:");
      ESP_LOGCONFIG(TAG, "  port: %u", port_);
      ESP_LOGCONFIG(TAG, "  channel: %u", channel_);
    }

    void UdpServer::setup()
    {
      this->set_timeout("SysInfoInit", 3000, [this]
                        { this->sendSysInfo(); });
      this->set_interval(MESSAGE_SYSINFO_REPEAT_SEC * 1000, [this]()
                         { this->sendSysInfo(); });
      this->set_interval(MESSAGE_STATUSINFO_REPEAT_SEC * 1000, [this]()
                         { this->sendStatusInfo(); });
      this->set_interval(MESSAGE_IDENTITYINFO_REPEAT_SEC * 1000, [this]()
                         { this->sendIdentityInfo(); });
    }

    IPAddress UdpServer::getMulticastIpforChannel(uint16_t channel)
    {
      if (channel > 100 && channel < 350)
        return IPAddress(230, 1, 1, channel - 100);
      if (channel > 500 && channel < 750)
        return IPAddress(230, 1, 2, channel - 500);
      return IPAddress(230, 2, channel >> 8, channel & 0xFF);
    }

    void UdpServer::changeChannel(uint16_t channel)
    {
      if ((channel_ != 0) && udp_channel_)
        udp_channel_->stop();

      channel_ = channel;

      if (channel_ != 0 && udp_main_)
        startMulticast(false);
    }

    IPAddress UdpServer::getIp(bool main)
    {
      if (main)
        return IPAddress(230, 0, 0, 1);
      else
        return getMulticastIpforChannel(channel_);
    }

    uint16_t UdpServer::getPort(bool main)
    {
      if (main)
        return port_;
      else
        return port_ + channel_;
    }

    void UdpServer::startMulticast(bool main)
    {
      std::unique_ptr<WiFiUDP> &udp = main ? udp_main_ : udp_channel_;
      udp = make_unique<WiFiUDP>();
      // if (!udp_channel_->beginMulticast(IPAddress(255,255,255,255), port_ + channel_))
      // if (!udp_channel_->begin(port_ + channel_))
      auto address = getIp(main);
      auto port = getPort(main);

      bool res = false;
#ifdef ESP32
      res = udp->beginMulticast(address, port);
#elif defined(ESP8266)
      res = udp->beginMulticast(WiFi.localIP(), address, port);
#endif
      if (!res)
      {
        ESP_LOGE(TAG, "Cannot bind UdpServer channel to %d.", port);
        mark_failed();
        return;
      }
    }

    void UdpServer::writePacket(bool main, const uint8_t *headerData, const uint8_t *bodyData, uint16_t bodyLen)
    {
      std::unique_ptr<WiFiUDP> &udp = main ? udp_main_ : udp_channel_;
#ifdef ESP32
      udp->beginMulticastPacket();
#elif defined(ESP8266)
      udp->beginPacketMulticast(getIp(main), getPort(main), WiFi.localIP());
#endif
      udp->write(headerData, sizeof(PacketHeader));
      udp->write(bodyData, bodyLen);
      udp->endPacket();
    }

    void UdpServer::processIncoming(bool main, std::unique_ptr<WiFiUDP> &udp)
    {
      std::vector<uint8_t> payload;
      PacketUdpServer packet;

      while (uint16_t packet_size = udp->parsePacket())
      {
        if (packet_size < sizeof(PacketHeader))
        {
          ESP_LOGD(TAG, "Ignoring small packet %s: from: %s:%u, size %zu.", main ? "main" : "channel", network::IPAddress(udp->remoteIP()).str().c_str(), udp->remotePort(), packet_size);
          continue;
        }

        packet.body.resize(packet_size - sizeof(PacketHeader));
        if (!udp->read(reinterpret_cast<unsigned char *>(&packet.header), sizeof(PacketHeader)) || !udp->read(packet.body.data(), packet.body.size()))
        {
          ESP_LOGD(TAG, "Error reading of packet %s: from: %s:%u, size %zu.", main ? "main" : "channel", network::IPAddress(udp->remoteIP()).str().c_str(), udp->remotePort(), packet_size);
          continue;
        }

        processPacket(main, udp->remoteIP(), udp->remotePort(), packet);
      }
    }

    void UdpServer::wifiConnect()
    {
      startMulticast(true);

      if (channel_ != 0)
        startMulticast(false);
    }

    void UdpServer::wifiDisconnect()
    {
    }

    void UdpServer::loop()
    {
      if (wifi::global_wifi_component->is_connected() != wifi_connected_)
      {
        wifi_connected_ = !wifi_connected_;
        if (wifi_connected_)
          wifiConnect();
        else
          wifiDisconnect();
      }

      if (!network::is_connected()) 
        return;

      processIncoming(true, udp_main_);
      if (channel_ != 0)
        processIncoming(false, udp_channel_);

      // uint16_t packet_size = udp_main_->parsePacket();
      // if (packet_size)
      // {
      //   ESP_LOGD(TAG, "Receive packet: size %zu.", packet_size);
      //   std::vector<uint8_t> payload;
      //   payload.resize(packet_size);
      //   udp_main_->read(&payload[0], payload.size());
      // }
    }

    // bool UdpServer::decodePacket(bool main, const std::vector<uint8_t> &data, PacketUdpServer &packet)
    // {
    //   // if (data.size() < E131_MIN_PACKET_SIZE)
    //   //   return false;

    //   // auto *sbuff = reinterpret_cast<const E131RawPacket *>(&data[0]);

    //   // if (memcmp(sbuff->acn_id, ACN_ID, sizeof(sbuff->acn_id)) != 0)
    //   //   return false;
    //   // if (htonl(sbuff->root_vector) != VECTOR_ROOT)
    //   //   return false;
    //   // if (htonl(sbuff->frame_vector) != VECTOR_FRAME)
    //   //   return false;
    //   // if (sbuff->dmp_vector != VECTOR_DMP)
    //   //   return false;
    //   // if (sbuff->property_values[0] != 0)
    //   //   return false;

    //   // universe = htons(sbuff->universe);
    //   // packet.count = htons(sbuff->property_value_count);
    //   // if (packet.count > E131_MAX_PROPERTY_VALUES_COUNT)
    //   //   return false;

    //   // memcpy(packet.values, sbuff->property_values, packet.count);
    //   return true;
    // }

    void UdpServer::processPacket(bool main, IPAddress remoteIP, uint16_t remotePort, PacketUdpServer &packet)
    {
      ESP_LOGD(TAG, "Receive packet %s: from: %s:%u, kind: %u, body size %zu.", main ? "main" : "channel", network::IPAddress(remoteIP).str().c_str(), remotePort, static_cast<uint8_t>(packet.header.packetKind), packet.body.size());

      switch (packet.header.packetKind)
      {
      case PacketKind::SYS_INFO:
        if (packet.body.size() == sizeof(PacketSysInfo))
        {
          // auto *data = packet.body.data();
          // PacketSysInfo *packetSysInfo = reinterpret_cast<PacketSysInfo *>(data);
          // // auto deviceItem = this->GlobalDevices.updateFromSysInfo(packetSysInfo);
          // auto deviceItem = new DeviceItem();
          // deviceItem->updateFromSysInfo(packetSysInfo);
          // this->neighbor_callback_.call(deviceItem);
        }
        break;
      case PacketKind::PING_REQ:
        if (packet.body.size() == sizeof(PacketPing))
        {
          auto *data = packet.body.data();
          PacketPing *packetPing = reinterpret_cast<PacketPing *>(data);
          auto mac = str_sprintf("%02X%02X%02X", packetPing->mac[3], packetPing->mac[4], packetPing->mac[5]).c_str();
          ESP_LOGI(TAG, "PingReq received %s.", mac);
          this->sendPingRes();
        }
        break;
      case PacketKind::PING_RES:
        if (packet.body.size() == sizeof(PacketPing))
        {
          auto *data = packet.body.data();
          PacketPing *packetPing = reinterpret_cast<PacketPing *>(data);
          auto mac = str_sprintf("%02X%02X%02X", packetPing->mac[3], packetPing->mac[4], packetPing->mac[5]).c_str();
          ESP_LOGI(TAG, "PingRes received %s.", mac);
        }
        break;
      case PacketKind::CONTROL:
        if (packet.body.size() == sizeof(PacketControl))
        {
          auto *data = packet.body.data();
          PacketControl *packetControl = reinterpret_cast<PacketControl *>(data);
          this->control_callback_.call(*packetControl);
        }
        break;
      case PacketKind::STATUS:
        if (packet.body.size() == sizeof(PacketStatus))
        {
          auto *data = packet.body.data();
          PacketStatus *packetStatus = reinterpret_cast<PacketStatus *>(data);
          this->status_callback_.call(*packetStatus);
        }
        break;
      case PacketKind::IDENTITY:
        if (packet.body.size() == sizeof(PacketIdentity))
        {
          auto *data = packet.body.data();
          PacketIdentity *packetIdentity = reinterpret_cast<PacketIdentity *>(data);
          this->identity_callback_.call(*packetIdentity);
        }
        break;
      case PacketKind::RECONFIG:
        if (packet.body.size() == sizeof(PacketReconfig))
        {
          auto *data = packet.body.data();
          PacketReconfig *packetReconfig = reinterpret_cast<PacketReconfig *>(data);
          this->reconfig_callback_.call(*packetReconfig);
        }
        break;
      default:
        ESP_LOGD(TAG, "Unknown packet kind %u.", static_cast<uint8_t>(packet.header.packetKind));
      }
    }

    void UdpServer::sendMessage(bool main, PacketKind kind, const uint8_t *bodyData, uint16_t bodyLen)
    {
      if (!udp_main_)
        return;

      PacketHeader packetHeader;
      packetHeader.mark[0] = UdpPacket_Mark[0];
      packetHeader.mark[1] = UdpPacket_Mark[1];
      packetHeader.protocol_ver = UDP_PROTOCOL_VERSION;
      packetHeader.packetKind = kind;
      const uint8_t *headerData = reinterpret_cast<const uint8_t *>(&packetHeader);

      if (main)
      {
        writePacket(main, headerData, bodyData, bodyLen);
        ESP_LOGD(TAG, "Send msg main: to: %s:%u, kind: %u, size %zu, channel: %u", network::IPAddress(getIp(main)).str().c_str(), getPort(main), static_cast<uint8_t>(packetHeader.packetKind), sizeof(packetHeader) + bodyLen, channel_);
      }
      else
      {
        if (channel_ != 0)
        {
          writePacket(main, headerData, bodyData, bodyLen);
          ESP_LOGD(TAG, "Send msg channel: to: %s:%u, kind: %u, size %zu, channel: %u", network::IPAddress(getIp(main)).str().c_str(), getPort(main), static_cast<uint8_t>(packetHeader.packetKind), sizeof(packetHeader) + bodyLen, channel_);
        }
      }
    }

    void UdpServer::sendControlRadiation(storage::RadiationMode mode, KindRadiationSource source)
    {
      PacketControl packet;
      packet.mode = mode;
      packet.source = source;
      sendControl(packet);
    }

    void UdpServer::sendStatusMotion(bool motion, KindMotionSource source)
    {
      PacketStatus packet = fillStatus();
      packet.event = KindStatusEvent::MOTION;
      packet.motion = motion;
      packet.motionSource = source;
      sendStatus(packet);
    }

    // void WakeOnLanButton::press_action() {
    //   ESP_LOGI(TAG, "Sending Wake-on-LAN Packet...");
    //   bool begin_status = false;
    //   bool end_status = false;
    //   uint32_t interface = esphome::network::get_ip_address();
    //   IPAddress interface_ip = IPAddress(interface);
    //   IPAddress broadcast = IPAddress(255, 255, 255, 255);
    // #ifdef USE_ESP8266
    //   begin_status = this->udp_client_.beginPacketMulticast(broadcast, 9, interface_ip, 128);
    // #endif
    // #ifdef USE_ESP32
    //   begin_status = this->udp_client_.beginPacket(broadcast, 9);
    // #endif

    //   if (begin_status) {
    //     this->udp_client_.write(PREFIX, 6);
    //     for (size_t i = 0; i < 16; i++) {
    //       this->udp_client_.write(macaddr_, 6);
    //     }
    //     end_status = this->udp_client_.endPacket();
    //   }
    //   if (!begin_status || end_status) {
    //     ESP_LOGE(TAG, "Sending Wake-on-LAN Packet Failed!");
    //   }
    // }

    UdpServer *udpServer = nullptr;

  } // namespace udp_server
} // namespace esphome

// #endif  // USE_ARDUINO
