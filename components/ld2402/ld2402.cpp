#include "ld2402.h"
#include "ld2402_web.h"
#include "esphome/core/log.h"
#include "esphome/core/application.h"
#include <esp_http_server.h>
#include <mbedtls/base64.h>
#include <cstring>
#include <algorithm>
#include <sstream>
#include <iomanip>

namespace esphome {
namespace ld2402 {

static const char *TAG = "ld2402";

// ═══════════════════════════════════════════════════════════════════
//  工具函数：构建完整命令帧
// ═══════════════════════════════════════════════════════════════════

static std::vector<uint8_t> build_frame(uint16_t cmd,
                                         const uint8_t *val = nullptr,
                                         size_t val_len = 0) {
    std::vector<uint8_t> f;
    for (auto b : CMD_HEADER) f.push_back(b);
    uint16_t dlen = 2 + (uint16_t)val_len;
    f.push_back(dlen & 0xFF);
    f.push_back((dlen >> 8) & 0xFF);
    f.push_back(cmd & 0xFF);
    f.push_back((cmd >> 8) & 0xFF);
    for (size_t i = 0; i < val_len; i++) f.push_back(val[i]);
    for (auto b : CMD_FOOTER) f.push_back(b);
    return f;
}

// ═══════════════════════════════════════════════════════════════════
//  生命周期
// ═══════════════════════════════════════════════════════════════════

void LD2402Component::setup() {
    ESP_LOGI(TAG, "LD2402 setup, web port %u", web_port_);

    cmd_read_firmware([this](const std::string &ver) {
        firmware_ver_ = ver;
        if (firmware_sensor_) firmware_sensor_->publish_state(ver);
        ESP_LOGI(TAG, "Firmware: %s", ver.c_str());
    });

    cmd_read_sn([this](const std::string &sn) {
        sn_str_ = sn;
        ESP_LOGI(TAG, "SN: %s", sn.c_str());
    });

    if (work_mode_sensor_) work_mode_sensor_->publish_state("Normal");
}

void LD2402Component::loop() {
    if (!web_started_ && network::is_connected()) {
        start_web_server_();
        web_started_ = true;
    }

    process_rx_();
    pump_cmd_queue_();
}

// ═══════════════════════════════════════════════════════════════════
//  UART 发送
// ═══════════════════════════════════════════════════════════════════

void LD2402Component::send_raw_(const std::vector<uint8_t> &frame) {
    write_array(frame.data(), frame.size());
}

// 直接发送 item.payload（已是完整帧）
void LD2402Component::send_cmd_frame_(const CmdQueueItem &item) {
    write_array(item.payload.data(), item.payload.size());
}

// ═══════════════════════════════════════════════════════════════════
//  接收解析
// ═══════════════════════════════════════════════════════════════════

void LD2402Component::process_rx_() {
    while (available()) {
        uint8_t b;
        read_byte(&b);
        parse_byte_(b);
    }
}

void LD2402Component::parse_byte_(uint8_t b) {
    switch (parse_state_) {

        case ParseState::IDLE:
            hdr_idx_ = 0;
            frame_buf_.clear();
            if (b == CMD_HEADER[0]) {
                parse_state_ = ParseState::CMD_HEADER;
                hdr_idx_ = 1;
            } else if (engineer_mode_ && b == DATA_HEADER[0]) {
                parse_state_ = ParseState::DAT_HEADER;
                hdr_idx_ = 1;
            } else {
                if (b == '\n') {
                    if (!line_buf_.empty() && line_buf_.back() == '\r')
                        line_buf_.pop_back();
                    if (!line_buf_.empty())
                        parse_normal_line_(line_buf_);
                    line_buf_.clear();
                } else if (b >= 0x20 || b == '\r') {
                    line_buf_ += (char)b;
                }
            }
            break;

        case ParseState::CMD_HEADER:
            if (b == CMD_HEADER[hdr_idx_]) {
                hdr_idx_++;
                if (hdr_idx_ == 4) {
                    parse_state_ = ParseState::CMD_LENGTH;
                    frame_len_ = 0;
                    frame_recv_ = 0;
                }
            } else {
                parse_state_ = ParseState::IDLE;
                line_buf_.clear();
            }
            break;

        case ParseState::CMD_LENGTH:
            if (frame_recv_ == 0) {
                frame_len_ = b;
                frame_recv_ = 1;
            } else {
                frame_len_ |= ((uint16_t)b << 8);
                frame_buf_.clear();
                frame_recv_ = 0;
                if (frame_len_ == 0 || frame_len_ > 256) {
                    parse_state_ = ParseState::IDLE;
                } else {
                    parse_state_ = ParseState::CMD_DATA;
                }
            }
            break;

        case ParseState::CMD_DATA:
            frame_buf_.push_back(b);
            frame_recv_++;
            if (frame_recv_ >= frame_len_) {
                hdr_idx_ = 0;
                parse_state_ = ParseState::CMD_FOOTER;
            }
            break;

        case ParseState::CMD_FOOTER:
            if (b == CMD_FOOTER[hdr_idx_]) {
                hdr_idx_++;
                if (hdr_idx_ == 4) {
                    dispatch_cmd_frame_(frame_buf_);
                    parse_state_ = ParseState::IDLE;
                }
            } else {
                parse_state_ = ParseState::IDLE;
            }
            break;

        case ParseState::DAT_HEADER:
            if (b == DATA_HEADER[hdr_idx_]) {
                hdr_idx_++;
                if (hdr_idx_ == 4) {
                    parse_state_ = ParseState::DAT_LENGTH;
                    frame_len_ = 0;
                    frame_recv_ = 0;
                }
            } else {
                parse_state_ = ParseState::IDLE;
            }
            break;

        case ParseState::DAT_LENGTH:
            if (frame_recv_ == 0) {
                frame_len_ = b;
                frame_recv_ = 1;
            } else {
                frame_len_ |= ((uint16_t)b << 8);
                frame_buf_.clear();
                frame_recv_ = 0;
                parse_state_ = ParseState::DAT_DATA;
            }
            break;

        case ParseState::DAT_DATA:
            frame_buf_.push_back(b);
            frame_recv_++;
            if (frame_recv_ >= frame_len_) {
                hdr_idx_ = 0;
                parse_state_ = ParseState::DAT_FOOTER;
            }
            break;

        case ParseState::DAT_FOOTER:
            if (b == DATA_FOOTER[hdr_idx_]) {
                hdr_idx_++;
                if (hdr_idx_ == 4) {
                    dispatch_data_frame_(frame_buf_);
                    parse_state_ = ParseState::IDLE;
                }
            } else {
                parse_state_ = ParseState::IDLE;
            }
            break;
    }
}

void LD2402Component::parse_normal_line_(const std::string &line) {
    if (line == "OFF") {
        detection_result_ = 0x00;
        target_distance_  = 0;
        if (presence_sensor_) presence_sensor_->publish_state(false);
        if (distance_sensor_)  distance_sensor_->publish_state(0);

    } else if (line.rfind("distance:", 0) == 0) {
        std::string num = line.substr(9);
        size_t end = num.find_first_not_of("0123456789");
        if (end != std::string::npos) num = num.substr(0, end);
        if (!num.empty()) {
            float dist = std::stof(num);
            detection_result_ = 0x01;

            // ✅ 收到 distance: 帧本身 = 有人，不管数值是否为 0
            if (presence_sensor_) presence_sensor_->publish_state(true);

            // ✅ 距离只在有效时更新，dist==0 保留上次有效值
            if (dist > 0) {
                target_distance_ = (uint16_t)dist;
                if (distance_sensor_) distance_sensor_->publish_state(dist);
            }
        }
    }
}


void LD2402Component::dispatch_cmd_frame_(const std::vector<uint8_t> &data) {
    if (data.size() < 4) return;
    on_ack_(data);
}

void LD2402Component::dispatch_data_frame_(const std::vector<uint8_t> &data) {
    if (data.size() < 131) return;

    detection_result_ = data[0];
    target_distance_  = (uint16_t)data[1] | ((uint16_t)data[2] << 8);

    size_t offset = 3;
    for (int i = 0; i < NUM_GATES; i++) {
        motion_energy_[i] = (uint32_t)data[offset]
                          | ((uint32_t)data[offset+1] << 8)
                          | ((uint32_t)data[offset+2] << 16)
                          | ((uint32_t)data[offset+3] << 24);
        offset += 4;
    }
    for (int i = 0; i < NUM_GATES; i++) {
        micro_energy_[i] = (uint32_t)data[offset]
                         | ((uint32_t)data[offset+1] << 8)
                         | ((uint32_t)data[offset+2] << 16)
                         | ((uint32_t)data[offset+3] << 24);
        offset += 4;
    }

    bool has_presence = (detection_result_ != 0x00);
    // 无人时距离归零，与正常模式 HA 实体行为一致
    if (!has_presence) target_distance_ = 0;

    if (presence_sensor_) presence_sensor_->publish_state(has_presence);
    if (distance_sensor_) distance_sensor_->publish_state(has_presence ? (float)target_distance_ : 0.0f);
}

// ═══════════════════════════════════════════════════════════════════
//  命令队列
// ═══════════════════════════════════════════════════════════════════

void LD2402Component::enqueue_cmd_(CmdQueueItem item) {
    cmd_queue_.push_back(std::move(item));
}

void LD2402Component::on_ack_(const std::vector<uint8_t> &data) {
    if (!cmd_in_flight_) return;
    auto cb = std::move(cmd_in_flight_->callback);
    cmd_in_flight_.reset();
    if (cb) cb(data);
}

void LD2402Component::pump_cmd_queue_() {
    if (cmd_in_flight_) {
        uint32_t elapsed = millis() - cmd_sent_ms_;
        uint32_t timeout = cmd_in_flight_->is_config_cmd
                         ? 3000
                         : cmd_in_flight_->timeout_ms;

        if (elapsed > timeout) {
            if (cmd_in_flight_->retry_count < 2) {
                ESP_LOGW(TAG, "Command timeout, retry %d/2",
                         cmd_in_flight_->retry_count + 1);
                cmd_in_flight_->retry_count++;
                send_cmd_frame_(*cmd_in_flight_);
                cmd_sent_ms_ = millis();
            } else {
                ESP_LOGW(TAG, "Command failed after 3 attempts");
                if (cmd_in_flight_->on_timeout)
                    cmd_in_flight_->on_timeout();
                cmd_in_flight_.reset();
            }
        }
        return;
    }

    if (cmd_queue_.empty()) return;

    cmd_in_flight_ = std::make_unique<CmdQueueItem>(std::move(cmd_queue_.front()));
    cmd_queue_.pop_front();
    cmd_in_flight_->retry_count = 0;
    send_cmd_frame_(*cmd_in_flight_);
    cmd_sent_ms_ = millis();
}

// ═══════════════════════════════════════════════════════════════════
//  公开命令接口
// ═══════════════════════════════════════════════════════════════════

void LD2402Component::cmd_read_firmware(
        std::function<void(const std::string&)> cb) {

    CmdQueueItem en;
    uint8_t en_val[] = {0x01, 0x00};
    en.payload       = build_frame(CMD_ENABLE_CONFIG, en_val, 2);
    en.is_config_cmd = true;
    en.callback = [this, cb](const std::vector<uint8_t> &ack) {
        if (ack.size() < 4 || ack[2] != 0) return;

        CmdQueueItem fw;
        fw.payload       = build_frame(CMD_READ_FW);
        fw.is_config_cmd = true;
        fw.callback = [this, cb](const std::vector<uint8_t> &d) {
            if (d.size() >= 7 && d[2] == 0) {
                uint16_t vlen = (uint16_t)d[4] | ((uint16_t)d[5] << 8);
                if (d.size() >= (size_t)(6 + vlen)) {
                    std::string ver(d.begin() + 6, d.begin() + 6 + vlen);
                    cb(ver);
                }
            }
            CmdQueueItem end;
            end.payload       = build_frame(CMD_END_CONFIG);
            end.is_config_cmd = true;
            end.callback      = [](const std::vector<uint8_t>&){};
            enqueue_cmd_(end);
        };
        enqueue_cmd_(fw);
    };
    enqueue_cmd_(en);
}

void LD2402Component::cmd_read_sn(
        std::function<void(const std::string&)> cb) {

    CmdQueueItem en;
    uint8_t en_val[] = {0x01, 0x00};
    en.payload       = build_frame(CMD_ENABLE_CONFIG, en_val, 2);
    en.is_config_cmd = true;
    en.callback = [this, cb](const std::vector<uint8_t> &ack) {
        if (ack.size() < 4 || ack[2] != 0) return;

        CmdQueueItem sn;
        sn.payload       = build_frame(CMD_READ_SN_STR);
        sn.is_config_cmd = true;
        sn.callback = [this, cb](const std::vector<uint8_t> &d) {
            if (d.size() >= 7 && d[2] == 0) {
                uint16_t slen = (uint16_t)d[4] | ((uint16_t)d[5] << 8);
                if (d.size() >= (size_t)(6 + slen)) {
                    std::string s(d.begin() + 6, d.begin() + 6 + slen);
                    cb(s);
                }
            }
            CmdQueueItem end;
            end.payload       = build_frame(CMD_END_CONFIG);
            end.is_config_cmd = true;
            end.callback      = [](const std::vector<uint8_t>&){};
            enqueue_cmd_(end);
        };
        enqueue_cmd_(sn);
    };
    enqueue_cmd_(en);
}

void LD2402Component::cmd_set_engineer_mode(
        bool enable, std::function<void(bool)> cb) {

    CmdQueueItem en;
    uint8_t en_val[] = {0x01, 0x00};
    en.payload       = build_frame(CMD_ENABLE_CONFIG, en_val, 2);
    en.is_config_cmd = true;
    en.callback = [this, enable, cb](const std::vector<uint8_t> &ack) {
        if (ack.size() < 4 || ack[2] != 0) { cb(false); return; }

        uint32_t mode_val = enable ? MODE_ENGINEER : MODE_NORMAL;
        uint8_t mv[6] = {
            0x00, 0x00,
            (uint8_t)(mode_val & 0xFF),
            (uint8_t)((mode_val >> 8) & 0xFF),
            (uint8_t)((mode_val >> 16) & 0xFF),
            (uint8_t)((mode_val >> 24) & 0xFF)
        };

        CmdQueueItem mode;
        mode.payload       = build_frame(CMD_SET_MODE, mv, 6);
        mode.is_config_cmd = true;
        mode.callback = [this, enable, cb](const std::vector<uint8_t> &d) {
            bool ok = (d.size() >= 4 && d[2] == 0);
            if (ok) {
                engineer_mode_ = enable;
                parse_state_   = ParseState::IDLE;
                line_buf_.clear();
                if (work_mode_sensor_)
                    work_mode_sensor_->publish_state(enable ? "Engineer" : "Normal");
            }
            cb(ok);

            CmdQueueItem end;
            end.payload       = build_frame(CMD_END_CONFIG);
            end.is_config_cmd = true;
            end.callback      = [](const std::vector<uint8_t>&){};
            enqueue_cmd_(end);
        };
        enqueue_cmd_(mode);
    };
    enqueue_cmd_(en);
}

void LD2402Component::cmd_read_gate_thresholds(
        bool micro,
        std::function<void(const std::vector<uint32_t>&)> cb) {

    uint16_t base = micro ? 0x0030 : 0x0010;
    std::vector<uint8_t> val;
    for (int i = 0; i < NUM_GATES; i++) {
        uint16_t id = base + i;
        val.push_back(id & 0xFF);
        val.push_back((id >> 8) & 0xFF);
    }

    CmdQueueItem en;
    uint8_t en_val[] = {0x01, 0x00};
    en.payload       = build_frame(CMD_ENABLE_CONFIG, en_val, 2);
    en.is_config_cmd = true;
    en.callback = [this, val, micro, cb](const std::vector<uint8_t>&) {
        CmdQueueItem rd;
        rd.payload       = build_frame(CMD_READ_PARAMS, val.data(), val.size());
        rd.is_config_cmd = true;
        rd.timeout_ms    = 1000;
        rd.callback = [this, micro, cb](const std::vector<uint8_t> &d) {
            if (d.size() < 4 || d[2] != 0) { cb({}); }
            else {
                std::vector<uint32_t> vals;
                size_t offset = 4;
                while (offset + 3 < d.size()) {
                    uint32_t v = (uint32_t)d[offset]
                               | ((uint32_t)d[offset+1] << 8)
                               | ((uint32_t)d[offset+2] << 16)
                               | ((uint32_t)d[offset+3] << 24);
                    vals.push_back(v);
                    offset += 4;
                }
                if (micro) {
                    for (int i = 0; i < NUM_GATES && i < (int)vals.size(); i++)
                        micro_thresholds_[i] = vals[i];
                } else {
                    for (int i = 0; i < NUM_GATES && i < (int)vals.size(); i++)
                        motion_thresholds_[i] = vals[i];
                }
                cb(vals);
            }
            CmdQueueItem end;
            end.payload       = build_frame(CMD_END_CONFIG);
            end.is_config_cmd = true;
            end.callback      = [](const std::vector<uint8_t>&){};
            enqueue_cmd_(end);
        };
        enqueue_cmd_(rd);
    };
    enqueue_cmd_(en);
}

void LD2402Component::cmd_write_gate_threshold(
        bool micro, uint8_t gate, uint32_t raw_val,
        std::function<void(bool)> cb) {

    uint16_t param_id = (micro ? 0x0030 : 0x0010) + gate;
    std::vector<std::pair<uint16_t, uint32_t>> params = {{param_id, raw_val}};
    cmd_write_params_batch(params, cb);
}

void LD2402Component::cmd_write_params_batch(
        const std::vector<std::pair<uint16_t,uint32_t>> &params,
        std::function<void(bool)> cb) {

    std::vector<uint8_t> val;
    for (auto &p : params) {
        val.push_back(p.first & 0xFF);
        val.push_back((p.first >> 8) & 0xFF);
        val.push_back(p.second & 0xFF);
        val.push_back((p.second >> 8) & 0xFF);
        val.push_back((p.second >> 16) & 0xFF);
        val.push_back((p.second >> 24) & 0xFF);
    }

    CmdQueueItem en;
    uint8_t en_val[] = {0x01, 0x00};
    en.payload       = build_frame(CMD_ENABLE_CONFIG, en_val, 2);
    en.is_config_cmd = true;
    en.callback = [this, val, cb](const std::vector<uint8_t>&) {
        CmdQueueItem wr;
        wr.payload       = build_frame(CMD_WRITE_PARAMS, val.data(), val.size());
        wr.is_config_cmd = true;
        wr.timeout_ms    = 2000;
        wr.callback = [this, cb](const std::vector<uint8_t> &d) {
            bool ok = (d.size() >= 4 && d[2] == 0);
            cb(ok);
            // 只退出配置，不保存
            CmdQueueItem end;
            end.payload       = build_frame(CMD_END_CONFIG);
            end.is_config_cmd = true;
            end.callback      = [](const std::vector<uint8_t>&){};
            enqueue_cmd_(end);
        };
        wr.on_timeout = [this, cb]() {
            ESP_LOGW(TAG, "Write params timeout");
            cb(false);
            CmdQueueItem end;
            end.payload       = build_frame(CMD_END_CONFIG);
            end.is_config_cmd = true;
            end.callback      = [](const std::vector<uint8_t>&){};
            enqueue_cmd_(end);
        };
        enqueue_cmd_(wr);
    };
    enqueue_cmd_(en);
}


void LD2402Component::cmd_save_params(std::function<void(bool)> cb) {
    CmdQueueItem en;
    uint8_t en_val[] = {0x01, 0x00};
    en.payload       = build_frame(CMD_ENABLE_CONFIG, en_val, 2);
    en.is_config_cmd = true;
    en.callback = [this, cb](const std::vector<uint8_t> &ack) {
        if (ack.size() < 4 || ack[2] != 0) { cb(false); return; }

        CmdQueueItem save;
        save.payload       = build_frame(CMD_SAVE_PARAMS);
        save.is_config_cmd = true;
        save.timeout_ms    = 3000;
        save.callback = [this, cb](const std::vector<uint8_t> &d) {
            bool ok = (d.size() >= 4 && d[2] == 0);
            ESP_LOGI(TAG, "Save to flash %s", ok ? "OK" : "FAIL");
            cb(ok);
            CmdQueueItem end;
            end.payload       = build_frame(CMD_END_CONFIG);
            end.is_config_cmd = true;
            end.callback      = [](const std::vector<uint8_t>&){};
            enqueue_cmd_(end);
        };
        save.on_timeout = [this, cb]() {
            ESP_LOGW(TAG, "Save to flash timeout");
            cb(false);
            CmdQueueItem end;
            end.payload       = build_frame(CMD_END_CONFIG);
            end.is_config_cmd = true;
            end.callback      = [](const std::vector<uint8_t>&){};
            enqueue_cmd_(end);
        };
        enqueue_cmd_(save);
    };
    enqueue_cmd_(en);
}


void LD2402Component::cmd_auto_threshold(
        uint16_t trig, uint16_t hold, uint16_t micro_coeff,
        std::function<void(bool)> cb) {

    CmdQueueItem en;
    uint8_t en_val[] = {0x01, 0x00};
    en.payload       = build_frame(CMD_ENABLE_CONFIG, en_val, 2);
    en.is_config_cmd = true;
    en.callback = [this, trig, hold, micro_coeff, cb](const std::vector<uint8_t>&) {
        uint8_t val[6] = {
            (uint8_t)(trig & 0xFF),        (uint8_t)((trig >> 8) & 0xFF),
            (uint8_t)(hold & 0xFF),        (uint8_t)((hold >> 8) & 0xFF),
            (uint8_t)(micro_coeff & 0xFF), (uint8_t)((micro_coeff >> 8) & 0xFF),
        };
        CmdQueueItem at;
        at.payload       = build_frame(CMD_AUTO_THRESHOLD, val, 6);
        at.is_config_cmd = true;
        at.timeout_ms    = 500;
        at.callback = [this, cb](const std::vector<uint8_t> &d) {
            bool ok = (d.size() >= 4 && d[2] == 0);
            cb(ok);
            CmdQueueItem end;
            end.payload       = build_frame(CMD_END_CONFIG);
            end.is_config_cmd = true;
            end.callback      = [](const std::vector<uint8_t>&){};
            enqueue_cmd_(end);
        };
        enqueue_cmd_(at);
    };
    enqueue_cmd_(en);
}

void LD2402Component::cmd_auto_progress(std::function<void(int)> cb) {
    CmdQueueItem en;
    uint8_t en_val[] = {0x01, 0x00};
    en.payload       = build_frame(CMD_ENABLE_CONFIG, en_val, 2);
    en.is_config_cmd = true;
    en.callback = [this, cb](const std::vector<uint8_t>&) {
        CmdQueueItem ap;
        ap.payload       = build_frame(CMD_AUTO_PROGRESS);
        ap.is_config_cmd = true;
        ap.callback = [this, cb](const std::vector<uint8_t> &d) {
            int progress = 0;
            if (d.size() >= 6 && d[2] == 0)
                progress = (int)((uint16_t)d[4] | ((uint16_t)d[5] << 8));
            cb(progress);
            CmdQueueItem end;
            end.payload       = build_frame(CMD_END_CONFIG);
            end.is_config_cmd = true;
            end.callback      = [](const std::vector<uint8_t>&){};
            enqueue_cmd_(end);
        };
        enqueue_cmd_(ap);
    };
    enqueue_cmd_(en);
}

void LD2402Component::cmd_auto_gain(std::function<void(bool)> cb) {
    CmdQueueItem en;
    uint8_t en_val[] = {0x01, 0x00};
    en.payload       = build_frame(CMD_ENABLE_CONFIG, en_val, 2);
    en.is_config_cmd = true;
    en.callback = [this, cb](const std::vector<uint8_t>&) {
        CmdQueueItem ag;
        ag.payload       = build_frame(CMD_AUTO_GAIN);
        ag.is_config_cmd = true;
        ag.timeout_ms    = 5000;
        ag.callback = [this, cb](const std::vector<uint8_t> &d) {
            bool ok = (d.size() >= 4 && d[2] == 0);
            cb(ok);
            CmdQueueItem end;
            end.payload       = build_frame(CMD_END_CONFIG);
            end.is_config_cmd = true;
            end.callback      = [](const std::vector<uint8_t>&){};
            enqueue_cmd_(end);
        };
        enqueue_cmd_(ag);
    };
    enqueue_cmd_(en);
}

void LD2402Component::cmd_read_param(uint16_t id,
                                      std::function<void(uint32_t)> cb) {
    CmdQueueItem en;
    uint8_t en_val[] = {0x01, 0x00};
    en.payload       = build_frame(CMD_ENABLE_CONFIG, en_val, 2);
    en.is_config_cmd = true;
    en.callback = [this, id, cb](const std::vector<uint8_t>&) {
        uint8_t val[2] = {(uint8_t)(id & 0xFF), (uint8_t)((id >> 8) & 0xFF)};
        CmdQueueItem rp;
        rp.payload       = build_frame(CMD_READ_PARAMS, val, 2);
        rp.is_config_cmd = true;
        rp.callback = [this, cb](const std::vector<uint8_t> &d) {
            uint32_t v = 0;
            if (d.size() >= 8 && d[2] == 0) {
                v = (uint32_t)d[4] | ((uint32_t)d[5] << 8)
                  | ((uint32_t)d[6] << 16) | ((uint32_t)d[7] << 24);
            }
            cb(v);
            CmdQueueItem end;
            end.payload       = build_frame(CMD_END_CONFIG);
            end.is_config_cmd = true;
            end.callback      = [](const std::vector<uint8_t>&){};
            enqueue_cmd_(end);
        };
        enqueue_cmd_(rp);
    };
    enqueue_cmd_(en);
}

// ═══════════════════════════════════════════════════════════════════
//  SSE 客户端管理
// ═══════════════════════════════════════════════════════════════════

void LD2402Component::add_sse_client(httpd_handle_t h, int fd) {
    sse_clients_.push_back({h, fd});
    ESP_LOGI(TAG, "SSE client connected fd=%d total=%d",
             fd, (int)sse_clients_.size());
}

void LD2402Component::remove_sse_client(int fd) {
    sse_clients_.erase(
        std::remove_if(sse_clients_.begin(), sse_clients_.end(),
                       [fd](const SseClient &c) { return c.fd == fd; }),
        sse_clients_.end());
}

void LD2402Component::push_sse_energy_() {
    if (sse_clients_.empty()) return;

    char buf[1024];
    int off = snprintf(buf, sizeof(buf),
        "data:{\"result\":%d,\"dist\":%d,\"motion\":[",
        detection_result_, target_distance_);

    for (int i = 0; i < NUM_GATES; i++) {
        off += snprintf(buf + off, sizeof(buf) - off,
                        "%.2f%s", raw_to_db(motion_energy_[i]),
                        i < NUM_GATES - 1 ? "," : "");
    }
    off += snprintf(buf + off, sizeof(buf) - off, "],\"micro\":[");
    for (int i = 0; i < NUM_GATES; i++) {
        off += snprintf(buf + off, sizeof(buf) - off,
                        "%.2f%s", raw_to_db(micro_energy_[i]),
                        i < NUM_GATES - 1 ? "," : "");
    }
    off += snprintf(buf + off, sizeof(buf) - off, "]}\n\n");

    std::vector<int> dead;
    for (auto &c : sse_clients_) {
        int ret = httpd_socket_send(c.handle, c.fd, buf, off, 0);
        if (ret < 0) dead.push_back(c.fd);
    }
    for (int fd : dead) remove_sse_client(fd);
}

// ═══════════════════════════════════════════════════════════════════
//  Web 服务器
// ═══════════════════════════════════════════════════════════════════

bool LD2402Component::check_basic_auth_(httpd_req_t *req) {
    auto send_401 = [](httpd_req_t *r) {
        httpd_resp_set_status(r, "401 Unauthorized");
        httpd_resp_set_type(r, "text/plain");
        httpd_resp_set_hdr(r, "WWW-Authenticate", "Basic realm=\"LD2402 Radar\"");
        httpd_resp_sendstr(r, "Unauthorized");
    };

    char auth_buf[300] = {0};
    esp_err_t ret = httpd_req_get_hdr_value_str(req, "Authorization",
                                                 auth_buf, sizeof(auth_buf));
    if (ret != ESP_OK) { send_401(req); return false; }

    if (strncmp(auth_buf, "Basic ", 6) != 0) { send_401(req); return false; }

    const char *b64 = auth_buf + 6;
    size_t b64_len  = strlen(b64);
    while (b64_len > 0 && (b64[b64_len-1] == '\r' ||
                            b64[b64_len-1] == '\n' ||
                            b64[b64_len-1] == ' ')) {
        b64_len--;
    }

    unsigned char decoded[150] = {0};
    size_t olen = 0;
    if (mbedtls_base64_decode(decoded, sizeof(decoded) - 1, &olen,
                               (const unsigned char *)b64, b64_len) != 0) {
        send_401(req);
        return false;
    }
    decoded[olen] = '\0';

    char expected[130] = {0};
    snprintf(expected, sizeof(expected), "%s:%s",
             web_user_.c_str(), web_pass_.c_str());

    if (strcmp((char *)decoded, expected) != 0) {
        ESP_LOGW(TAG, "Auth failed: got '%s'", (char *)decoded);
        send_401(req);
        return false;
    }
    return true;
}

esp_err_t LD2402Component::handle_root_(httpd_req_t *req) {
    auto *self = (LD2402Component *)req->user_ctx;
    if (!self->check_basic_auth_(req)) return ESP_OK;

    httpd_resp_set_type(req, "text/html; charset=utf-8");
    httpd_resp_set_hdr(req, "Cache-Control", "no-cache");
    extern const char LD2402_WEB_HTML[];
    extern const size_t LD2402_WEB_HTML_SIZE;
    httpd_resp_send(req, LD2402_WEB_HTML, LD2402_WEB_HTML_SIZE);
    return ESP_OK;
}

esp_err_t LD2402Component::handle_api_info_(httpd_req_t *req) {
    auto *self = (LD2402Component *)req->user_ctx;
    if (!self->check_basic_auth_(req)) return ESP_OK;

    char buf[512];
    snprintf(buf, sizeof(buf),
        "{\"fw\":\"%s\",\"sn\":\"%s\","
        "\"engineer\":%s,\"result\":%d,\"dist\":%d,"
        "\"motion_th\":[",
        self->firmware_ver_.c_str(), self->sn_str_.c_str(),
        self->engineer_mode_ ? "true" : "false",
        self->detection_result_, self->target_distance_);

    std::string json(buf);
    for (int i = 0; i < NUM_GATES; i++) {
        char tmp[32];
        snprintf(tmp, sizeof(tmp), "%u%s",
                 self->motion_thresholds_[i],
                 i < NUM_GATES - 1 ? "," : "");
        json += tmp;
    }
    json += "],\"micro_th\":[";
    for (int i = 0; i < NUM_GATES; i++) {
        char tmp[32];
        snprintf(tmp, sizeof(tmp), "%u%s",
                 self->micro_thresholds_[i],
                 i < NUM_GATES - 1 ? "," : "");
        json += tmp;
    }
    json += "],\"max_distance\":" + std::to_string(self->max_distance_gates_)
          + ",\"timeout\":"       + std::to_string(self->absence_timeout_) + "}";

    httpd_resp_set_type(req, "application/json");
    httpd_resp_send(req, json.c_str(), json.size());
    return ESP_OK;
}


esp_err_t LD2402Component::handle_api_cmd_(httpd_req_t *req) {
    auto *self = (LD2402Component *)req->user_ctx;
    if (!self->check_basic_auth_(req)) return ESP_OK;

    char body[1024] = {0};
    int ret = httpd_req_recv(req, body, sizeof(body) - 1);
    if (ret <= 0) {
        httpd_resp_send_err(req, HTTPD_400_BAD_REQUEST, "No body");
        return ESP_OK;
    }
    body[ret] = 0;

    auto get_str_val = [](const char *json, const char *key) -> std::string {
        std::string s(json);
        std::string k = std::string("\"") + key + "\":\"";
        auto pos = s.find(k);
        if (pos == std::string::npos) return "";
        pos += k.size();
        auto end = s.find('"', pos);
        if (end == std::string::npos) return "";
        return s.substr(pos, end - pos);
    };
    auto get_num_val = [](const char *json, const char *key) -> double {
        std::string s(json);
        std::string k = std::string("\"") + key + "\":";
        auto pos = s.find(k);
        if (pos == std::string::npos) return 0;
        pos += k.size();
        return std::stod(s.substr(pos));
    };

    std::string cmd = get_str_val(body, "cmd");
    httpd_resp_set_type(req, "application/json");

    if (cmd == "set_engineer") {
        bool enable = get_num_val(body, "value") != 0;
        self->cmd_set_engineer_mode(enable, [](bool) {});
        httpd_resp_sendstr(req, "{\"ok\":true}");

    } else if (cmd == "read_thresholds") {
        bool micro = get_num_val(body, "micro") != 0;
        self->cmd_read_gate_thresholds(micro, [](const std::vector<uint32_t>&) {});
        httpd_resp_sendstr(req, "{\"ok\":true}");

    } else if (cmd == "write_threshold") {
        bool     micro = get_num_val(body, "micro") != 0;
        int      gate  = (int)get_num_val(body, "gate");
        uint32_t raw   = (uint32_t)get_num_val(body, "value");
        self->cmd_write_gate_threshold(micro, (uint8_t)gate, raw, [](bool) {});
        httpd_resp_sendstr(req, "{\"ok\":true}");

    } else if (cmd == "auto_threshold") {
        uint16_t trig  = (uint16_t)get_num_val(body, "trig");
        uint16_t hold  = (uint16_t)get_num_val(body, "hold");
        uint16_t micro = (uint16_t)get_num_val(body, "micro");
        self->cmd_auto_threshold(trig, hold, micro, [](bool) {});
        httpd_resp_sendstr(req, "{\"ok\":true}");

    } else if (cmd == "auto_progress") {
        httpd_resp_sendstr(req, "{\"ok\":true,\"progress\":0}");

    } else if (cmd == "auto_gain") {
        self->cmd_auto_gain([](bool) {});
        httpd_resp_sendstr(req, "{\"ok\":true}");

    } else if (cmd == "save_flash") {
        self->cmd_save_params([](bool ok) {
            ESP_LOGI("ld2402", "Flash save result: %s", ok ? "OK" : "FAIL");
        });
        httpd_resp_sendstr(req, "{\"ok\":true}");

    } else if (cmd == "set_max_distance") {
        uint32_t gates = (uint32_t)get_num_val(body, "value");
        gates = std::min((uint32_t)16, std::max((uint32_t)1, gates));
        self->max_distance_gates_ = gates;
        // ✅ 每门 0.7m，转换为 0.1m 单位（×7），上限 100
        uint32_t param_val = gates * 7;
        if (param_val > 100) param_val = 100;
        self->cmd_write_params_batch({{0x0001, param_val}}, [](bool) {});
        httpd_resp_sendstr(req, "{\"ok\":true}");

    } else if (cmd == "set_timeout") {
        uint32_t secs = (uint32_t)get_num_val(body, "value");
        secs = std::min((uint32_t)3600, std::max((uint32_t)0, secs));
        self->absence_timeout_ = secs;
        self->cmd_write_params_batch({{0x0004, secs}}, [](bool) {});
        httpd_resp_sendstr(req, "{\"ok\":true}");


    } else if (cmd == "read_info") {
        self->cmd_read_firmware([self](const std::string &ver) {
            self->firmware_ver_ = ver;
            if (self->firmware_sensor_) self->firmware_sensor_->publish_state(ver);
        });
        self->cmd_read_sn([self](const std::string &sn) { self->sn_str_ = sn; });
        self->cmd_read_gate_thresholds(false, [](const std::vector<uint32_t>&) {});
        self->cmd_read_gate_thresholds(true,  [](const std::vector<uint32_t>&) {});
        self->cmd_read_param(0x0001, [self](uint32_t v) {
            if (v >= 7 && v <= 100) self->max_distance_gates_ = v / 7;
        });
        self->cmd_read_param(0x0004, [self](uint32_t v) { self->absence_timeout_ = v; });
        httpd_resp_sendstr(req, "{\"ok\":true}");


    } else {
        httpd_resp_send_err(req, HTTPD_400_BAD_REQUEST, "Unknown command");
    }
    return ESP_OK;
}

esp_err_t LD2402Component::handle_sse_(httpd_req_t *req) {
    auto *self = (LD2402Component *)req->user_ctx;
    if (!self->check_basic_auth_(req)) return ESP_OK;

    httpd_resp_set_type(req, "text/event-stream");
    httpd_resp_set_hdr(req, "Cache-Control", "no-cache");
    httpd_resp_set_hdr(req, "Connection", "keep-alive");
    httpd_resp_set_hdr(req, "Access-Control-Allow-Origin", "*");

    const char *hello = "data:{\"connected\":true}\n\n";
    if (httpd_resp_send_chunk(req, hello, strlen(hello)) != ESP_OK) {
        return ESP_OK;
    }
    ESP_LOGI(TAG, "SSE client connected");

    while (true) {
        if (self->engineer_mode_) {
            vTaskDelay(pdMS_TO_TICKS(100));

            char buf[1100];
            int len = snprintf(buf, sizeof(buf),
                "data:{\"result\":%d,\"dist\":%d,\"motion\":[",
                self->detection_result_, self->target_distance_);

            for (int i = 0; i < NUM_GATES; i++) {
                len += snprintf(buf + len, sizeof(buf) - len,
                    "%.2f%s", raw_to_db(self->motion_energy_[i]),
                    i < NUM_GATES - 1 ? "," : "");
            }
            len += snprintf(buf + len, sizeof(buf) - len, "],\"micro\":[");
            for (int i = 0; i < NUM_GATES; i++) {
                len += snprintf(buf + len, sizeof(buf) - len,
                    "%.2f%s", raw_to_db(self->micro_energy_[i]),
                    i < NUM_GATES - 1 ? "," : "");
            }
            len += snprintf(buf + len, sizeof(buf) - len, "]}\n\n");

            if (httpd_resp_send_chunk(req, buf, len) != ESP_OK) {
                ESP_LOGI(TAG, "SSE client disconnected");
                break;
            }
        } else {
            vTaskDelay(pdMS_TO_TICKS(500));

            char buf[128];
            int len = snprintf(buf, sizeof(buf),
                "data:{\"result\":%d,\"dist\":%d}\n\n",
                self->detection_result_, self->target_distance_);

            if (httpd_resp_send_chunk(req, buf, len) != ESP_OK) {
                ESP_LOGI(TAG, "SSE client disconnected");
                break;
            }
        }
    }

    httpd_resp_send_chunk(req, NULL, 0);
    return ESP_OK;
}

void LD2402Component::start_web_server_() {
    httpd_config_t config    = HTTPD_DEFAULT_CONFIG();
    config.server_port       = web_port_;
    config.ctrl_port         = web_port_ + 1;
    config.max_open_sockets  = 9;
    config.lru_purge_enable  = true;
    config.stack_size        = 10240;
    config.task_priority     = tskIDLE_PRIORITY + 5;

    if (httpd_start(&httpd_, &config) != ESP_OK) {
        ESP_LOGE(TAG, "Failed to start httpd");
        return;
    }

    httpd_uri_t uris[] = {
        {"/",         HTTP_GET,  handle_root_,      this},
        {"/api/info", HTTP_GET,  handle_api_info_,  this},
        {"/api/cmd",  HTTP_POST, handle_api_cmd_,   this},
        {"/sse",      HTTP_GET,  handle_sse_,        this},
    };
    for (auto &u : uris) {
        httpd_register_uri_handler(httpd_, &u);
    }

    ESP_LOGI(TAG, "Web server started on port %u", web_port_);
}

}  // namespace ld2402
}  // namespace esphome
