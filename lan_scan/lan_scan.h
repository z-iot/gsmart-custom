#pragma once

#ifdef USE_ESP32

#include "esphome/core/component.h"
#include "esphome/components/json/json_util.h"

#include <cstdint>
#include <string>
#include <utility>
#include <vector>

namespace esphome {
namespace lan_scan {

/// Ports an ESP answers on while it runs an OTA server: 3232 on ESP32,
/// 8266 on ESP8266. They are the cheapest proof that something on the LAN is
/// one of ours - a lamp whose HTTP API is dead still listens there, and that is
/// exactly the kind of unit this scan exists to find.
constexpr uint16_t kOtaPortEsp32 = 3232;
constexpr uint16_t kOtaPortEsp8266 = 8266;
constexpr uint16_t kHttpPort = 80;

/// Hard ceiling on results, whatever the caller asked for. Every row costs heap
/// on a device that is also holding a TLS cloud socket, so the page size is a
/// budget, not a preference.
constexpr uint8_t kMaxResults = 32;
constexpr uint8_t kDefaultLimit = 20;

/// Concurrent connect() probes. Eight keeps a /24 sweep near twenty seconds
/// without ever parking the loop: each pass through `loop()` only polls sockets
/// that are already open. Must stay even - a host is always started as a pair,
/// one probe per OTA port.
constexpr uint8_t kMaxInFlight = 8;

constexpr uint32_t kConnectTimeoutMs = 450;
constexpr uint32_t kHttpTimeoutMs = 2500;

/// Cap on one HTTP body. `/info` and `/status` are well under it; anything that
/// runs over is a device answering with something we did not ask for.
constexpr size_t kMaxBodyBytes = 6144;

enum class JobState : uint8_t { IDLE, SWEEPING, IDENTIFYING, DONE, STOPPED, FAILED };

const char *job_state_to_string(JobState state);

/// One unit the sweep found, in the order its IP falls on the segment.
struct Neighbour {
  uint32_t ip{0};  ///< host byte order
  uint16_t ota_port{0};
  uint8_t mac[6]{};
  bool mac_known{false};

  /// Filled from the target's own API. Everything here stays empty for a unit
  /// that only answered on the OTA port - which is still a useful result, it
  /// says "there is an ESP here that will not talk".
  bool api_ok{false};
  std::string api_base;  ///< "/api/g-node/v1" or "/api/mobile/v1"
  std::string model;
  uint8_t model_num{0};
  std::string serial;
  std::string firmware;
  std::string name;
  bool is_emitter{false};

  std::string region_id;
  bool region_active{false};
  bool region_master{false};

  bool scheduler_known{false};
  bool scheduler_active{false};
  uint16_t scheduler_items{0};

  std::string mode;
  bool radiate{false};
};

/// What to sweep. The caller gives a base network and a starting host octet;
/// paging is "start again from where the last page stopped", not an offset into
/// a list nobody keeps.
struct ScanRequest {
  uint32_t network{0};   ///< network address in host byte order
  uint8_t prefix{24};
  uint8_t from_host{1};
  uint8_t to_host{254};
  uint8_t limit{kDefaultLimit};
  bool identify{true};
};

/// A single write aimed at one unit the scan found.
struct TargetAction {
  uint32_t ip{0};
  std::string action;  ///< scheduler.clear | region.clear | usage.clear | factory.reset | restart | wifi.set
  std::string body;    ///< JSON the target's own endpoint expects, already serialised
};

/// Non-blocking probe of one TCP endpoint, and optionally one HTTP GET/POST on
/// top of it. Owns exactly one socket and never blocks longer than it takes to
/// call `select()` with a zero timeout.
class Probe {
 public:
  enum class Kind : uint8_t { PORT, HTTP };
  enum class State : uint8_t { IDLE, CONNECTING, SENDING, READING, DONE, FAILED };

  Probe() = default;
  ~Probe() { this->close(); }

