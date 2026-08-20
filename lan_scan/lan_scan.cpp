#include "lan_scan.h"

#ifdef USE_ESP32

#include "esphome/core/helpers.h"
#include "esphome/core/log.h"

#ifdef USE_GSMART_OTA_PUSH
#include "esphome/components/ota_push/ota_push.h"
#endif

#include <WiFi.h>
#include <errno.h>
#include <fcntl.h>
#include <lwip/etharp.h>
#include <lwip/netif.h>
#include <lwip/sockets.h>
// LOCK_TCPIP_CORE() expands to sys_mutex_lock(&lock_tcpip_core); neither is
// declared by opt.h, which is all the headers above pull in.
#include <lwip/sys.h>
#include <lwip/tcpip.h>

#include <algorithm>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <utility>

namespace esphome {
namespace lan_scan {

static const char *const TAG = "lan_scan";

LanScanComponent *global_lan_scan = nullptr;  // NOLINT

const char *job_state_to_string(JobState state) {
  switch (state) {
    case JobState::SWEEPING:
      return "sweeping";
    case JobState::IDENTIFYING:
      return "identifying";
    case JobState::DONE:
      return "done";
    case JobState::STOPPED:
      return "stopped";
    case JobState::FAILED:
      return "failed";
    case JobState::IDLE:
    default:
      return "idle";
  }
}

namespace {

std::string mac_to_string(const uint8_t mac[6]) {
  char buf[18];
  snprintf(buf, sizeof(buf), "%02x:%02x:%02x:%02x:%02x:%02x", mac[0], mac[1], mac[2], mac[3], mac[4], mac[5]);
  return std::string(buf);
}

/// Serial is the last three bytes of the MAC, the same rule the device applies
/// to itself. That is what makes an ARP answer alone enough to name a unit
/// whose HTTP API is dead.
std::string serial_from_mac(const uint8_t mac[6]) {
  char buf[7];
  snprintf(buf, sizeof(buf), "%02x%02x%02x", mac[3], mac[4], mac[5]);
  return std::string(buf);
}

std::string json_string(JsonVariantConst value) {
  if (value.isNull())
    return {};
  if (value.is<const char *>()) {
    const char *raw = value.as<const char *>();
    return raw == nullptr ? std::string{} : std::string(raw);
  }
  if (value.is<std::string>())
    return value.as<std::string>();
  return {};
}

}  // namespace

// ---------------------------------------------------------------------------
// Probe
// ---------------------------------------------------------------------------

Probe &Probe::operator=(Probe &&other) noexcept {
  if (this != &other) {
    this->close();
    this->fd_ = other.fd_;
    other.fd_ = -1;
    this->kind_ = other.kind_;
    this->state_ = other.state_;
    this->ip_ = other.ip_;
    this->port_ = other.port_;
    this->deadline_ = other.deadline_;
    this->request_ = std::move(other.request_);
    this->sent_ = other.sent_;
    this->body_ = std::move(other.body_);
    this->headers_done_ = other.headers_done_;
    this->http_status_ = other.http_status_;
    other.state_ = State::IDLE;
  }
  return *this;
}

bool Probe::start_socket_(uint32_t ip, uint16_t port, uint32_t timeout_ms) {
  this->close();
  this->ip_ = ip;
  this->port_ = port;
  this->body_.clear();
  this->headers_done_ = false;
  this->http_status_ = 0;
  this->sent_ = 0;

  this->fd_ = ::socket(AF_INET, SOCK_STREAM, IPPROTO_TCP);
  if (this->fd_ < 0) {
    this->state_ = State::FAILED;
    return false;
  }

  const int flags = ::fcntl(this->fd_, F_GETFL, 0);
  ::fcntl(this->fd_, F_SETFL, flags | O_NONBLOCK);

  struct sockaddr_in addr {};
  addr.sin_family = AF_INET;
  addr.sin_port = htons(port);
  addr.sin_addr.s_addr = htonl(ip);

  const int rc = ::connect(this->fd_, reinterpret_cast<struct sockaddr *>(&addr), sizeof(addr));
  if (rc < 0 && errno != EINPROGRESS) {
    this->fail_();
    return false;
  }

  this->deadline_ = millis() + timeout_ms;
  this->state_ = State::CONNECTING;
  return true;
}

bool Probe::begin_port(uint32_t ip, uint16_t port) {
  this->kind_ = Kind::PORT;
  this->request_.clear();
  return this->start_socket_(ip, port, kConnectTimeoutMs);
}

bool Probe::begin_http(uint32_t ip, uint16_t port, const std::string &method, const std::string &path,
                       const std::string &body) {
  this->kind_ = Kind::HTTP;

  // HTTP/1.0 on purpose: it means "answer and close", so the read side ends on
  // EOF and nothing here has to understand chunked encoding or keep-alive.
  std::string request = method + " " + path + " HTTP/1.0\r\n";
  request += "Host: " + LanScanComponent::ip_to_string(ip) + "\r\n";
  request += "User-Agent: gsmart-lan-scan\r\n";
  request += "Accept: application/json\r\n";
  if (!body.empty()) {
    request += "Content-Type: application/json\r\n";
    request += "Content-Length: " + to_string(body.size()) + "\r\n";
  }
  request += "Connection: close\r\n\r\n";
  request += body;
  this->request_ = request;

  return this->start_socket_(ip, port, kHttpTimeoutMs);
}

void Probe::fail_() {
  this->close();
  this->state_ = State::FAILED;
}

void Probe::close() {
  if (this->fd_ >= 0) {
    ::close(this->fd_);
    this->fd_ = -1;
  }
}

bool Probe::poll() {
  if (this->state_ == State::DONE || this->state_ == State::FAILED || this->state_ == State::IDLE)
    return true;

  if (static_cast<int32_t>(millis() - this->deadline_) >= 0) {
    // A read that already has a body is a usable answer even if the peer never
    // closed: better a truncated `/status` than nothing at all.
    if (this->state_ == State::READING && this->headers_done_) {
      this->close();
      this->state_ = State::DONE;
    } else {
      this->fail_();
    }
    return true;
  }

  fd_set read_fds;
  fd_set write_fds;
  FD_ZERO(&read_fds);
  FD_ZERO(&write_fds);
  const bool waiting_to_write = this->state_ == State::CONNECTING || this->state_ == State::SENDING;
  if (waiting_to_write) {
    FD_SET(this->fd_, &write_fds);
  } else {
    FD_SET(this->fd_, &read_fds);
  }

  struct timeval tv {};
  tv.tv_sec = 0;
  tv.tv_usec = 0;
  const int rc = ::select(this->fd_ + 1, &read_fds, &write_fds, nullptr, &tv);
  if (rc < 0) {
    this->fail_();
    return true;
  }
  if (rc == 0)
    return false;

  if (this->state_ == State::CONNECTING) {
    int sock_error = 0;
    socklen_t len = sizeof(sock_error);
    if (::getsockopt(this->fd_, SOL_SOCKET, SO_ERROR, &sock_error, &len) < 0 || sock_error != 0) {
      this->fail_();
      return true;
    }
    if (this->kind_ == Kind::PORT) {
      // The handshake completing is the whole answer. Hang up before the target
      // starts an OTA session it will then have to time out of.
      this->close();
      this->state_ = State::DONE;
      return true;
    }
    this->state_ = State::SENDING;
    return false;
  }

  if (this->state_ == State::SENDING) {
    const size_t remaining = this->request_.size() - this->sent_;
    const int written = ::send(this->fd_, this->request_.data() + this->sent_, remaining, 0);
    if (written < 0) {
      if (errno == EAGAIN || errno == EWOULDBLOCK)
        return false;
      this->fail_();
      return true;
    }
    this->sent_ += static_cast<size_t>(written);
    if (this->sent_ >= this->request_.size()) {
      this->request_.clear();
      this->request_.shrink_to_fit();
      this->state_ = State::READING;
    }
    return false;
  }

  // READING
  char buffer[512];
  const int received = ::recv(this->fd_, buffer, sizeof(buffer), 0);
  if (received < 0) {
    if (errno == EAGAIN || errno == EWOULDBLOCK)
      return false;
    this->fail_();
    return true;
  }
  if (received == 0) {
    this->close();
    this->state_ = this->headers_done_ ? State::DONE : State::FAILED;
    return true;
  }

  this->body_.append(buffer, static_cast<size_t>(received));

  if (!this->headers_done_) {
    const size_t split = this->body_.find("\r\n\r\n");
    if (split != std::string::npos) {
      const std::string head = this->body_.substr(0, split);
      const size_t status_start = head.find(' ');
      if (status_start != std::string::npos)
        this->http_status_ = static_cast<uint16_t>(atoi(head.c_str() + status_start + 1));
      this->body_.erase(0, split + 4);
      this->headers_done_ = true;
    } else if (this->body_.size() > kMaxBodyBytes) {
      this->fail_();
      return true;
    }
  }

  if (this->headers_done_ && this->body_.size() > kMaxBodyBytes) {
    // Whatever this is, it is not one of our payloads. Take what fits and stop
    // rather than letting a chatty peer eat the heap.
    this->body_.resize(kMaxBodyBytes);
    this->close();
    this->state_ = State::DONE;
    return true;
  }

  return false;
}

// ---------------------------------------------------------------------------
// Helpers
// ---------------------------------------------------------------------------

std::string LanScanComponent::ip_to_string(uint32_t ip) {
  char buf[16];
  snprintf(buf, sizeof(buf), "%u.%u.%u.%u", static_cast<unsigned>((ip >> 24) & 0xFF),
           static_cast<unsigned>((ip >> 16) & 0xFF), static_cast<unsigned>((ip >> 8) & 0xFF),
           static_cast<unsigned>(ip & 0xFF));
  return std::string(buf);
}

bool LanScanComponent::parse_network(const std::string &value, uint32_t *network, uint8_t *prefix) {
  if (value.empty())
    return false;

  std::string address = value;
  uint8_t bits = 24;

  const size_t slash = address.find('/');
  if (slash != std::string::npos) {
    bits = static_cast<uint8_t>(atoi(address.c_str() + slash + 1));
    address = address.substr(0, slash);
  }
  if (bits < 16 || bits > 30)
    return false;

  unsigned a = 0;
  unsigned b = 0;
  unsigned c = 0;
  unsigned d = 0;
  const int parsed = sscanf(address.c_str(), "%u.%u.%u.%u", &a, &b, &c, &d);
  // "192.168.1" is what a person types for a class C, so treat a missing fourth
  // octet as zero rather than as a typo.
  if (parsed < 3 || a > 255 || b > 255 || c > 255 || d > 255)
    return false;

  const uint32_t raw = (a << 24) | (b << 16) | (c << 8) | d;
  const uint32_t mask = bits == 0 ? 0 : (0xFFFFFFFFu << (32 - bits));
  *network = raw & mask;
  *prefix = bits;
  return true;
}

bool LanScanComponent::arp_lookup(uint32_t ip, uint8_t out[6]) {
  ip4_addr_t addr;
  ip4_addr_set_u32(&addr, htonl(ip));

  struct eth_addr *eth = nullptr;
  const ip4_addr_t *found = nullptr;
  bool ok = false;

#if LWIP_TCPIP_CORE_LOCKING
  LOCK_TCPIP_CORE();
#endif
  for (struct netif *nif = netif_list; nif != nullptr; nif = nif->next) {
    if (etharp_find_addr(nif, &addr, &eth, &found) >= 0 && eth != nullptr) {
      memcpy(out, eth->addr, 6);
      ok = true;
      break;
    }
  }
#if LWIP_TCPIP_CORE_LOCKING
  UNLOCK_TCPIP_CORE();
#endif

  return ok;
}

std::vector<std::pair<uint32_t, uint8_t>> LanScanComponent::local_subnets() {
  std::vector<std::pair<uint32_t, uint8_t>> out;

  auto prefix_from_mask = [](uint32_t mask) -> uint8_t {
    uint8_t bits = 0;
    while (bits < 32 && (mask & (0x80000000u >> bits)) != 0)
      bits++;
    return bits;
  };

  if (WiFi.isConnected()) {
    const uint32_t ip = static_cast<uint32_t>(WiFi.localIP());
    const uint32_t mask = static_cast<uint32_t>(WiFi.subnetMask());
    if (ip != 0) {
      // Arduino's IPAddress is little endian on the wire side; bring both into
      // host order before masking or the network comes out reversed.
      const uint32_t ip_host = ntohl(ip);
      const uint32_t mask_host = ntohl(mask);
      out.emplace_back(ip_host & mask_host, prefix_from_mask(mask_host));
    }
  }

  const uint32_t ap_ip = static_cast<uint32_t>(WiFi.softAPIP());
  if (ap_ip != 0) {
    const uint32_t ap_host = ntohl(ap_ip);
    const uint32_t network = ap_host & 0xFFFFFF00u;
    const bool duplicate = std::any_of(out.begin(), out.end(),
                                       [&](const std::pair<uint32_t, uint8_t> &item) { return item.first == network; });
    if (!duplicate)
      out.emplace_back(network, 24);
  }

  return out;
}

// ---------------------------------------------------------------------------
// Component
// ---------------------------------------------------------------------------

void LanScanComponent::setup() { global_lan_scan = this; }

void LanScanComponent::dump_config() {
  ESP_LOGCONFIG(TAG, "LAN scan:");
  ESP_LOGCONFIG(TAG, "  Page size: %u (max %u)", kDefaultLimit, kMaxResults);
  ESP_LOGCONFIG(TAG, "  Probes in flight: %u", kMaxInFlight);
  for (const auto &subnet : local_subnets())
    ESP_LOGCONFIG(TAG, "  Reachable: %s/%u", ip_to_string(subnet.first).c_str(), subnet.second);
}

void LanScanComponent::reset_() {
  for (auto &probe : this->in_flight_)
    probe.close();
  this->in_flight_.clear();
  this->identify_probe_.close();
  this->identify_probe_open_ = false;
  this->hit_ips_.clear();
  this->results_.clear();
  this->identify_index_ = 0;
  this->identify_step_ = 0;
  this->scanned_ = 0;
  this->last_host_scanned_ = 0;
  this->error_.clear();
}

bool LanScanComponent::start(const ScanRequest &request, std::string *error) {
  if (this->busy()) {
    if (error != nullptr)
      *error = "scan_already_running";
    return false;
  }

#ifdef USE_GSMART_OTA_PUSH
  // A delegated OTA already holds a TLS download and a push socket open. A sweep
  // on top of that competes for the same heap, and both fail instead of one
  // succeeding.
  if (ota_push::global_ota_push != nullptr && ota_push::global_ota_push->busy()) {
    if (error != nullptr)
      *error = "ota_in_progress";
    return false;
  }
#endif

  if (!WiFi.isConnected()) {
    if (error != nullptr)
      *error = "not_connected";
    return false;
  }

  ScanRequest job = request;
  if (job.limit == 0 || job.limit > kMaxResults)
    job.limit = kDefaultLimit;
  if (job.from_host < 1)
    job.from_host = 1;
  if (job.to_host < job.from_host || job.to_host > 254)
    job.to_host = 254;

  const uint32_t host_count = (1u << (32 - job.prefix)) - 2;
  if (job.to_host > host_count)
    job.to_host = static_cast<uint8_t>(std::min<uint32_t>(host_count, 254));

  // The sweep walks the last octet, so it is always one class C: the /24 that
  // holds whatever network was asked for. A wider prefix is still reported as
  // given, but nobody pages through 65k addresses one lamp at a time.
  job.network &= 0xFFFFFF00u;

  this->reset_();
  this->in_flight_.reserve(kMaxInFlight + 2);
  this->request_ = job;
  this->next_host_ = job.from_host;
  this->total_ = job.to_host >= job.from_host ? (job.to_host - job.from_host + 1) : 0;
  this->started_ms_ = millis();
  this->finished_ms_ = 0;
  this->state_ = JobState::SWEEPING;

  char buf[24];
  snprintf(buf, sizeof(buf), "scan-%u", static_cast<unsigned>(this->started_ms_));
  this->job_id_ = buf;

  ESP_LOGI(TAG, "Sweep %s/%u hosts %u-%u, page %u", ip_to_string(job.network).c_str(), job.prefix, job.from_host,
           job.to_host, job.limit);
  return true;
}

void LanScanComponent::stop() {
  if (!this->busy())
    return;
  for (auto &probe : this->in_flight_)
    probe.close();
  this->in_flight_.clear();
  this->identify_probe_.close();
  this->identify_probe_open_ = false;
  this->state_ = JobState::STOPPED;
  this->finished_ms_ = millis();
}

void LanScanComponent::loop() {
  switch (this->state_) {
    case JobState::SWEEPING:
      this->loop_sweep_();
      break;
    case JobState::IDENTIFYING:
      this->loop_identify_();
      break;
    default:
      break;
  }
  if (this->action_running_)
    this->loop_action_();
}

void LanScanComponent::loop_sweep_() {
  // Harvest whatever settled since the last pass.
  for (size_t i = 0; i < this->in_flight_.size();) {
    Probe &probe = this->in_flight_[i];
    if (!probe.poll()) {
      i++;
      continue;
    }

    const uint32_t ip = probe.ip();
    const uint16_t port = probe.port();
    const bool hit = probe.ok();
    probe.close();

    if (hit) {
      const bool already =
          std::any_of(this->hit_ips_.begin(), this->hit_ips_.end(), [&](uint32_t seen) { return seen == ip; });
      if (!already) {
        this->hit_ips_.push_back(ip);
        Neighbour row;
        row.ip = ip;
        row.ota_port = port;
        // The ARP entry is at its freshest right after the handshake, and for a
        // unit whose API never answers it is the only identity we will get.
        if (arp_lookup(ip, row.mac)) {
          row.mac_known = true;
          row.serial = serial_from_mac(row.mac);
        }
        this->results_.push_back(row);
        ESP_LOGD(TAG, "Found %s on port %u", ip_to_string(ip).c_str(), port);
      }
    }

    const uint32_t host = ip & 0xFF;
    if (host > this->last_host_scanned_)
      this->last_host_scanned_ = host;

    this->in_flight_.erase(this->in_flight_.begin() + static_cast<long>(i));
  }

  // Stop handing out new work once the page is full, but let what is already in
  // flight finish: a lower IP may still be mid-handshake, and dropping it would
  // break the "first N from the bottom" promise the caller is paging on.
  const bool page_full = this->results_.size() >= this->request_.limit;

  // A host needs room for *both* its ports before it is started. Squeezing in
  // only the 3232 probe because the window was one slot short would drop every
  // rex at that address - silently, and only on a busy segment.
  while (!page_full && this->in_flight_.size() + 2 <= kMaxInFlight &&
         this->next_host_ <= this->request_.to_host) {
    const uint32_t ip = this->request_.network | this->next_host_;
    this->next_host_++;
    this->scanned_++;

    // Both OTA ports at once. Sequential would double the sweep on a segment
    // where most addresses are simply empty.
    for (const uint16_t port : {kOtaPortEsp32, kOtaPortEsp8266}) {
      this->in_flight_.emplace_back();
      if (!this->in_flight_.back().begin_port(ip, port))
        this->in_flight_.pop_back();
    }
  }

  const bool exhausted = this->next_host_ > this->request_.to_host;
  if (this->in_flight_.empty() && (page_full || exhausted))
    this->finish_sweep_();
}

void LanScanComponent::finish_sweep_() {
  std::sort(this->results_.begin(), this->results_.end(),
            [](const Neighbour &a, const Neighbour &b) { return a.ip < b.ip; });
  if (this->results_.size() > this->request_.limit)
    this->results_.resize(this->request_.limit);

  ESP_LOGI(TAG, "Sweep done: %u found in %u ms", static_cast<unsigned>(this->results_.size()),
           static_cast<unsigned>(millis() - this->started_ms_));

  if (this->request_.identify && !this->results_.empty()) {
    this->begin_identify_();
    return;
  }

  this->state_ = JobState::DONE;
  this->finished_ms_ = millis();
}

void LanScanComponent::begin_identify_() {
  this->identify_index_ = 0;
  this->identify_step_ = 0;
  this->identify_probe_open_ = false;
  this->state_ = JobState::IDENTIFYING;
}

void LanScanComponent::loop_identify_() {
  if (this->identify_index_ >= this->results_.size()) {
    this->state_ = JobState::DONE;
    this->finished_ms_ = millis();
    ESP_LOGI(TAG, "Identify done for %u units", static_cast<unsigned>(this->results_.size()));
    return;
  }

  Neighbour &row = this->results_[this->identify_index_];

  if (!this->identify_probe_open_) {
    // Step 0 asks the current API, step 1 the legacy alias, step 2 the status
    // that carries region and scheduler. A unit that answers neither info path
    // is left as it came out of the sweep - an ESP that will not talk.
    std::string path;
    switch (this->identify_step_) {
      case 0:
        path = "/api/g-node/v1/info";
        break;
      case 1:
        path = "/api/mobile/v1/info";
        break;
      case 2:
        path = row.api_base + "/status";
        break;
      default:
        this->identify_index_++;
        this->identify_step_ = 0;
        return;
    }

    this->identify_probe_ = Probe{};
    if (!this->identify_probe_.begin_http(row.ip, kHttpPort, "GET", path, "")) {
      this->identify_step_ = this->identify_step_ == 0 ? 1 : 3;
      return;
    }
    this->identify_probe_open_ = true;
    return;
  }

  if (!this->identify_probe_.poll())
    return;

  const bool ok = this->identify_probe_.ok() && this->identify_probe_.http_status() == 200;
  const std::string payload = ok ? this->identify_probe_.body() : std::string{};
  this->identify_probe_.close();
  this->identify_probe_open_ = false;

  switch (this->identify_step_) {
    case 0:
      if (ok) {
        row.api_base = "/api/g-node/v1";
        this->apply_info_(row, payload);
        this->identify_step_ = 2;
      } else {
        this->identify_step_ = 1;
      }
      break;
    case 1:
      if (ok) {
        row.api_base = "/api/mobile/v1";
        this->apply_info_(row, payload);
        this->identify_step_ = 2;
      } else {
        // No API at all. Keep the row: MAC and an open OTA port are still enough
        // to push firmware at it, which is the whole point of finding it.
        this->identify_index_++;
        this->identify_step_ = 0;
      }
      break;
    case 2:
    default:
      if (ok)
        this->apply_status_(row, payload);
      this->identify_index_++;
      this->identify_step_ = 0;
      break;
  }
}

void LanScanComponent::apply_info_(Neighbour &row, const std::string &payload) {
  json::parse_json(payload, [&row](JsonObject root) -> bool {
    row.api_ok = true;
    row.model = json_string(root["model"]);
    row.model_num = root["modelNum"].is<int>() ? static_cast<uint8_t>(root["modelNum"].as<int>()) : 0;
    row.name = json_string(root["name"]);

    const std::string serial = json_string(root["serial"]);
    if (!serial.empty())
      row.serial = serial;

    const std::string mac = json_string(root["mac"]);
    if (!mac.empty() && !row.mac_known) {
      unsigned bytes[6] = {0, 0, 0, 0, 0, 0};
      if (sscanf(mac.c_str(), "%x:%x:%x:%x:%x:%x", &bytes[0], &bytes[1], &bytes[2], &bytes[3], &bytes[4], &bytes[5]) ==
          6) {
        for (int i = 0; i < 6; i++)
          row.mac[i] = static_cast<uint8_t>(bytes[i]);
        row.mac_known = true;
      }
    }

    JsonObjectConst firmware = root["firmware"].as<JsonObjectConst>();
    if (!firmware.isNull())
      row.firmware = json_string(firmware["version"]);
    if (row.firmware.empty())
      row.firmware = json_string(root["fwVersion"]);

    JsonObjectConst capabilities = root["capabilities"].as<JsonObjectConst>();
    if (!capabilities.isNull())
      row.is_emitter = capabilities["emitter"].as<bool>();
    return true;
  });
}

void LanScanComponent::apply_status_(Neighbour &row, const std::string &payload) {
  json::parse_json(payload, [&row](JsonObject root) -> bool {
    row.mode = json_string(root["mode"]);
    row.radiate = root["radiate"].as<bool>();

    JsonObjectConst region = root["region"].as<JsonObjectConst>();
    if (!region.isNull()) {
      row.region_id = json_string(region["regionId"]);
      row.region_active = region["active"].as<bool>();
      row.region_master = region["isMaster"].as<bool>();
    }

    JsonObjectConst situation = root["situation"].as<JsonObjectConst>();
    if (!situation.isNull()) {
      row.scheduler_known = true;
      row.scheduler_active = situation["schedulerActive"].as<bool>();
      row.scheduler_items = static_cast<uint16_t>(situation["schedulerItemsCount"].as<int>());
    }
    return true;
  });
}

// ---------------------------------------------------------------------------
// Target actions
// ---------------------------------------------------------------------------

bool LanScanComponent::start_action(const TargetAction &action, std::string *error) {
  if (this->action_running_) {
    if (error != nullptr)
      *error = "action_already_running";
    return false;
  }
  if (action.ip == 0) {
    if (error != nullptr)
      *error = "missing_target_ip";
    return false;
  }

  std::string path;
  std::string body = action.body;
  if (action.action == "scheduler.clear") {
    // Clearing a plan is writing an empty one. The target has no "forget the
    // calendar" endpoint of its own, and an empty frame list is what its own
    // scheduler treats as nothing scheduled.
    path = "/scheduler";
    if (body.empty())
      body = "{\"mode\":\"off\",\"enabled\":false,\"frames\":[]}";
  } else if (action.action == "scheduler.state") {
    path = "/scheduler/state";
  } else if (action.action == "region.clear") {
    path = "/control/clear-region";
    if (body.empty())
      body = "{\"confirm\":\"CLEAR_REGION\",\"schedule\":false}";
  } else if (action.action == "usage.clear") {
    path = "/control/clear-usage";
    if (body.empty())
      body = "{\"confirm\":\"CLEAR_USAGE\"}";
  } else if (action.action == "factory.reset") {
    path = "/control/factory-reset";
    if (body.empty())
      body = "{\"confirm\":\"FACTORY_RESET\"}";
  } else if (action.action == "restart") {
    path = "/control/restart";
    if (body.empty())
      body = "{\"source\":\"lan-scan\"}";
  } else if (action.action == "wifi.set") {
    if (body.empty()) {
      if (error != nullptr)
        *error = "missing_network_body";
      return false;
    }
    path = "/network";
  } else if (action.action == "identify") {
    path = "/control/identify";
    if (body.empty())
      body = "{\"durationSec\":5}";
  } else {
    if (error != nullptr)
      *error = "unsupported_action";
    return false;
  }

  // Same alias dance as the sweep: a unit old enough to be worth resetting may
  // only know the legacy path.
  const std::string base = "/api/g-node/v1";
  this->action_ = action;
  this->action_.body = body;
  this->action_result_.clear();
  this->action_error_.clear();
  this->action_status_ = 0;
  this->action_probe_ = Probe{};

  if (!this->action_probe_.begin_http(action.ip, kHttpPort, "POST", base + path, body)) {
    if (error != nullptr)
      *error = "connect_failed";
    return false;
  }

  this->action_running_ = true;
  ESP_LOGI(TAG, "Action %s -> %s", action.action.c_str(), ip_to_string(action.ip).c_str());
  return true;
}

void LanScanComponent::loop_action_() {
  if (!this->action_probe_.poll())
    return;

  this->action_status_ = this->action_probe_.http_status();
  if (this->action_probe_.ok() && this->action_status_ >= 200 && this->action_status_ < 300) {
    this->action_result_ = this->action_probe_.body();
  } else {
    this->action_error_ = this->action_probe_.ok() ? "http_" + to_string(this->action_status_) : "unreachable";
  }
  this->action_probe_.close();
  this->action_running_ = false;

  ESP_LOGI(TAG, "Action %s -> %s: %s", this->action_.action.c_str(), ip_to_string(this->action_.ip).c_str(),
           this->action_error_.empty() ? "ok" : this->action_error_.c_str());
}

// ---------------------------------------------------------------------------
// JSON
// ---------------------------------------------------------------------------

void LanScanComponent::build_status(JsonObject root) const {
  root["jobId"] = this->job_id_;
  root["state"] = job_state_to_string(this->state_);
  root["running"] = this->busy();
  root["network"] = ip_to_string(this->request_.network);
  root["prefix"] = this->request_.prefix;
  root["fromHost"] = this->request_.from_host;
  root["toHost"] = this->request_.to_host;
  root["limit"] = this->request_.limit;
  root["scanned"] = this->scanned_;
  root["total"] = this->total_;
  root["found"] = static_cast<uint32_t>(this->results_.size());
  root["elapsedMs"] = this->started_ms_ == 0
                          ? 0
                          : (this->finished_ms_ != 0 ? this->finished_ms_ - this->started_ms_
                                                     : millis() - this->started_ms_);
  if (!this->error_.empty())
    root["error"] = this->error_;

  // Where the next page starts. Full page means "there may be more above the
  // last hit"; a sweep that ran out of segment says so with null.
  const bool page_full = this->results_.size() >= this->request_.limit;
  if (page_full && !this->results_.empty()) {
    const uint32_t last_host = this->results_.back().ip & 0xFF;
    if (last_host < this->request_.to_host) {
      root["nextFrom"] = last_host + 1;
    } else {
      root["nextFrom"] = nullptr;
    }
  } else {
    root["nextFrom"] = nullptr;
  }

  JsonArray devices = root["devices"].to<JsonArray>();
  for (const auto &row : this->results_) {
    JsonObject item = devices.add<JsonObject>();
    item["ip"] = ip_to_string(row.ip);
    item["otaPort"] = row.ota_port;
    item["mac"] = row.mac_known ? mac_to_string(row.mac) : std::string{};
    item["serial"] = row.serial;
    item["apiOk"] = row.api_ok;
    item["apiBase"] = row.api_base;
    item["model"] = row.model;
    item["modelNum"] = row.model_num;
    item["name"] = row.name;
    item["firmware"] = row.firmware;
    item["emitter"] = row.is_emitter;
    item["regionId"] = row.region_id;
    item["regionActive"] = row.region_active;
    item["regionMaster"] = row.region_master;
    item["schedulerKnown"] = row.scheduler_known;
    item["schedulerActive"] = row.scheduler_active;
    item["schedulerItems"] = row.scheduler_items;
    item["mode"] = row.mode;
    item["radiate"] = row.radiate;
  }
}

void LanScanComponent::build_networks(JsonObject root) const {
  JsonArray networks = root["networks"].to<JsonArray>();

  const auto subnets = local_subnets();
  bool first = true;
  for (const auto &subnet : subnets) {
    JsonObject item = networks.add<JsonObject>();
    item["network"] = ip_to_string(subnet.first);
    item["prefix"] = subnet.second;
    item["cidr"] = ip_to_string(subnet.first) + "/" + to_string(subnet.second);
    item["source"] = first ? "sta" : "softap";
    item["default"] = first;
    first = false;
  }

  root["connected"] = WiFi.isConnected();
  root["ip"] = WiFi.isConnected() ? std::string(WiFi.localIP().toString().c_str()) : std::string{};
  root["ssid"] = WiFi.isConnected() ? std::string(WiFi.SSID().c_str()) : std::string{};
}

void LanScanComponent::build_action(JsonObject root) const {
  root["running"] = this->action_running_;
  root["action"] = this->action_.action;
  root["ip"] = this->action_.ip == 0 ? std::string{} : ip_to_string(this->action_.ip);
  root["status"] = this->action_status_;
  root["ok"] = !this->action_running_ && this->action_error_.empty() && this->action_status_ != 0;
  if (!this->action_error_.empty())
    root["error"] = this->action_error_;
  if (!this->action_result_.empty())
    root["response"] = this->action_result_;
}

}  // namespace lan_scan
}  // namespace esphome

#endif  // USE_ESP32
