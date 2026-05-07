#include "infra/web/ConfigWebServer.h"

#include <ArduinoJson.h>
#include <LittleFS.h>
#include <WiFi.h>
#include <cstring>

#include "config/AppConfig.h"

namespace {
void copyCString(char* dst, size_t dstSize, const char* src) {
  if (dst == nullptr || dstSize == 0) {
    return;
  }
  if (src == nullptr) {
    dst[0] = '\0';
    return;
  }
  strncpy(dst, src, dstSize - 1);
  dst[dstSize - 1] = '\0';
}

const char kConfigPageHtml[] PROGMEM = R"HTML(
<!doctype html>
<html lang="en">
<head>
  <meta charset="utf-8">
  <meta name="viewport" content="width=device-width,initial-scale=1">
  <title>ESP32 Photo Frame Config</title>
  <style>
    :root { --bg:#0b1020; --panel:#121a30; --line:#30405f; --text:#e2e8f0; --muted:#93a5c4; --ok:#10b981; --err:#ef4444; --btn:#2563eb; }
    body { margin:0; font-family: system-ui,Segoe UI,Roboto,sans-serif; background:linear-gradient(180deg,#0b1020,#10172b); color:var(--text); }
    .wrap { max-width:980px; margin:20px auto; padding:0 12px 30px; }
    h1 { margin:10px 0 16px; font-size:22px; }
    h2 { margin-top:0; }
    .tabs { display:flex; gap:8px; margin:0 0 12px; flex-wrap:wrap; }
    .tab-btn { background:#0a1224; color:var(--muted); border:1px solid var(--line); border-radius:999px; padding:8px 14px; cursor:pointer; }
    .tab-btn.active { background:var(--btn); color:#fff; border-color:#3b82f6; }
    .tab-panel { display:none; }
    .tab-panel.active { display:block; }
    .card { background:var(--panel); border:1px solid var(--line); border-radius:10px; padding:14px; margin-bottom:14px; }
    .group-grid { display:grid; gap:12px; grid-template-columns:repeat(auto-fit,minmax(280px,1fr)); }
    .group-box { background:#0d1834; border:1px solid #324a75; border-radius:12px; padding:12px; }
    .group-box h3 { margin:0 0 10px; font-size:15px; color:#d9e7ff; }
    .group-box .row { grid-template-columns:190px 1fr; margin-bottom:8px; }
    .row { display:grid; grid-template-columns: 260px 1fr; gap:10px; align-items:center; margin-bottom:10px; }
    .row label { color:var(--muted); }
    input[type="text"], input[type="number"], input[type="password"], input[type="date"], select, textarea {
      width:100%; box-sizing:border-box; background:#0a1224; color:var(--text); border:1px solid #2a3b61; border-radius:8px; padding:9px;
    }
    textarea { min-height:200px; font-family:ui-monospace,Consolas,monospace; font-size:13px; }
    .check { display:flex; align-items:center; gap:8px; }
    button { background:var(--btn); color:#fff; border:0; border-radius:8px; padding:9px 14px; cursor:pointer; }
    .muted { color:var(--muted); font-size:13px; }
    .status { margin-top:8px; font-weight:600; }
    .ok { color:var(--ok); }
    .err { color:var(--err); }
    .alarm-list { display:flex; flex-direction:column; gap:12px; }
    .alarm-empty { border:1px dashed #3a4f77; border-radius:10px; padding:12px; color:var(--muted); }
    .alarm-card {
      background:linear-gradient(160deg,#102042,#0d1a36);
      border:1px solid #334972;
      border-radius:16px;
      padding:14px;
      box-shadow:0 8px 20px rgba(0,0,0,0.2);
    }
    .alarm-head { display:flex; justify-content:space-between; gap:12px; align-items:flex-start; flex-wrap:wrap; }
    .alarm-time-block { min-width:190px; }
    .alarm-time-row { display:flex; align-items:flex-end; gap:6px; margin-bottom:8px; }
    .alarm-time-input {
      width:72px;
      text-align:center;
      font-size:32px;
      font-weight:700;
      letter-spacing:1px;
      padding:8px 0;
      border-radius:10px;
      background:#081127;
      border:1px solid #2f4774;
      color:var(--text);
    }
    .alarm-colon { font-size:30px; color:var(--muted); line-height:1; padding-bottom:8px; }
    .alarm-label-input { font-size:15px; }
    .alarm-switches { display:flex; flex-wrap:wrap; gap:8px; }
    .alarm-switch {
      display:inline-flex;
      align-items:center;
      gap:7px;
      background:#0a152f;
      border:1px solid #2d4267;
      border-radius:999px;
      padding:7px 10px;
      color:var(--text);
      font-size:13px;
      white-space:nowrap;
    }
    .alarm-meta { margin-top:12px; display:grid; grid-template-columns:1fr; gap:10px; }
    .alarm-field label { display:block; margin-bottom:6px; color:var(--muted); font-size:13px; }
    .alarm-days-row { display:none; }
    .day-chips { display:flex; flex-wrap:wrap; gap:6px; }
    .day-chip input { display:none; }
    .day-chip span {
      width:32px;
      height:32px;
      display:inline-flex;
      align-items:center;
      justify-content:center;
      border-radius:999px;
      border:1px solid #34507e;
      background:#0a152f;
      color:var(--muted);
      font-weight:600;
      cursor:pointer;
      user-select:none;
    }
    .day-chip input:checked + span {
      background:#2563eb;
      border-color:#3b82f6;
      color:#fff;
    }
    .alarm-date-row { display:none; }
    .alarm-actions { margin-top:12px; display:flex; justify-content:space-between; align-items:center; gap:8px; }
    .alarm-id { color:var(--muted); font-size:12px; }
    .alarm-remove {
      background:#7f1d1d;
      border:1px solid #ef4444;
      border-radius:8px;
      color:#fff;
      padding:8px 10px;
      cursor:pointer;
    }
    @media (max-width:760px){ .row{grid-template-columns:1fr;} .group-box .row{grid-template-columns:1fr;} }
  </style>
</head>
<body>
<div class="wrap">
  <h1>ESP32 Photo Frame Config</h1>

  <div class="tabs">
    <button type="button" class="tab-btn active" data-tab="frame">Frame Setup</button>
    <button type="button" class="tab-btn" data-tab="alarm">Alarm</button>
    <button type="button" class="tab-btn" data-tab="agenda">Agenda</button>
    <button type="button" class="tab-btn" data-tab="weather">Weather</button>
    <button type="button" class="tab-btn" data-tab="photo">Photo URLs</button>
    <button type="button" class="tab-btn" data-tab="github">GitHub Auth</button>
  </div>

  <div id="tab-frame" class="tab-panel active">
    <div class="card">
      <h2>Photo Frame Setup</h2>
      <div class="group-grid">
        <div class="group-box">
          <h3>Screen Timing (seconds)</h3>
          <div class="row"><label>Auto Timeout Enabled</label><div class="check"><input id="auto_timeout_enabled" type="checkbox"></div></div>
          <div class="row"><label>Auto Timeout (s)</label><input id="auto_timeout_sec" type="number" min="1"></div>
        </div>
        <div class="group-box">
          <h3>Photo Rotation</h3>
          <div class="row"><label>Auto Cycle Enabled</label><div class="check"><input id="auto_cycle_enabled" type="checkbox"></div></div>
          <div class="row"><label>Auto Cycle Photo Count</label><input id="auto_cycle_photo_count" type="number" min="1"></div>
          <div class="row"><label>Auto Cycle Duration (s)</label><input id="auto_cycle_duration_sec" type="number" min="1"></div>
          <div class="row"><label>Photo Refresh (s)</label><input id="photo_refresh_sec" type="number" min="1"></div>
        </div>
        <div class="group-box">
          <h3>Display</h3>
          <div class="row">
            <label>Photo Mode</label>
            <select id="photo_fill_mode">
              <option value="1">Fill (6:7 crop)</option>
              <option value="0">Fit (show full image)</option>
            </select>
          </div>
          <div class="row">
            <label>Display Transform</label>
            <select id="display_transform">
              <option value="0">Normal</option>
              <option value="1">Mirror X</option>
              <option value="2">Mirror Y</option>
              <option value="3">Rotate 180</option>
            </select>
          </div>
        </div>
      </div>
      <button onclick="saveSettings()">Save Frame Settings</button>
      <div id="status_frame" class="status"></div>
    </div>
  </div>

  <div id="tab-alarm" class="tab-panel">
    <div class="card">
      <h2>Alarm Settings</h2>
      <div class="row">
        <label>Alarm Rules</label>
        <div>
          <div id="alarms_box" class="alarm-list"></div>
          <div id="alarms_limit" class="muted"></div>
          <button type="button" onclick="addAlarmRow()">Add Alarm</button>
        </div>
      </div>
      <button onclick="saveSettings()">Save Alarm Settings</button>
      <div id="status_alarm" class="status"></div>
    </div>
  </div>

  <div id="tab-agenda" class="tab-panel">
    <div class="card">
      <h2>Agenda & VALARM</h2>
      <div class="row"><label>Agenda URL</label><input id="agenda_url" type="text"></div>
      <div class="row"><label>VALARM Enabled</label><div class="check"><input id="agenda_event_alarm_enabled" type="checkbox"></div></div>
      <div class="row"><label>Reminder Minutes Before Event</label><input id="agenda_event_lead_minutes" type="number" min="0" max="10080"></div>
      <div class="row"><label>VALARM Sound</label><div class="check"><input id="agenda_event_sound" type="checkbox"></div></div>
      <div class="row"><label></label><div class="muted">VALARM is checked once per minute for efficiency.</div></div>
      <button onclick="saveSettings()">Save Agenda Settings</button>
      <div id="status_agenda" class="status"></div>
    </div>
  </div>

  <div id="tab-weather" class="tab-panel">
    <div class="card">
      <h2>Weather</h2>
      <div class="row"><label>Weather Label</label><input id="weather_label" type="text"></div>
      <div class="row"><label>Weather Latitude</label><input id="weather_latitude" type="number" step="0.0001" min="-90" max="90"></div>
      <div class="row"><label>Weather Longitude</label><input id="weather_longitude" type="number" step="0.0001" min="-180" max="180"></div>
      <button onclick="saveSettings()">Save Weather Settings</button>
      <div id="status_weather" class="status"></div>
    </div>
  </div>

  <div id="tab-photo" class="tab-panel">
    <div class="card">
      <h2>Photo URLs JSON</h2>
      <p class="muted">Edit full JSON from <code>/photo_urls.json</code>.</p>
      <textarea id="photo_urls_json"></textarea>
      <button onclick="savePhotoUrls()">Save Photo URLs</button>
      <div id="status_photo" class="status"></div>
    </div>
  </div>

  <div id="tab-github" class="tab-panel">
    <div class="card">
      <h2>GitHub Auth JSON</h2>
      <p class="muted">Token is write-only for safety. Leave blank to keep existing token.</p>
      <div class="row"><label>User Agent</label><input id="github_user_agent" type="text"></div>
      <div class="row"><label>New GitHub Token</label><input id="github_token" type="password" autocomplete="new-password"></div>
      <button onclick="saveGithubAuth()">Save GitHub Auth</button>
      <div id="status_github" class="status"></div>
    </div>
  </div>
</div>

<script>
  let alarmRows = [];
  let maxAlarms = 8;

  function setActiveTab(tabKey){
    document.querySelectorAll('.tab-btn').forEach(btn => {
      btn.classList.toggle('active', btn.dataset.tab === tabKey);
    });
    document.querySelectorAll('.tab-panel').forEach(panel => {
      panel.classList.toggle('active', panel.id === `tab-${tabKey}`);
    });
  }

  function setStatus(id, ok, text){
    const el = document.getElementById(id);
    if (!el) return;
    el.className = 'status ' + (ok ? 'ok' : 'err');
    el.textContent = text;
  }

  function setSettingsStatus(ok, text){
    setStatus('status_frame', ok, text);
    setStatus('status_alarm', ok, text);
    setStatus('status_agenda', ok, text);
    setStatus('status_weather', ok, text);
  }

  const dayLabels = ['Sun','Mon','Tue','Wed','Thu','Fri','Sat'];
  function escapeHtml(value){
    return String(value ?? '')
      .replace(/&/g, '&amp;')
      .replace(/</g, '&lt;')
      .replace(/>/g, '&gt;')
      .replace(/"/g, '&quot;')
      .replace(/'/g, '&#39;');
  }

  function msToSec(ms, fallbackSec){
    const n = Number(ms);
    if (!Number.isFinite(n) || n <= 0) return fallbackSec;
    return Math.max(1, Math.round(n / 1000));
  }

  function secToMs(sec, fallbackMs){
    const n = Number(sec);
    if (!Number.isFinite(n) || n <= 0) return fallbackMs;
    return Math.max(1000, Math.round(n * 1000));
  }

  function formatDateInput(year, month, day){
    const y = Number(year || 0);
    const m = Number(month || 0);
    const d = Number(day || 0);
    if (y < 2020 || y > 2099 || m < 1 || m > 12 || d < 1 || d > 31) return '';
    const mm = String(m).padStart(2, '0');
    const dd = String(d).padStart(2, '0');
    return `${y}-${mm}-${dd}`;
  }

  function parseDateInput(value){
    const txt = String(value || '');
    const m = txt.match(/^(\d{4})-(\d{2})-(\d{2})$/);
    if (!m) return { year:0, month:0, day:0 };
    const year = Number(m[1]);
    const month = Number(m[2]);
    const day = Number(m[3]);
    if (year < 2020 || year > 2099 || month < 1 || month > 12 || day < 1 || day > 31) {
      return { year:0, month:0, day:0 };
    }
    return { year, month, day };
  }

  function todayDateInputValue(){
    const now = new Date();
    const y = now.getFullYear();
    const m = String(now.getMonth() + 1).padStart(2, '0');
    const d = String(now.getDate()).padStart(2, '0');
    return `${y}-${m}-${d}`;
  }

  function makeDayChip(mask, idx){
    const checked = ((Number(mask) & (1 << idx)) !== 0) ? 'checked' : '';
    const letter = dayLabels[idx].slice(0, 1);
    return `<label class="day-chip"><input type="checkbox" data-day="${idx}" ${checked}><span>${letter}</span></label>`;
  }

  function refreshAlarmCardUi(card){
    if (!card) return;
    const repeatMode = Number(card.querySelector('.alarm-repeat')?.value ?? 0);
    const daysRow = card.querySelector('.alarm-days-row');
    const dateRow = card.querySelector('.alarm-date-row');
    if (daysRow) daysRow.style.display = (repeatMode === 1) ? 'block' : 'none';
    if (dateRow) dateRow.style.display = (repeatMode === 2) ? 'block' : 'none';
    if (repeatMode === 2) {
      const dateInput = card.querySelector('.alarm-date');
      if (dateInput && !dateInput.value) {
        dateInput.value = todayDateInputValue();
      }
    }
  }

  function renderAlarmRows(alarms){
    alarmRows = Array.isArray(alarms) ? alarms.slice() : [];
    if (alarmRows.length > maxAlarms) {
      alarmRows = alarmRows.slice(0, maxAlarms);
    }
    const box = document.getElementById('alarms_box');
    box.innerHTML = '';
    document.getElementById('alarms_limit').textContent = `Maximum alarms: ${maxAlarms}`;
    if (alarmRows.length === 0) {
      box.innerHTML = '<div class="alarm-empty">No alarms configured.</div>';
      return;
    }
    alarmRows.forEach((a, idx) => {
      const repeatMode = Math.max(0, Math.min(2, Number(a.repeat_mode ?? 0)));
      let repeatDays = Number(a.repeat_days_mask ?? 0x7F) & 0x7F;
      if (repeatMode === 1 && repeatDays === 0) repeatDays = 0x3E;
      const dateValue = formatDateInput(a.one_time_year, a.one_time_month, a.one_time_day);
      const row = document.createElement('div');
      row.className = 'alarm-card';
      row.innerHTML = `
        <div class="alarm-head">
          <div class="alarm-time-block">
            <div class="alarm-time-row">
              <input class="alarm-time-input alarm-hour" type="number" min="0" max="23" value="${Math.max(0, Math.min(23, Number(a.hour ?? 7)))}">
              <span class="alarm-colon">:</span>
              <input class="alarm-time-input alarm-minute" type="number" min="0" max="59" value="${Math.max(0, Math.min(59, Number(a.minute ?? 0)))}">
            </div>
            <input class="alarm-label-input alarm-label" type="text" value="${escapeHtml(a.label ?? '')}" placeholder="Wake up">
          </div>
          <div class="alarm-switches">
            <label class="alarm-switch"><input class="alarm-enabled" type="checkbox" ${a.enabled ? 'checked' : ''}> Enabled</label>
            <label class="alarm-switch"><input class="alarm-sound" type="checkbox" ${a.sound_enabled ? 'checked' : ''}> Sound</label>
          </div>
        </div>
        <div class="alarm-meta">
          <div class="alarm-field">
            <label>Repeat</label>
            <select class="alarm-repeat">
              <option value="0" ${repeatMode === 0 ? 'selected' : ''}>Every day</option>
              <option value="1" ${repeatMode === 1 ? 'selected' : ''}>Custom days</option>
              <option value="2" ${repeatMode === 2 ? 'selected' : ''}>One-time</option>
            </select>
          </div>
          <div class="alarm-field alarm-days-row">
            <label>Days</label>
            <div class="day-chips">
              ${makeDayChip(repeatDays, 0)}
              ${makeDayChip(repeatDays, 1)}
              ${makeDayChip(repeatDays, 2)}
              ${makeDayChip(repeatDays, 3)}
              ${makeDayChip(repeatDays, 4)}
              ${makeDayChip(repeatDays, 5)}
              ${makeDayChip(repeatDays, 6)}
            </div>
          </div>
          <div class="alarm-field alarm-date-row">
            <label>Date</label>
            <input class="alarm-date" type="date" value="${dateValue}">
          </div>
        </div>
        <div class="alarm-actions">
          <div class="alarm-id">Alarm ${idx + 1}</div>
          <button type="button" class="alarm-remove">Remove</button>
        </div>
      `;
      const removeBtn = row.querySelector('.alarm-remove');
      removeBtn.onclick = () => {
        alarmRows.splice(idx, 1);
        renderAlarmRows(alarmRows);
      };
      const repeatSelect = row.querySelector('.alarm-repeat');
      repeatSelect.addEventListener('change', () => refreshAlarmCardUi(row));
      refreshAlarmCardUi(row);
      box.appendChild(row);
    });
  }

  function addAlarmRow(){
    if (alarmRows.length >= maxAlarms) {
      setStatus('status_alarm', false, `Maximum ${maxAlarms} alarms reached`);
      return;
    }
    alarmRows.push({
      enabled:true,
      sound_enabled:true,
      hour:7,
      minute:0,
      repeat_mode:0,
      repeat_days_mask:0x7F,
      one_time_year:0,
      one_time_month:0,
      one_time_day:0,
      label:`Alarm ${alarmRows.length + 1}`
    });
    renderAlarmRows(alarmRows);
  }

  function collectAlarms(){
    const rows = Array.from(document.querySelectorAll('#alarms_box .alarm-card'));
    const out = [];
    for (const r of rows){
      const label = (r.querySelector('.alarm-label')?.value || '').trim();
      const hour = Math.max(0, Math.min(23, Number(r.querySelector('.alarm-hour')?.value || 0)));
      const minute = Math.max(0, Math.min(59, Number(r.querySelector('.alarm-minute')?.value || 0)));
      const repeat_mode = Math.max(0, Math.min(2, Number(r.querySelector('.alarm-repeat')?.value || 0)));
      let repeat_days_mask = 0;
      r.querySelectorAll('.day-chip input[type="checkbox"]').forEach(chk => {
        const day = Number(chk.dataset.day ?? -1);
        if (chk.checked && day >= 0 && day <= 6) {
          repeat_days_mask |= (1 << day);
        }
      });
      if (repeat_mode === 0) repeat_days_mask = 0x7F;
      if (repeat_mode === 1 && repeat_days_mask === 0) repeat_days_mask = 0x3E;
      let oneTime = parseDateInput(r.querySelector('.alarm-date')?.value || '');
      if (repeat_mode === 2 && oneTime.year === 0) {
        oneTime = parseDateInput(todayDateInputValue());
      }
      if (repeat_mode !== 2) {
        oneTime = { year:0, month:0, day:0 };
      }
      const sound_enabled = !!r.querySelector('.alarm-sound')?.checked;
      const enabled = !!r.querySelector('.alarm-enabled')?.checked;
      if (label.length === 0) continue;
      out.push({
        label,
        hour,
        minute,
        enabled,
        sound_enabled,
        repeat_mode,
        repeat_days_mask,
        one_time_year: oneTime.year,
        one_time_month: oneTime.month,
        one_time_day: oneTime.day
      });
    }
    return out;
  }

  async function loadData(){
    try {
      const c = await fetch('/api/config');
      const cfg = await c.json();
      document.getElementById('auto_timeout_enabled').checked = !!cfg.auto_timeout_enabled;
      document.getElementById('auto_timeout_sec').value = msToSec(cfg.auto_timeout_ms ?? 30000, 30);
      document.getElementById('auto_cycle_enabled').checked = !!cfg.auto_cycle_enabled;
      document.getElementById('auto_cycle_photo_count').value = cfg.auto_cycle_photo_count ?? 2;
      document.getElementById('auto_cycle_duration_sec').value = msToSec(cfg.auto_cycle_duration_ms ?? 30000, 30);
      document.getElementById('photo_refresh_sec').value = msToSec(cfg.photo_refresh_ms ?? 10000, 10);
      document.getElementById('photo_fill_mode').value = (cfg.photo_fill_mode === false || cfg.photo_fill_mode === 0) ? '0' : '1';
      document.getElementById('display_transform').value = String(cfg.display_transform ?? 0);
      document.getElementById('weather_label').value = cfg.weather_label ?? 'Hamburg';
      document.getElementById('weather_latitude').value = cfg.weather_latitude ?? 0;
      document.getElementById('weather_longitude').value = cfg.weather_longitude ?? 0;
      document.getElementById('agenda_url').value = cfg.agenda_url ?? '';
      document.getElementById('agenda_event_alarm_enabled').checked = !!cfg.agenda_event_alarm_enabled;
      document.getElementById('agenda_event_sound').checked = !!cfg.agenda_event_sound;
      document.getElementById('agenda_event_lead_minutes').value = Number(cfg.agenda_event_lead_minutes ?? 0);
      maxAlarms = Number(cfg.max_alarms ?? 8);
      if (!Number.isFinite(maxAlarms) || maxAlarms < 1) maxAlarms = 8;
      renderAlarmRows(cfg.alarms ?? []);
      document.getElementById('github_user_agent').value = cfg.github_user_agent ?? 'esp32-photo-frame';

      const p = await fetch('/api/photo_urls');
      const txt = await p.text();
      document.getElementById('photo_urls_json').value = txt;
    } catch (e) {
      setSettingsStatus(false, 'Load failed: ' + e);
    }
  }

  async function saveSettings(){
    const payload = {
      auto_timeout_enabled: document.getElementById('auto_timeout_enabled').checked,
      auto_timeout_ms: secToMs(document.getElementById('auto_timeout_sec').value, 30000),
      auto_cycle_enabled: document.getElementById('auto_cycle_enabled').checked,
      auto_cycle_photo_count: Number(document.getElementById('auto_cycle_photo_count').value),
      auto_cycle_duration_ms: secToMs(document.getElementById('auto_cycle_duration_sec').value, 30000),
      photo_refresh_ms: secToMs(document.getElementById('photo_refresh_sec').value, 10000),
      photo_fill_mode: document.getElementById('photo_fill_mode').value === '1',
      display_transform: Number(document.getElementById('display_transform').value),
      weather_label: document.getElementById('weather_label').value,
      weather_latitude: Number(document.getElementById('weather_latitude').value),
      weather_longitude: Number(document.getElementById('weather_longitude').value),
      agenda_url: document.getElementById('agenda_url').value,
      agenda_event_alarm_enabled: document.getElementById('agenda_event_alarm_enabled').checked,
      agenda_event_sound: document.getElementById('agenda_event_sound').checked,
      agenda_event_lead_minutes: Number(document.getElementById('agenda_event_lead_minutes').value),
      alarms: collectAlarms()
    };
    try {
      const r = await fetch('/api/settings', { method:'POST', headers:{'Content-Type':'application/json'}, body: JSON.stringify(payload) });
      const j = await r.json();
      setSettingsStatus(r.ok, j.message || (r.ok ? 'Saved' : 'Failed'));
    } catch (e) {
      setSettingsStatus(false, 'Save failed: ' + e);
    }
  }

  async function savePhotoUrls(){
    setActiveTab('photo');
    try {
      const body = document.getElementById('photo_urls_json').value;
      const r = await fetch('/api/photo_urls', { method:'POST', headers:{'Content-Type':'application/json'}, body });
      const j = await r.json();
      setStatus('status_photo', r.ok, j.message || (r.ok ? 'Saved' : 'Failed'));
    } catch (e) {
      setStatus('status_photo', false, 'Save failed: ' + e);
    }
  }

  async function saveGithubAuth(){
    setActiveTab('github');
    const payload = {
      user_agent: document.getElementById('github_user_agent').value,
      github_token: document.getElementById('github_token').value
    };
    try {
      const r = await fetch('/api/github_auth', { method:'POST', headers:{'Content-Type':'application/json'}, body: JSON.stringify(payload) });
      const j = await r.json();
      setStatus('status_github', r.ok, j.message || (r.ok ? 'Saved' : 'Failed'));
      if (r.ok) {
        document.getElementById('github_token').value = '';
      }
    } catch (e) {
      setStatus('status_github', false, 'Save failed: ' + e);
    }
  }

  document.querySelectorAll('.tab-btn').forEach(btn => {
    btn.addEventListener('click', () => setActiveTab(btn.dataset.tab || 'frame'));
  });
  setActiveTab('frame');
  loadData();
</script>
</body>
</html>
)HTML";
}  // namespace

ConfigWebServer::ConfigWebServer()
    : server_(80),
      started_(false),
      configChangedCb_(nullptr),
      configChangedCtx_(nullptr) {}

void ConfigWebServer::setConfigChangedCallback(void (*cb)(void*), void* ctx) {
  configChangedCb_ = cb;
  configChangedCtx_ = ctx;
}

bool ConfigWebServer::ensureFsMounted() {
  return LittleFS.begin(false);
}

bool ConfigWebServer::readFile(const char* path, String* out) {
  if (out == nullptr || path == nullptr) {
    return false;
  }
  if (!ensureFsMounted()) {
    return false;
  }
  File f = LittleFS.open(path, "r");
  if (!f) {
    return false;
  }
  *out = f.readString();
  f.close();
  return true;
}

bool ConfigWebServer::writeFileAtomic(const char* path, const String& content) {
  if (path == nullptr) {
    return false;
  }
  if (!ensureFsMounted()) {
    return false;
  }

  String tmpPath(path);
  tmpPath += ".tmp";
  LittleFS.remove(tmpPath);

  File tmp = LittleFS.open(tmpPath, "w");
  if (!tmp) {
    return false;
  }
  if (tmp.print(content) == 0) {
    tmp.close();
    LittleFS.remove(tmpPath);
    return false;
  }
  tmp.close();

  LittleFS.remove(path);
  if (!LittleFS.rename(tmpPath, path)) {
    LittleFS.remove(tmpPath);
    return false;
  }
  return true;
}

void ConfigWebServer::notifyConfigChanged() const {
  if (configChangedCb_ != nullptr) {
    configChangedCb_(configChangedCtx_);
  }
}

void ConfigWebServer::setupRoutes() {
  server_.on("/", HTTP_GET, [this]() { handleRoot(); });
  server_.on("/api/config", HTTP_GET, [this]() { handleGetConfig(); });
  server_.on("/api/photo_urls", HTTP_GET, [this]() { handleGetPhotoUrls(); });
  server_.on("/api/settings", HTTP_POST, [this]() { handlePostSettings(); });
  server_.on("/api/photo_urls", HTTP_POST, [this]() { handlePostPhotoUrls(); });
  server_.on("/api/github_auth", HTTP_POST, [this]() { handlePostGithubAuth(); });
  server_.onNotFound([this]() { server_.send(404, "application/json", "{\"message\":\"Not found\"}"); });
}

void ConfigWebServer::begin() {
  if (started_) {
    return;
  }
  if (!ensureFsMounted()) {
    Serial.println("WebConfig: LittleFS mount failed");
    return;
  }
  setupRoutes();
  server_.begin();
  started_ = true;
  Serial.print("WebConfig: http://");
  Serial.println(WiFi.localIP());
}

void ConfigWebServer::loop() {
  if (!started_) {
    return;
  }
  server_.handleClient();
}

void ConfigWebServer::handleRoot() {
  server_.send_P(200, "text/html", kConfigPageHtml);
}

void ConfigWebServer::handleGetConfig() {
  String authRaw;
  String authUserAgent = "esp32-photo-frame";
  bool tokenConfigured = false;
  if (readFile(Config::kGithubAuthJsonPath, &authRaw)) {
    DynamicJsonDocument authDoc(1024);
    if (deserializeJson(authDoc, authRaw) == DeserializationError::Ok) {
      const char* ua = authDoc["user_agent"] | "esp32-photo-frame";
      authUserAgent = ua;
      const char* tok = authDoc["github_token"] | "";
      tokenConfigured = (tok != nullptr && tok[0] != '\0');
    }
  }

  DynamicJsonDocument doc(4096);
  doc["auto_timeout_enabled"] = Config::gAutoViewSettings.infoPageAutoTimeoutEnabled;
  doc["auto_timeout_ms"] = Config::gAutoViewSettings.infoPageAutoTimeoutMs;
  doc["auto_cycle_enabled"] = Config::gAutoViewSettings.infoPageAutoCycleEnabled;
  doc["auto_cycle_photo_count"] = Config::gAutoViewSettings.infoPageAutoCyclePhotoCount;
  doc["auto_cycle_duration_ms"] = Config::gAutoViewSettings.infoPageAutoCycleDurationMs;
  doc["photo_refresh_ms"] = Config::gAutoViewSettings.photoRefreshIntervalMs;
  doc["photo_fill_mode"] = Config::gAutoViewSettings.photoFillMode;
  doc["display_transform"] = Config::gDisplayTransform;
  doc["weather_label"] = Config::gWeatherLocationLabel;
  doc["weather_latitude"] = Config::gWeatherLatitude;
  doc["weather_longitude"] = Config::gWeatherLongitude;
  doc["agenda_url"] = Config::gAgendaScriptUrl;
  doc["agenda_event_alarm_enabled"] = Config::gAgendaEventAlarmEnabled;
  doc["agenda_event_sound"] = Config::gAgendaEventSoundEnabled;
  doc["agenda_event_lead_minutes"] = Config::gAgendaEventLeadMinutes;
  doc["max_alarms"] = Config::kMaxAlarms;
  JsonArray alarms = doc.createNestedArray("alarms");
  for (uint8_t i = 0; i < Config::gAlarmCount; ++i) {
    JsonObject alarm = alarms.createNestedObject();
    alarm["enabled"] = Config::gAlarms[i].enabled;
    alarm["sound_enabled"] = Config::gAlarms[i].soundEnabled;
    alarm["hour"] = Config::gAlarms[i].hour;
    alarm["minute"] = Config::gAlarms[i].minute;
    alarm["repeat_mode"] = Config::gAlarms[i].repeatMode;
    alarm["repeat_days_mask"] = Config::gAlarms[i].repeatDaysMask;
    alarm["one_time_year"] = Config::gAlarms[i].oneTimeYear;
    alarm["one_time_month"] = Config::gAlarms[i].oneTimeMonth;
    alarm["one_time_day"] = Config::gAlarms[i].oneTimeDay;
    alarm["label"] = Config::gAlarms[i].label;
  }
  doc["github_user_agent"] = authUserAgent;
  doc["github_token_configured"] = tokenConfigured;

  String out;
  serializeJson(doc, out);
  server_.send(200, "application/json", out);
}

void ConfigWebServer::handleGetPhotoUrls() {
  String raw;
  if (!readFile(Config::kPhotoUrlsJsonPath, &raw)) {
    server_.send(404, "text/plain", "[]");
    return;
  }
  server_.send(200, "application/json", raw);
}

void ConfigWebServer::handlePostSettings() {
  if (!server_.hasArg("plain")) {
    server_.send(400, "application/json", "{\"message\":\"Missing body\"}");
    return;
  }

  DynamicJsonDocument doc(6144);
  const DeserializationError err = deserializeJson(doc, server_.arg("plain"));
  if (err) {
    server_.send(400, "application/json", "{\"message\":\"Invalid JSON\"}");
    return;
  }

  Config::gAutoViewSettings.infoPageAutoTimeoutEnabled =
      doc["auto_timeout_enabled"] | Config::gAutoViewSettings.infoPageAutoTimeoutEnabled;
  Config::gAutoViewSettings.infoPageAutoTimeoutMs =
      doc["auto_timeout_ms"] | Config::gAutoViewSettings.infoPageAutoTimeoutMs;
  Config::gAutoViewSettings.infoPageAutoCycleEnabled =
      doc["auto_cycle_enabled"] | Config::gAutoViewSettings.infoPageAutoCycleEnabled;
  Config::gAutoViewSettings.infoPageAutoCyclePhotoCount =
      doc["auto_cycle_photo_count"] | Config::gAutoViewSettings.infoPageAutoCyclePhotoCount;
  Config::gAutoViewSettings.infoPageAutoCycleDurationMs =
      doc["auto_cycle_duration_ms"] | Config::gAutoViewSettings.infoPageAutoCycleDurationMs;
  Config::gAutoViewSettings.photoRefreshIntervalMs =
      doc["photo_refresh_ms"] | Config::gAutoViewSettings.photoRefreshIntervalMs;
  Config::gAutoViewSettings.photoFillMode =
      doc["photo_fill_mode"] | Config::gAutoViewSettings.photoFillMode;
  Config::gDisplayTransform = doc["display_transform"] | Config::gDisplayTransform;
  copyCString(Config::gWeatherLocationLabel, sizeof(Config::gWeatherLocationLabel),
              doc["weather_label"] | Config::gWeatherLocationLabel);
  Config::gWeatherLatitude = doc["weather_latitude"] | Config::gWeatherLatitude;
  Config::gWeatherLongitude = doc["weather_longitude"] | Config::gWeatherLongitude;
  if (Config::gDisplayTransform > Config::kDisplayTransformMax) {
    Config::gDisplayTransform = Config::kDefaultDisplayTransform;
  }

  copyCString(Config::gAgendaScriptUrl, sizeof(Config::gAgendaScriptUrl),
              doc["agenda_url"] | Config::gAgendaScriptUrl);
  Config::gAgendaEventAlarmEnabled =
      doc["agenda_event_alarm_enabled"] | Config::gAgendaEventAlarmEnabled;
  Config::gAgendaEventSoundEnabled = doc["agenda_event_sound"] | Config::gAgendaEventSoundEnabled;
  Config::gAgendaEventLeadMinutes = doc["agenda_event_lead_minutes"] | Config::gAgendaEventLeadMinutes;

  if (doc["alarms"].is<JsonArray>()) {
    JsonArray alarms = doc["alarms"].as<JsonArray>();
    Config::gAlarmCount = 0;
    for (JsonObject alarm : alarms) {
      if (Config::gAlarmCount >= Config::kMaxAlarms) {
        break;
      }
      Config::AlarmEntry& dst = Config::gAlarms[Config::gAlarmCount++];
      dst.enabled = alarm["enabled"] | false;
      dst.soundEnabled = alarm["sound_enabled"] | false;
      dst.hour = alarm["hour"] | 7;
      dst.minute = alarm["minute"] | 0;
      dst.repeatMode = alarm["repeat_mode"] | static_cast<uint8_t>(Config::AlarmRepeatMode::kDaily);
      dst.repeatDaysMask = alarm["repeat_days_mask"] | Config::kWeekdayMaskAll;
      dst.oneTimeYear = alarm["one_time_year"] | 0;
      dst.oneTimeMonth = alarm["one_time_month"] | 0;
      dst.oneTimeDay = alarm["one_time_day"] | 0;
      copyCString(dst.label, sizeof(dst.label), alarm["label"] | "");
    }
  }

  if (!Config::saveAutoViewSettings()) {
    server_.send(500, "application/json", "{\"message\":\"Failed to save settings\"}");
    return;
  }

  notifyConfigChanged();
  server_.send(200, "application/json", "{\"message\":\"Settings saved\"}");
}

void ConfigWebServer::handlePostPhotoUrls() {
  if (!server_.hasArg("plain")) {
    server_.send(400, "application/json", "{\"message\":\"Missing body\"}");
    return;
  }

  DynamicJsonDocument doc(16384);
  const DeserializationError err = deserializeJson(doc, server_.arg("plain"));
  if (err) {
    server_.send(400, "application/json", "{\"message\":\"Invalid photo_urls JSON\"}");
    return;
  }

  String pretty;
  serializeJsonPretty(doc, pretty);
  pretty += '\n';
  if (!writeFileAtomic(Config::kPhotoUrlsJsonPath, pretty)) {
    server_.send(500, "application/json", "{\"message\":\"Failed to save photo_urls.json\"}");
    return;
  }

  notifyConfigChanged();
  server_.send(200, "application/json", "{\"message\":\"Photo URLs saved\"}");
}

void ConfigWebServer::handlePostGithubAuth() {
  if (!server_.hasArg("plain")) {
    server_.send(400, "application/json", "{\"message\":\"Missing body\"}");
    return;
  }

  DynamicJsonDocument req(2048);
  const DeserializationError reqErr = deserializeJson(req, server_.arg("plain"));
  if (reqErr) {
    server_.send(400, "application/json", "{\"message\":\"Invalid JSON\"}");
    return;
  }

  String existingToken;
  String existingUserAgent("esp32-photo-frame");
  {
    String existingRaw;
    if (readFile(Config::kGithubAuthJsonPath, &existingRaw)) {
      DynamicJsonDocument oldDoc(1024);
      if (deserializeJson(oldDoc, existingRaw) == DeserializationError::Ok) {
        existingToken = String(oldDoc["github_token"] | "");
        existingUserAgent = String(oldDoc["user_agent"] | "esp32-photo-frame");
      }
    }
  }

  const char* postedUa = req["user_agent"] | existingUserAgent.c_str();
  String outUserAgent = String(postedUa);
  outUserAgent.trim();
  if (outUserAgent.isEmpty()) {
    outUserAgent = "esp32-photo-frame";
  }
  if (outUserAgent.length() >= Config::kMaxUserAgentLength) {
    outUserAgent.remove(Config::kMaxUserAgentLength - 1);
  }

  String outToken = existingToken;
  if (req["github_token"].is<const char*>()) {
    String postedToken = String(req["github_token"].as<const char*>());
    postedToken.trim();
    if (!postedToken.isEmpty()) {
      if (postedToken.length() >= Config::kMaxGithubTokenLength) {
        postedToken.remove(Config::kMaxGithubTokenLength - 1);
      }
      outToken = postedToken;
    }
  }

  DynamicJsonDocument outDoc(1024);
  outDoc["github_token"] = outToken;
  outDoc["user_agent"] = outUserAgent;
  String pretty;
  serializeJsonPretty(outDoc, pretty);
  pretty += '\n';

  if (!writeFileAtomic(Config::kGithubAuthJsonPath, pretty)) {
    server_.send(500, "application/json", "{\"message\":\"Failed to save github_auth.json\"}");
    return;
  }

  notifyConfigChanged();
  server_.send(200, "application/json", "{\"message\":\"GitHub auth saved\"}");
}
