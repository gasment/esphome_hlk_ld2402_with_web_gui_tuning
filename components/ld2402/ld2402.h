#pragma once

#include "esphome/core/component.h"
#include "esphome/components/uart/uart.h"
#include "esphome/components/binary_sensor/binary_sensor.h"
#include "esphome/components/sensor/sensor.h"
#include "esphome/components/text_sensor/text_sensor.h"
#include "esphome/components/network/util.h"

#include <esp_http_server.h>
#include <vector>
#include <deque>
#include <memory>
#include <functional>
#include <string>
#include <cmath>

namespace esphome {
namespace ld2402 {

// ─── 协议常量 ───────────────────────────────────────────────────
static const uint8_t CMD_HEADER[4]  = {0xFD, 0xFC, 0xFB, 0xFA};
static const uint8_t CMD_FOOTER[4]  = {0x04, 0x03, 0x02, 0x01};
static const uint8_t DATA_HEADER[4] = {0xF4, 0xF3, 0xF2, 0xF1};
static const uint8_t DATA_FOOTER[4] = {0xF8, 0xF7, 0xF6, 0xF5};

static const uint16_t CMD_READ_FW        = 0x0000;
static const uint16_t CMD_ENABLE_CONFIG  = 0x00FF;
static const uint16_t CMD_END_CONFIG     = 0x00FE;   // 退出配置
static const uint16_t CMD_SAVE_PARAMS    = 0x00FD;   // 保存参数
static const uint16_t CMD_READ_SN_STR   = 0x0011;
static const uint16_t CMD_READ_PARAMS    = 0x0008;
static const uint16_t CMD_WRITE_PARAMS   = 0x0007;
static const uint16_t CMD_SET_MODE       = 0x0012;
static const uint16_t CMD_AUTO_THRESHOLD = 0x0009;
static const uint16_t CMD_AUTO_PROGRESS  = 0x000A;
static const uint16_t CMD_AUTO_GAIN      = 0x00EE;

static const uint32_t MODE_NORMAL   = 0x00000064;
static const uint32_t MODE_ENGINEER = 0x00000004;

static const uint8_t NUM_GATES = 16;

// ─── 命令队列项 ─────────────────────────────────────────────────
struct CmdQueueItem {
    uint16_t cmd_word{0};
    std::vector<uint8_t> payload;
    std::function<void(const std::vector<uint8_t> &)> callback;
    std::function<void()> on_timeout;
    bool    is_config_cmd{false};
    uint8_t retry_count{0};
    uint32_t timeout_ms{1000};
};

// ─── SSE 客户端 ─────────────────────────────────────────────────
struct SseClient {
    httpd_handle_t handle;
    int fd;
};

// ─── 主组件类 ───────────────────────────────────────────────────
class LD2402Component : public Component, public uart::UARTDevice {
public:
    void setup() override;
    void loop() override;
    float get_setup_priority() const override { return setup_priority::LATE; }

    // 配置接口
    void set_web_port(uint16_t port)                               { web_port_ = port; }
    void set_web_credentials(const std::string &u, const std::string &p) {
        web_user_ = u; web_pass_ = p;
    }
    void set_presence_sensor(binary_sensor::BinarySensor *s)      { presence_sensor_ = s; }
    void set_distance_sensor(sensor::Sensor *s)                    { distance_sensor_ = s; }
    void set_firmware_sensor(text_sensor::TextSensor *s)           { firmware_sensor_ = s; }
    void set_work_mode_sensor(text_sensor::TextSensor *s)          { work_mode_sensor_ = s; }

    // 公开命令接口
    void cmd_read_firmware(std::function<void(const std::string&)> cb);
    void cmd_read_sn(std::function<void(const std::string&)> cb);
    void cmd_set_engineer_mode(bool enable, std::function<void(bool)> cb);
    void cmd_read_gate_thresholds(bool micro,
                                  std::function<void(const std::vector<uint32_t>&)> cb);
    void cmd_write_gate_threshold(bool micro, uint8_t gate, uint32_t raw_val,
                                  std::function<void(bool)> cb);
    void cmd_write_params_batch(const std::vector<std::pair<uint16_t,uint32_t>> &params,
                                std::function<void(bool)> cb);
    void cmd_save_params(std::function<void(bool)> cb);
    void cmd_auto_threshold(uint16_t trig, uint16_t hold, uint16_t micro_coeff,
                            std::function<void(bool)> cb);
    void cmd_auto_progress(std::function<void(int)> cb);
    void cmd_auto_gain(std::function<void(bool)> cb);
    void cmd_read_param(uint16_t id, std::function<void(uint32_t)> cb);
    void enqueue_cmd_(CmdQueueItem item);