  // A probe owns one file descriptor, so it must never be copied: the sweep
  // keeps probes in a vector, and a copied fd would be closed twice - once by
  // the moved-from element on reallocation, while the original is still reading.
  Probe(const Probe &) = delete;
  Probe &operator=(const Probe &) = delete;
  Probe(Probe &&other) noexcept { *this = std::move(other); }
  Probe &operator=(Probe &&other) noexcept;

  bool begin_port(uint32_t ip, uint16_t port);
  bool begin_http(uint32_t ip, uint16_t port, const std::string &method, const std::string &path,
                  const std::string &body);
  /// Advances the state machine. Returns true once the probe has settled into
  /// DONE or FAILED. Costs microseconds, so it is safe to call every loop.
  bool poll();
  void close();

  State state() const { return this->state_; }
  bool ok() const { return this->state_ == State::DONE; }
  uint16_t http_status() const { return this->http_status_; }
  const std::string &body() const { return this->body_; }
  uint32_t ip() const { return this->ip_; }
  uint16_t port() const { return this->port_; }

 protected:
  bool start_socket_(uint32_t ip, uint16_t port, uint32_t timeout_ms);
  void fail_();

  int fd_{-1};
  Kind kind_{Kind::PORT};
  State state_{State::IDLE};
  uint32_t ip_{0};
  uint16_t port_{0};
  uint32_t deadline_{0};
  std::string request_;
  size_t sent_{0};
  std::string body_;
  bool headers_done_{false};
  uint16_t http_status_{0};
};

class LanScanComponent : public Component {
 public:
  void setup() override;
  void loop() override;
  void dump_config() override;
  float get_setup_priority() const override { return setup_priority::AFTER_WIFI; }

  /// Starts a sweep. Refuses while a sweep or a delegated OTA is already
  /// running: the relay has one radio and one heap, and a scan that competes
  /// with a firmware push is how both end up failing.
  bool start(const ScanRequest &request, std::string *error);
  void stop();

  bool start_action(const TargetAction &action, std::string *error);

  void build_status(JsonObject root) const;
  void build_networks(JsonObject root) const;
  void build_action(JsonObject root) const;

  JobState state() const { return this->state_; }
  bool busy() const { return this->state_ == JobState::SWEEPING || this->state_ == JobState::IDENTIFYING; }

  /// Subnets this device can actually reach without a router: its station
  /// interface and, when it is up, its own soft AP. Anything else on the combo
  /// box would be a promise the device cannot keep.
  static std::vector<std::pair<uint32_t, uint8_t>> local_subnets();

  static bool parse_network(const std::string &value, uint32_t *network, uint8_t *prefix);
  static std::string ip_to_string(uint32_t ip);
  static bool arp_lookup(uint32_t ip, uint8_t out[6]);

 protected:
  void loop_sweep_();
  void loop_identify_();
  void loop_action_();
  void finish_sweep_();
  void begin_identify_();
  void apply_info_(Neighbour &row, const std::string &json);
  void apply_status_(Neighbour &row, const std::string &json);
  void reset_();

  JobState state_{JobState::IDLE};
  std::string job_id_;
  std::string error_;
  ScanRequest request_{};

  // Sweep bookkeeping.
  uint32_t next_host_{0};   ///< next host octet to enqueue
  uint32_t scanned_{0};     ///< hosts whose probes have all settled
  uint32_t total_{0};
  uint32_t started_ms_{0};
  uint32_t finished_ms_{0};
  std::vector<Probe> in_flight_;
  std::vector<uint32_t> hit_ips_;
  std::vector<Neighbour> results_;
  uint32_t last_host_scanned_{0};

  // Identify bookkeeping.
  size_t identify_index_{0};
  uint8_t identify_step_{0};  ///< 0 = /info, 1 = legacy /info, 2 = /status
  Probe identify_probe_{};
  bool identify_probe_open_{false};

  // Action bookkeeping.
  bool action_running_{false};
  TargetAction action_{};
  Probe action_probe_{};
  std::string action_result_;
  std::string action_error_;
  uint16_t action_status_{0};
};

extern LanScanComponent *global_lan_scan;

}  // namespace lan_scan
}  // namespace esphome

#endif  // USE_ESP32
