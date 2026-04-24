#pragma once

// #ifdef USE_ARDUINO

#include "esphome/core/component.h"

#include "esphome/core/log.h"
#include "esphome/core/util.h"
#include "esphome/core/helpers.h"
#include "esphome/components/network/ip_address.h"
#include "esphome/components/network/util.h"
#include "esphome/core/automation.h"

#include "WiFiUdp.h"

#include <cstring>
#include <memory>
#include <set>
#include <map>

#include "device_list.h"

namespace esphome
{
  namespace udp_server
  {
    // // E1.31 Packet Structure
    // union E131RawPacket {
    //   struct {
    //     // Root Layer
    //     uint16_t preamble_size;
    //     uint16_t postamble_size;
    //     uint8_t acn_id[12];
    //     uint16_t root_flength;
    //     uint32_t root_vector;
    //     uint8_t cid[16];

    //     // Frame Layer
    //     uint16_t frame_flength;
    //     uint32_t frame_vector;
    //     uint8_t source_name[64];
    //     uint8_t priority;
    //     uint16_t reserved;
    //     uint8_t sequence_number;
    //     uint8_t options;
    //     uint16_t universe;

    //     // DMP Layer
    //     uint16_t dmp_flength;
    //     uint8_t dmp_vector;
    //     uint8_t type;
    //     uint16_t first_address;
    //     uint16_t address_increment;
    //     uint16_t property_value_count;
    //     uint8_t property_values[E131_MAX_PROPERTY_VALUES_COUNT];
    //   } __attribute__((packed));

    //   uint8_t raw[638];
    // };

    // // We need to have at least one `1` value
    // // Get the offset of `property_values[1]`
    // const size_t E131_MIN_PACKET_SIZE = reinterpret_cast<size_t>(&((E131RawPacket *) nullptr)->property_values[1]);

    // if (listen_method_ == E131_MULTICAST) {
    //   ip4_addr_t multicast_addr = {
    //       static_cast<uint32_t>(network::IPAddress(239, 255, ((universe >> 8) & 0xff), ((universe >> 0) & 0xff)))};

    class UdpServer : public esphome::Component
    {
    public:
      UdpServer();
      ~UdpServer();

      void setup() override;
      void loop() override;
      float get_setup_priority() const override { return setup_priority::AFTER_WIFI; }
      void dump_config() override;

      void changeChannel(uint16_t channel);

      void set_port(uint16_t port) { port_ = port; }
      void set_channel(uint16_t channel) { channel_ = channel; }
      uint16_t get_port() const { return port_; }
      uint16_t get_channel() const { return channel_; }

      static IPAddress getMulticastIpforChannel(uint16_t channel);

      void add_on_neighbor_callback(std::function<void(DeviceItem *deviceItem)> &&callback) { this->neighbor_callback_.add(std::move(callback)); }
      void add_on_control_callback(std::function<void(PacketControl packet)> &&callback) { this->control_callback_.add(std::move(callback)); }
      void add_on_status_callback(std::function<void(PacketStatus packet)> &&callback) { this->status_callback_.add(std::move(callback)); }
      void add_on_identity_callback(std::function<void(PacketIdentity packet)> &&callback) { this->identity_callback_.add(std::move(callback)); }
      void add_on_reconfig_callback(std::function<void(PacketReconfig packet)> &&callback) { this->reconfig_callback_.add(std::move(callback)); }
      void add_on_status_fill_callback(std::function<void(PacketStatus packet)> &&callback) { this->status_fill_callback_.add(std::move(callback)); }
      void add_on_identity_fill_callback(std::function<void(PacketIdentity packet)> &&callback) { this->identity_fill_callback_.add(std::move(callback)); }

      void sendSysInfo();
      void sendStatusInfo();
      void sendIdentityInfo();
      void sendPingReq();
      void sendPingRes();


      void sendControlRadiation(storage::RadiationMode mode, KindRadiationSource source);
      void sendStatusMotion(bool motion, KindMotionSource source);

      void sendControl(PacketControl packet);
      void sendStatus(PacketStatus packet);
      void sendIdentity(PacketIdentity packet);
      void sendReconfig(PacketReconfig packet);
      PacketStatus fillStatus();
      PacketIdentity fillIdentity();