    // 数据访问
    const uint32_t* motion_energy()  const { return motion_energy_; }
    const uint32_t* micro_energy()   const { return micro_energy_; }
    uint8_t  detection_result()      const { return detection_result_; }
    uint16_t target_distance_cm()    const { return target_distance_; }
    bool     is_engineer_mode()      const { return engineer_mode_; }
    uint32_t get_max_distance_gates() const { return max_distance_gates_; }
    uint32_t get_absence_timeout()    const { return absence_timeout_; }

    // SSE 客户端管理
    void add_sse_client(httpd_handle_t h, int fd);
    void remove_sse_client(int fd);

private:
    // UART
    void send_raw_(const std::vector<uint8_t> &frame);
    void process_rx_();

    enum class ParseState {
        IDLE,
        CMD_HEADER, CMD_LENGTH, CMD_DATA, CMD_FOOTER,
        DAT_HEADER, DAT_LENGTH, DAT_DATA, DAT_FOOTER
    };

    void parse_byte_(uint8_t b);
    void dispatch_cmd_frame_(const std::vector<uint8_t> &data);
    void dispatch_data_frame_(const std::vector<uint8_t> &data);
    void parse_normal_line_(const std::string &line);

    // 命令队列
    void pump_cmd_queue_();
    void send_cmd_frame_(const CmdQueueItem &item);
    void on_ack_(const std::vector<uint8_t> &data);

    // Web 服务器
    void start_web_server_();
    void push_sse_energy_();
    bool check_basic_auth_(httpd_req_t *req);

    static esp_err_t handle_root_(httpd_req_t *req);
    static esp_err_t handle_api_info_(httpd_req_t *req);
    static esp_err_t handle_api_cmd_(httpd_req_t *req);
    static esp_err_t handle_sse_(httpd_req_t *req);

    // ── 成员变量 ─────────────────────────────────────────────────
    uint16_t web_port_{8080};
    std::string web_user_{"admin"};
    std::string web_pass_{"admin"};

    binary_sensor::BinarySensor *presence_sensor_{nullptr};
    sensor::Sensor               *distance_sensor_{nullptr};
    text_sensor::TextSensor       *firmware_sensor_{nullptr};
    text_sensor::TextSensor       *work_mode_sensor_{nullptr};

    ParseState parse_state_{ParseState::IDLE};
    uint8_t  hdr_idx_{0};
    uint16_t frame_len_{0};
    uint16_t frame_recv_{0};
    std::vector<uint8_t> frame_buf_;
    std::string line_buf_;

    bool     engineer_mode_{false};
    uint8_t  detection_result_{0};
    uint16_t target_distance_{0};
    uint32_t motion_energy_[NUM_GATES]{};
    uint32_t micro_energy_[NUM_GATES]{};

    // 命令队列 — 使用 deque 以支持 pop_front
    std::deque<CmdQueueItem>          cmd_queue_;        // deque，支持 pop_front
    std::unique_ptr<CmdQueueItem>     cmd_in_flight_;    // unique_ptr，非 bool
    uint32_t cmd_sent_ms_{0};

    httpd_handle_t httpd_{nullptr};
    std::vector<SseClient> sse_clients_;
    uint32_t last_sse_ms_{0};
    bool web_started_{false};

    std::string firmware_ver_{"unknown"};
    std::string sn_str_{"unknown"};
    uint32_t max_distance_gates_{12};   // 参数 0x0001，默认 12 门（8.4m）
    uint32_t absence_timeout_{30};      // 参数 0x0004，默认 30 秒
    uint32_t motion_thresholds_[NUM_GATES]{};
    uint32_t micro_thresholds_[NUM_GATES]{};
};

// ── 工具函数 ────────────────────────────────────────────────────
inline double raw_to_db(uint32_t raw) {
    if (raw == 0) return 0.0;
    return 10.0 * std::log10((double)raw);
}
inline uint32_t db_to_raw(double db) {
    return (uint32_t)std::round(std::pow(10.0, db / 10.0));
}

}  // namespace ld2402
}  // namespace esphome
