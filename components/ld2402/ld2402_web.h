#pragma once

namespace esphome {
namespace ld2402 {

const char LD2402_WEB_HTML[] = R"HTMLEOF(
<!DOCTYPE html>
<html lang="zh-CN">
<head>
<meta charset="UTF-8"/>
<meta name="viewport" content="width=device-width,initial-scale=1,maximum-scale=1"/>
<title>LD2402-Web</title>
<style>
*{box-sizing:border-box;margin:0;padding:0}
body{font-family:-apple-system,BlinkMacSystemFont,"Segoe UI",sans-serif;
     background:#111;color:#e0e0e0;min-height:100vh}
.wrap{max-width:700px;margin:0 auto;padding:12px}
.topbar{display:flex;align-items:center;justify-content:space-between;
        background:#1a1a2e;border-radius:12px;padding:12px 16px;margin-bottom:12px}
.topbar h1{font-size:1.1rem;font-weight:700;color:#7ecfff}
.badge{font-size:.72rem;padding:3px 8px;border-radius:20px;background:#263;color:#8f8}
.badge.away{background:#422;color:#f88}
.badge.unknown{background:#2a2a2a;color:#666}
.card{background:#1a1a2e;border-radius:12px;padding:14px;margin-bottom:12px}
.card-title{font-size:.8rem;font-weight:600;color:#888;text-transform:uppercase;
            letter-spacing:.06em}
.status-row{display:flex;gap:24px;align-items:center;flex-wrap:wrap}
.info-item label{font-size:.72rem;color:#666}
.info-item span{font-size:.9rem;font-weight:600;color:#ddd;margin-left:6px}
.gate-header{display:flex;align-items:center;justify-content:space-between;margin-bottom:10px}
.gate-status{display:flex;align-items:center;gap:8px}
.dist-label{font-size:.78rem;color:#aaa;white-space:nowrap}
.btn-row{display:flex;flex-wrap:wrap;gap:8px;margin-top:6px}
.btn{padding:8px 16px;border:none;border-radius:8px;cursor:pointer;
     font-size:.82rem;font-weight:600;transition:opacity .15s}
.btn:active{opacity:.7}
.btn-primary{background:#1565c0;color:#fff}
.btn-success{background:#2e7d32;color:#fff}
.btn-warn{background:#e65100;color:#fff}
.btn-info{background:#00838f;color:#fff}
.btn-danger{background:#b71c1c;color:#fff}
.btn-toggle{background:#37474f;color:#cfd8dc}
.btn-toggle.on{background:#1b5e20;color:#a5d6a7}
.log-box{background:#0d1117;border-radius:8px;padding:10px;
         height:120px;overflow-y:auto;font-size:.78rem;
         font-family:"Courier New",monospace;color:#80ff80;border:1px solid #222}
.log-box p{margin:1px 0;line-height:1.5}
.tabs{display:flex;gap:4px;margin-bottom:10px}
.tab{flex:1;padding:8px;text-align:center;border-radius:8px;
     background:#0d1117;cursor:pointer;font-size:.82rem;
     color:#888;border:1px solid #222;transition:all .15s}
.tab.active{background:#1565c0;color:#fff;border-color:#1565c0}
/* 门列表：两行布局 */
.gate-list{display:flex;flex-direction:column;gap:12px}
.gate-row{display:flex;flex-direction:column;gap:3px}
.gate-row-top{display:flex;justify-content:space-between;align-items:baseline}
.gate-label{font-size:.72rem;color:#666}
.bar-val{font-size:.68rem;white-space:nowrap;line-height:1.5}
/* 图柱 */
.bar-track{position:relative;height:20px;background:#0d1117;border-radius:6px;overflow:visible}
.bar-energy{position:absolute;top:2px;left:0;height:16px;border-radius:4px;
            background:linear-gradient(90deg,#1565c0,#42a5f5);
            transition:width .1s linear;min-width:0}
.bar-threshold{position:absolute;top:0;left:0;height:20px;border-radius:6px;
               background:rgba(255,165,0,.25);pointer-events:none}
.slider-thumb{position:absolute;top:-3px;width:18px;height:26px;border-radius:6px;
              background:#ff9800;cursor:ew-resize;box-shadow:0 2px 6px rgba(0,0,0,.5);
              transform:translateX(-50%);touch-action:none;z-index:10}
.config-grid{display:flex;flex-direction:column;gap:10px}
.config-item label{font-size:.75rem;color:#888;display:block;margin-bottom:4px}
.config-control{display:flex;align-items:center;gap:10px}
.config-control input[type=range]{flex:1;accent-color:#1565c0;cursor:pointer}
.config-control span{font-size:.78rem;color:#ddd;white-space:nowrap;min-width:110px}
.coeff-row{display:flex;gap:8px;align-items:center;flex-wrap:wrap;margin-bottom:10px}
.coeff-row label{font-size:.78rem;color:#888}
.coeff-row input{width:64px;background:#0d1117;border:1px solid #333;
                 border-radius:6px;padding:4px 6px;color:#ddd;font-size:.82rem}
.progress-bar-outer{height:10px;background:#0d1117;border-radius:5px;overflow:hidden;margin:8px 0}
.progress-bar-inner{height:100%;background:#1565c0;border-radius:5px;transition:width .3s}
@media(max-width:480px){
  .status-row{gap:14px}
}
</style>
</head>
<body>
<div class="wrap">

<!-- 顶栏 -->
<div class="topbar">
  <h1>📊 HLK-LD2402 配置工具</h1>
</div>

<!-- 状态卡片 -->
<div class="card">
  <div class="card-title" style="margin-bottom:10px">设备状态</div>
  <div class="status-row">
    <div class="info-item">
      <label>固件版本</label><span id="fw">-</span>
    </div>
    <div class="info-item">
      <label>序列号</label><span id="sn">-</span>
    </div>
  </div>
</div>

<!-- 基本配置 -->
<div class="card">
  <div class="card-title" style="margin-bottom:10px">基本配置</div>
  <div class="config-grid">
    <div class="config-item">
      <label>最大检测距离</label>
      <div class="config-control">
        <input type="range" id="max-dist-slider" min="1" max="16" step="1" value="12"
               oninput="updateDistLabel(this.value)">
        <span id="max-dist-label">8.4 m（12 门）</span>
      </div>
    </div>
    <div class="config-item">
      <label>无人超时时间</label>
      <div class="config-control">
        <input type="number" id="timeout-input" min="0" max="3600" step="5" value="30"
               style="width:80px;background:#0d1117;border:1px solid #333;border-radius:6px;
                      padding:4px 8px;color:#ddd;font-size:.85rem">
        <span style="font-size:.78rem;color:#888;min-width:0">秒</span>
      </div>
    </div>
  </div>
  <div class="btn-row" style="margin-top:10px">
    <button class="btn btn-primary" onclick="applyBasicConfig()">✔ 应用配置</button>
  </div>
</div>

<!-- 功能按钮 -->
<div class="card">
  <div class="card-title" style="margin-bottom:10px">操作</div>
  <div class="btn-row">
    <button class="btn btn-info"    onclick="apiCmd('read_info')">🔄 加载配置</button>
    <button class="btn btn-toggle"  id="btn-eng" onclick="toggleEngineer()">⚙️ 工程模式</button>
    <button class="btn btn-success" onclick="apiCmd('auto_gain')">📶 自动增益</button>
    <button class="btn btn-danger"  onclick="saveFlash()">💾 保存到Flash</button>
  </div>
</div>


<!-- 日志输出 -->
<div class="card">
  <div class="card-title" style="margin-bottom:10px">日志输出</div>
  <div class="log-box" id="log-box"></div>
</div>

<!-- 门限调整 -->
<div class="card">
  <div class="gate-header">
    <div class="card-title">门限调整（先开启工程模式）</div>
    <div class="gate-status">
      <span id="presence-badge" class="badge unknown">未知</span>
      <span id="dist" class="dist-label">- -</span>
    </div>
  </div>
  <div class="tabs">
    <div class="tab active" id="tab-motion" onclick="switchTab('motion')">运动触发</div>
    <div class="tab"        id="tab-micro"  onclick="switchTab('micro')">微动保持</div>
  </div>
  <div id="gates-motion" class="gate-list"></div>
  <div id="gates-micro"  class="gate-list" style="display:none"></div>
</div>

</div><!-- /wrap -->

<script>
const NUM_GATES = 16;
const DB_MIN    = 20;   // 显示区间下限 dB
const DB_MAX    = 70;   // 显示区间上限 dB
const EMA_ALPHA = 0.2;  // 平滑系数

let engineerMode = false;
let currentTab   = 'motion';
let sseSource    = null;

const thresholds = {
  motion: new Array(NUM_GATES).fill(10000),
  micro:  new Array(NUM_GATES).fill(10000),
};
const energies = {
  motion: new Array(NUM_GATES).fill(0),
  micro:  new Array(NUM_GATES).fill(0),
};
const smoothed = {
  motion: new Array(NUM_GATES).fill(0),
  micro:  new Array(NUM_GATES).fill(0),
};

function rawToDb(raw) {
  if (!raw || raw < 1) return 0;
  return 10 * Math.log10(raw);
}
function dbToRaw(db) {
  if (db <= 0) return 0;
  return Math.round(Math.pow(10, db / 10));
}
function rawToPercent(raw) {
  if (!raw || raw < 1) return 0;
  const db = rawToDb(raw);
  const clamped = Math.min(DB_MAX, Math.max(DB_MIN, db));
  return (clamped - DB_MIN) / (DB_MAX - DB_MIN) * 100;
}
function percentToRaw(pct) {
  const db      = DB_MIN + (pct / 100) * (DB_MAX - DB_MIN);
  const snapped = Math.round(db * 10) / 10;
  return dbToRaw(snapped);
}
function fmtDb(raw) {
  if (!raw || raw < 1) return '- dB';
  return rawToDb(raw).toFixed(1) + ' dB';
}

// ── 初始化 ────────────────────────────────────────────────────
function init() {
  buildGates('motion');
  buildGates('micro');
  fetchInfo();
}

function buildGates(type) {
  const container = document.getElementById('gates-' + type);
  container.innerHTML = '';
  for (let i = 0; i < NUM_GATES; i++) {
    const initRaw = thresholds[type][i];
    const initPct = rawToPercent(initRaw);
    const row = document.createElement('div');
    row.className = 'gate-row';
    row.innerHTML = `
      <div class="gate-row-top">
        <div class="gate-label">G${i}</div>
        <div class="bar-val">
          <span id="elabel-${type}-${i}" style="color:#42a5f5">- -</span>
          <span style="color:#444"> / </span>
          <span id="thlabel-${type}-${i}" style="color:#f9a825">${fmtDb(initRaw)}</span>
        </div>
      </div>
      <div class="bar-track" id="track-${type}-${i}">
        <div class="bar-energy"    id="energy-${type}-${i}"        style="width:0%"></div>
        <div class="bar-threshold" id="threshold-bar-${type}-${i}" style="width:${initPct}%"></div>
        <div class="slider-thumb"  id="thumb-${type}-${i}"
             style="left:${initPct}%"
             data-type="${type}" data-gate="${i}"></div>
      </div>
    `;
    container.appendChild(row);
    initDrag(`thumb-${type}-${i}`, type, i);
  }
}

// ── 拖拽滑块 ──────────────────────────────────────────────────
function initDrag(thumbId, type, gate) {
  const thumb = document.getElementById(thumbId);
  let dragging = false;

  const onStart = (e) => { dragging = true; e.preventDefault(); };

  const onMove = (e) => {
    if (!dragging) return;
    const track   = document.getElementById(`track-${type}-${gate}`);
    const rect    = track.getBoundingClientRect();
    const clientX = e.touches ? e.touches[0].clientX : e.clientX;
    let ratio = (clientX - rect.left) / rect.width;
    ratio = Math.min(1, Math.max(0, ratio));
    updateThumbUI(type, gate, percentToRaw(ratio * 100));
  };

  const onEnd = () => {
    if (!dragging) return;
    dragging = false;
    const raw = thresholds[type][gate];
    log(`写入门限: ${type} G${gate} = ${raw} raw (${fmtDb(raw)})`);
    pauseSSE();
    apiCmd('write_threshold', { micro: type === 'micro' ? 1 : 0, gate, value: raw })
      .finally(() => resumeSSE());
  };

  thumb.addEventListener('mousedown',  onStart);
  thumb.addEventListener('touchstart', onStart, { passive: false });
  document.addEventListener('mousemove',  onMove);
  document.addEventListener('touchmove',  onMove, { passive: false });
  document.addEventListener('mouseup',    onEnd);
  document.addEventListener('touchend',   onEnd);
}

function updateThumbUI(type, gate, raw) {
  thresholds[type][gate] = raw;
  const p = rawToPercent(raw);
  document.getElementById(`thumb-${type}-${gate}`).style.left          = p + '%';
  document.getElementById(`threshold-bar-${type}-${gate}`).style.width = p + '%';
  document.getElementById(`thlabel-${type}-${gate}`).textContent        = fmtDb(raw);
}

// ── 能量条更新（EMA 平滑）────────────────────────────────────
function updateEnergyBars(data) {
  ['motion', 'micro'].forEach(type => {
    const arr = data[type === 'motion' ? 'motion' : 'micro'];
    if (!arr) return;
    arr.forEach((raw, i) => {
      energies[type][i] = raw;

      smoothed[type][i] = EMA_ALPHA * raw + (1 - EMA_ALPHA) * smoothed[type][i];
      const display = smoothed[type][i];

      const bar    = document.getElementById(`energy-${type}-${i}`);
      const elabel = document.getElementById(`elabel-${type}-${i}`);
      if (!bar) return;

      const pct = Math.min(100, Math.max(0,
        (display - DB_MIN) / (DB_MAX - DB_MIN) * 100));
      bar.style.width = pct + '%';

      if (elabel) {
        elabel.textContent = display > DB_MIN ? display.toFixed(1) + ' dB' : '- -';
      }
    });
  });
}

// ── 选项卡 ────────────────────────────────────────────────────
function switchTab(type) {
  currentTab = type;
  ['motion', 'micro'].forEach(t => {
    document.getElementById('tab-'   + t).classList.toggle('active', t === type);
    document.getElementById('gates-' + t).style.display = t === type ? '' : 'none';
  });
}

// ── 工程模式切换 ──────────────────────────────────────────────
function toggleEngineer() {
  engineerMode = !engineerMode;
  const btn = document.getElementById('btn-eng');
  btn.classList.toggle('on', engineerMode);
  btn.textContent = engineerMode ? '🔬 工程模式 ON' : '⚙️ 工程模式';
  apiCmd('set_engineer', { value: engineerMode ? 1 : 0 });

  if (engineerMode) {
    startSSE();
  } else {
    stopSSE();
    const badge = document.getElementById('presence-badge');
    badge.textContent = '未知';
    badge.className   = 'badge unknown';
    document.getElementById('dist').textContent = '- -';
    ['motion', 'micro'].forEach(type => {
      for (let i = 0; i < NUM_GATES; i++) {
        const bar    = document.getElementById(`energy-${type}-${i}`);
        const elabel = document.getElementById(`elabel-${type}-${i}`);
        if (bar)    bar.style.width    = '0%';
        if (elabel) elabel.textContent = '- -';
        smoothed[type][i] = 0;
      }
    });
  }
}

// ── SSE ───────────────────────────────────────────────────────
function startSSE() {
  if (sseSource) sseSource.close();
  sseSource = new EventSource('/sse');
  sseSource.onmessage = (e) => {
    try {
      const d = JSON.parse(e.data);
      if (d.connected) return;
      if (engineerMode) {
        document.getElementById('dist').textContent = d.dist + ' cm';
        const present = d.result !== 0;
        const badge   = document.getElementById('presence-badge');
        badge.textContent = d.result === 0 ? '无人' : d.result === 1 ? '有人' : '有人(静止)';
        badge.className   = 'badge ' + (present ? '' : 'away');
        updateEnergyBars(d);
      }
    } catch(err) {}
  };
  sseSource.onerror = () => {
    log('SSE 连接断开，5s后重连...');
    stopSSE();
    setTimeout(() => { if (engineerMode) startSSE(); }, 5000);
  };
}

function stopSSE()  { if (sseSource) { sseSource.close(); sseSource = null; } }
function pauseSSE() { if (sseSource) { sseSource.close(); sseSource = null; } }
function resumeSSE(delayMs = 800) {
  setTimeout(() => { if (engineerMode) startSSE(); }, delayMs);
}

// ── 基本配置 ──────────────────────────────────────────────────
function updateDistLabel(gates) {
  const m = (gates * 0.7).toFixed(1);
  document.getElementById('max-dist-label').textContent = `${m} m（${gates} 门）`;
}

async function applyBasicConfig() {
  const gates   = parseInt(document.getElementById('max-dist-slider').value);
  const timeout = parseInt(document.getElementById('timeout-input').value);
  pauseSSE();
  log(`应用配置: 最大距离=${(gates*0.7).toFixed(1)}m  超时=${timeout}s`);
  try {
    const r1 = await apiCmd('set_max_distance', { value: gates });
    const r2 = await apiCmd('set_timeout',      { value: timeout });
    log('基本配置: ' + (r1?.ok && r2?.ok ? '✅ 成功' : '❌ 部分失败'));
  } catch(e) {
    log('基本配置失败: ' + e.message);
  } finally {
    resumeSSE(800);
  }
}

// ── 保存到 Flash ──────────────────────────────────────────────
async function saveFlash() {
  log('正在保存到 Flash...');
  pauseSSE();
  try {
    const res = await fetch('/api/cmd', {
      method: 'POST',
      headers: { 'Content-Type': 'application/json' },
      body: JSON.stringify({ cmd: 'save_flash' })
    });
    const d = await res.json();
    log('Flash 保存: ' + (d.ok ? '✅ 成功--注意及时关闭工程模式' : '❌ 失败'));
  } catch(e) {
    log('Flash 保存失败: ' + e.message);
  } finally {
    resumeSSE(1200);
  }
}

// ── 自动门限 ──────────────────────────────────────────────────
function showAutoThreshold() {
  const panel = document.getElementById('auto-th-panel');
  panel.style.display = panel.style.display === 'none' ? '' : 'none';
}

function startAutoThreshold() {
  const trig  = parseInt(document.getElementById('coeff-trig').value);
  const hold  = parseInt(document.getElementById('coeff-hold').value);
  const micro = parseInt(document.getElementById('coeff-micro').value);
  apiCmd('auto_threshold', { trig, hold, micro });
  log(`自动门限生成开始: 触发=${trig} 保持=${hold} 微动=${micro}`);
}

let progressTimer = null;
function queryProgress() {
  if (progressTimer) clearInterval(progressTimer);
  progressTimer = setInterval(async () => {
    const res = await fetch('/api/cmd', {
      method: 'POST',
      headers: { 'Content-Type': 'application/json' },
      body: JSON.stringify({ cmd: 'auto_progress' })
    });
    const d = await res.json();
    const p = d.progress || 0;
    document.getElementById('auto-progress').style.width = p + '%';
    document.getElementById('auto-progress-label').textContent = `进度：${p}%`;
    if (p >= 100) {
      clearInterval(progressTimer);
      log('自动门限生成完成！');
      fetchInfo();
    }
  }, 2000);
}

// ── 通用 API ──────────────────────────────────────────────────
async function apiCmd(cmd, extra = {}) {
  try {
    const res = await fetch('/api/cmd', {
      method: 'POST',
      headers: { 'Content-Type': 'application/json' },
      body: JSON.stringify({ cmd, ...extra })
    });
    const d = await res.json();
    log(`[${cmd}] ${d.ok ? '✓' : '✗'}`);
    return d;
  } catch(e) {
    log(`[${cmd}] 请求失败: ${e.message}`);
  }
}

// ── 获取设备信息 ──────────────────────────────────────────────
async function fetchInfo() {
  await apiCmd('read_info');
  setTimeout(async () => {
    try {
      const res = await fetch('/api/info');
      const d   = await res.json();
      document.getElementById('fw').textContent = d.fw || '-';
      document.getElementById('sn').textContent = d.sn || '-';
      engineerMode = d.engineer;
      const btn = document.getElementById('btn-eng');
      btn.classList.toggle('on', engineerMode);
      btn.textContent = engineerMode ? '⚙️ 工程模式 ON' : '⚙️ 工程模式';
      if (!engineerMode) {
        const badge = document.getElementById('presence-badge');
        badge.textContent = '未知';
        badge.className   = 'badge unknown';
        document.getElementById('dist').textContent = '- -';
      }
      if (d.max_distance) {
        document.getElementById('max-dist-slider').value = d.max_distance;
        updateDistLabel(d.max_distance);
      }
      if (d.timeout !== undefined)
        document.getElementById('timeout-input').value = d.timeout;
      if (d.motion_th && d.motion_th.length)
        d.motion_th.forEach((raw, i) => updateThumbUI('motion', i, raw));
      if (d.micro_th && d.micro_th.length)
        d.micro_th.forEach((raw, i) => updateThumbUI('micro', i, raw));
    } catch(e) { log('获取信息失败: ' + e.message); }
  }, 1500);
}

// ── 日志 ─────────────────────────────────────────────────────
function log(msg) {
  const box = document.getElementById('log-box');
  const p   = document.createElement('p');
  p.textContent = `[${new Date().toLocaleTimeString()}] ${msg}`;
  box.appendChild(p);
  box.scrollTop = box.scrollHeight;
  while (box.children.length > 100) box.removeChild(box.firstChild);
}

init();
</script>
</body>
</html>
)HTMLEOF";

const size_t LD2402_WEB_HTML_SIZE = sizeof(LD2402_WEB_HTML) - 1;

}  // namespace ld2402
}  // namespace esphome