      DeviceList GlobalDevices;

    protected:
      std::string getModelName();
      uint8_t getModelNumber();

      void wifiConnect();
      void wifiDisconnect();

      void processIncoming(bool main, std::unique_ptr<WiFiUDP> &udp);
      void processPacket(bool main, IPAddress remoteIP, uint16_t remotePort, PacketUdpServer &packet);

      void sendMessage(bool main, PacketKind kind, const uint8_t *bodyData, uint16_t bodyLen);

      IPAddress getIp(bool main);
      uint16_t getPort(bool main);

      void startMulticast(bool main);
      void writePacket(bool main, const uint8_t *headerData, const uint8_t *bodyData, uint16_t bodyLen);

      CallbackManager<void(DeviceItem *deviceItem)> neighbor_callback_{};
      CallbackManager<void(PacketControl packet)> control_callback_{};
      CallbackManager<void(PacketStatus packet)> status_callback_{};
      CallbackManager<void(PacketIdentity packet)> identity_callback_{};
      CallbackManager<void(PacketReconfig packet)> reconfig_callback_{};
      CallbackManager<void(PacketStatus packet)> status_fill_callback_{};
      CallbackManager<void(PacketIdentity packet)> identity_fill_callback_{};

      std::unique_ptr<WiFiUDP> udp_main_;
      std::unique_ptr<WiFiUDP> udp_channel_;
      uint16_t port_{30100};
      uint16_t channel_{0};
      bool wifi_connected_{false};
    };

    // template <typename... Ts>
    // class RadiationAction : public Action<Ts...>, public Parented<UdpServer>
    // {
    // public:
    //   TEMPLATABLE_VALUE(storage::RadiationMode, mode)

    //   void play(Ts... x) override
    //   {
    //     auto mode = this->mode_.value(x...);
    //     this->parent_->sendRadiation(mode);
    //   }
    // };

    // template <typename... Ts>
    // class MotionAction : public Action<Ts...>, public Parented<UdpServer>
    // {
    // public:
    //   TEMPLATABLE_VALUE(bool, state)

    //   void play(Ts... x) override
    //   {
    //     auto state = this->state_.value(x...);
    //     this->parent_->sendMotion(state);
    //   }
    // };

    class UdpControlNotifyTrigger : public Trigger<PacketControl>
    {
    public:
      explicit UdpControlNotifyTrigger(UdpServer *parent)
      {
        parent->add_on_control_callback([this](PacketControl packet)
                                        { this->trigger(packet); });
      }
    };

    class UdpStatusNotifyTrigger : public Trigger<PacketStatus>
    {
    public:
      explicit UdpStatusNotifyTrigger(UdpServer *parent)
      {
        parent->add_on_status_callback([this](PacketStatus packet)
                                       { this->trigger(packet); });
      }
    };

    class UdpIdentityNotifyTrigger : public Trigger<PacketIdentity>
    {
    public:
      explicit UdpIdentityNotifyTrigger(UdpServer *parent)
      {
        parent->add_on_identity_callback([this](PacketIdentity packet)
                                         { this->trigger(packet); });
      }
    };

    class UdpReconfigNotifyTrigger : public Trigger<PacketReconfig>
    {
    public:
      explicit UdpReconfigNotifyTrigger(UdpServer *parent)
      {
        parent->add_on_reconfig_callback([this](PacketReconfig packet)
                                         { this->trigger(packet); });
      }
    };

    class UdpStatusFillTrigger : public Trigger<PacketStatus>
    {
    public:
      explicit UdpStatusFillTrigger(UdpServer *parent)
      {
        parent->add_on_status_fill_callback([this](PacketStatus packet)
                                            { this->trigger(packet); });
      }
    };

    class UdpIdentityFillTrigger : public Trigger<PacketIdentity>
    {
    public:
      explicit UdpIdentityFillTrigger(UdpServer *parent)
      {
        parent->add_on_identity_fill_callback([this](PacketIdentity packet)
                                              { this->trigger(packet); });
      }
    };
    extern UdpServer *udpServer;

  } // namespace udp_server
} // namespace esphome

// #endif  // USE_ARDUINO
