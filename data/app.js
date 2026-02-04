// data/app.js
(function () {
  const $ = (id) => document.getElementById(id);

  const state = {
    adminToken: localStorage.getItem('wss_admin_token') || '',
    setupRequired: true,
    lastStep: 'welcome',
    adminMode: 'off',
    adminActive: false,
    adminRemainingS: 0,
  };
  const APP_PAGE = (window.APP_PAGE || 'main');
  const isSetupPage = APP_PAGE === 'setup';

  function on(el, evt, handler) {
    if (el) el.addEventListener(evt, handler);
  }

  function setDisabled(el, disabled) {
    if (!el) return;
    el.disabled = !!disabled;
  }

  function hdrs() {
    const h = {
      'Content-Type': 'application/json',
    };
    if (state.adminToken) h['X-Admin-Token'] = state.adminToken;
    return h;
  }

  function authHeader() {
    const h = {};
    if (state.adminToken) h['X-Admin-Token'] = state.adminToken;
    return h;
  }

  async function jget(url) {
    const r = await fetch(url, { cache: 'no-store', headers: hdrs() });
    const t = await r.text();
    let j = null;
    try { j = JSON.parse(t); } catch (e) { j = { raw: t }; }
    return { ok: r.ok, status: r.status, json: j };
  }

  async function jpost(url, obj) {
    const r = await fetch(url, { method: 'POST', headers: hdrs(), body: JSON.stringify(obj || {}) });
    const t = await r.text();
    let j = null;
    try { j = JSON.parse(t); } catch (e) { j = { raw: t }; }
    return { ok: r.ok, status: r.status, json: j };
  }

  function setHidden(el, hidden) {
    if (!el) return;
    el.classList.toggle('hidden', !!hidden);
  }

  function setText(id, txt) {
    const el = $(id);
    if (el) el.textContent = txt;
  }

  function asUnknown(v) {
    if (v === undefined || v === null) return 'Unknown';
    if (typeof v === 'string' && (v.trim() === '' || v === 'u')) return 'Unknown';
    return v;
  }

  function adminModeText() {
    if (state.adminMode === 'eligible') {
      return `Admin: Eligible (${state.adminRemainingS}s)`;
    }
    if (state.adminMode === 'authenticated') {
      return `Admin: Authenticated (${state.adminRemainingS}s)`;
    }
    return 'Admin: Off';
  }

  function nfcHealthText(nfc) {
    if (!nfc) return 'Unknown';
    const health = nfc.health || '';
    let label = 'Degraded';
    if (health === 'ok') label = 'OK';
    else if (health === 'unavailable' || health === 'disabled_cfg' || health === 'disabled_build') label = 'Unavailable';
    const prov = (nfc.provisioning_active === true) ? 'Yes' : (nfc.provisioning_active === false ? 'No' : 'Unknown');
    return `${label} (Provisioning enabled: ${prov})`;
  }

  function storageStatusText(storage) {
    if (!storage) return 'Unknown';
    const status = storage.status || storage.sd_status || '';
    if (status === 'OK') return 'SD OK';
    if (storage.fallback_active) return 'SD Missing (Using Flash Fallback)';
    if (status.length) return `SD ${status}`;
    return 'Unknown';
  }

  function networkText(j) {
    const mode = j.wifi_mode || '';
    const ip = j.ip || '';
    if (!mode && !ip) return 'Unknown';
    let out = mode.length ? mode : 'Unknown';
    if (mode === 'STA' && j.wifi_ssid) out += ` (${j.wifi_ssid})`;
    if (ip.length) out += ` — ${ip}`;
    return out;
  }

  async function refreshConfig() {
    if (!$('configJson')) return;
    if (!state.adminActive) {
      setText('configHint', 'Config is available in Admin mode.');
      setText('configJson', '');
      return;
    }
    const r = await jget('/api/config');
    if (!r.ok) {
      setText('configHint', 'Config unavailable.');
      setText('configJson', '');
      return;
    }
    setText('configHint', '');
    try {
      setText('configJson', JSON.stringify(r.json, null, 2));
    } catch (e) {
      setText('configJson', '');
    }
  }

  function renderWizardFields(step) {
    const host = $('wizFields');
    host.innerHTML = '';

    function addLabel(text) {
      const p = document.createElement('div');
      p.className = 'small';
      p.textContent = text;
      host.appendChild(p);
    }

    function addInput(id, label, type, placeholder) {
      const l = document.createElement('label');
      l.className = 'small';
      l.textContent = label;
      host.appendChild(l);
      const i = document.createElement('input');
      i.id = id;
      i.type = type || 'text';
      if (placeholder) i.placeholder = placeholder;
      host.appendChild(i);
    }

    function addCheckbox(id, label) {
      const wrap = document.createElement('div');
      wrap.className = 'kv';
      const k = document.createElement('div');
      k.className = 'k';
      k.textContent = label;
      const v = document.createElement('div');
      const cb = document.createElement('input');
      cb.type = 'checkbox';
      cb.id = id;
      cb.style.width = 'auto';
      cb.style.transform = 'scale(1.2)';
      v.appendChild(cb);
      wrap.appendChild(k);
      wrap.appendChild(v);
      host.appendChild(wrap);
    }

    if (step === 'welcome') {
      addLabel('Wizard required until completion.');
      addLabel('Passwords are never logged.');
      return;
    }

    if (step === 'network') {
      addCheckbox('wiz_sta_enabled', 'Enable STA (join existing Wi‑Fi)');
      addInput('wiz_sta_ssid', 'STA SSID', 'text');
      addInput('wiz_sta_password', 'STA Password', 'password');
      addInput('wiz_ap_password', 'AP Password (min 8)', 'password');
      addLabel('AP password must be changed from the default to complete setup.');
      return;
    }

    if (step === 'time') {
      addLabel('Time / RTC (best-effort).');
      addInput('wiz_timezone', 'Timezone (IANA name)', 'text', 'America/Los_Angeles');
      addCheckbox('wiz_set_time_now', 'Set device time to browser time');
      addLabel('If an RTC is present and configured, it will be adjusted.');
      return;
    }

    if (step === 'storage') {
      addLabel('Storage and log retention.');
      addCheckbox('wiz_sd_required', 'Require SD for normal operation');
      addInput('wiz_log_retention_days', 'Log retention (days)', 'number', '365');
      return;
    }

    if (step === 'controls') {
      addCheckbox('wiz_web_enabled', 'Enable Web UI controls');
      addCheckbox('wiz_nfc_enabled', 'Enable NFC controls (stub)');
      return;
    }

    if (step === 'sensors') {
      addCheckbox('wiz_motion_enabled', 'Motion sensor enabled');
      addCheckbox('wiz_door_enabled', 'Door/window sensor enabled');
      addLabel('At least one primary sensor must be enabled to complete setup.');
      return;
    }

    if (step === 'power') {
      addLabel('Power settings (stub in M1).');
      addCheckbox('wiz_batt_measure', 'Battery measurement enabled');
      addInput('wiz_batt_low', 'Battery low voltage', 'number', '0');
      addInput('wiz_batt_crit', 'Battery critical voltage', 'number', '0');
      addInput('wiz_batt_wifi', 'Battery Wi‑Fi disable voltage', 'number', '0');
      return;
    }

    if (step === 'outputs') {
      addLabel('Outputs settings (stub patterns in M1).');
      addCheckbox('wiz_horn_enabled', 'Horn enabled');
      addCheckbox('wiz_light_enabled', 'Light enabled');
      return;
    }

    if (step === 'security') {
      addInput('wiz_admin_password', 'Admin password (min 8)', 'password');
      addInput('wiz_admin_timeout', 'Admin mode timeout (seconds)', 'number', '600');
      return;
    }

    if (step === 'review') {
      addLabel('Review and complete setup.');
      addLabel('State machine and real controls are not active in M1.');
      return;
    }
  }

  async function refreshStatus() {
    const r = await jget('/api/status');
    if (!r.ok) return;
    const j = r.json || {};

    state.setupRequired = !!j.setup_required;
    state.lastStep = j.setup_last_step || 'welcome';
    state.adminMode = j.admin_mode || (j.admin_mode_active ? 'authenticated' : 'off');
    state.adminRemainingS = j.admin_mode_remaining_s || 0;
    state.adminActive = state.adminMode === 'authenticated';

    const fw = `${j.firmware_name || ''} ${j.firmware_version || ''}`.trim();
    setText('fw', fw.length ? fw : 'Unknown');
    setText('dev', j.device_suffix ? `…${j.device_suffix}` : 'Unknown');
    setText('maintFw', fw.length ? fw : 'Unknown');
    setText('resetReason', asUnknown(j.reset_reason));
    setText('state', asUnknown(j.state));

    const lt = j.last_transition || {};
    const reason = asUnknown(lt.reason);
    const ts = (lt.time_valid && lt.ts && lt.ts !== 'u') ? lt.ts : 'Unknown';
    setText('lastChange', `${reason} — ${ts}`);

    const t = j.time || {};
    const timeValid = t.time_valid === true;
    setText('timeStatus', timeValid ? 'Valid' : 'Unknown');
    setText('timeHelper', timeValid ? '' : 'Timestamps may be unavailable until RTC is set.');
    setText('rtc', asUnknown(t.status));
    setText('now', asUnknown(t.now_iso8601_utc));

    setText('network', networkText(j));

    const s = j.storage || {};
    setText('storageStatus', storageStatusText(s));
    setText('storageBackend', asUnknown(s.active_backend));
    if (typeof j.flash_fs_ok === 'boolean') {
      setText('fs', j.flash_fs_ok ? 'OK' : 'Missing');
    } else {
      setText('fs', 'Unknown');
    }

    const nfc = j.nfc || null;
    setText('nfcStatus', nfcHealthText(nfc));
    if (nfc && nfc.lockout_active) {
      const rem = (nfc.lockout_remaining_s !== undefined) ? nfc.lockout_remaining_s : '';
      setText('lockout', rem !== '' ? `Active (${rem}s)` : 'Active');
    } else if (nfc) {
      setText('lockout', 'None');
    } else {
      setText('lockout', 'Unknown');
    }

    setText('setup', state.setupRequired ? `REQUIRED (${state.lastStep})` : 'COMPLETE');
    setText('admin', adminModeText());
    if (j.state === 'TRIGGERED') {
      setText('stateHelper', 'Alarm is latched until cleared by Admin.');
    } else if (j.state === 'FAULT') {
      setText('stateHelper', 'Some guarantees may be reduced. Review faults below.');
    } else {
      setText('stateHelper', '');
    }

    const wizStep = $('wizStep');
    if (wizStep) {
      wizStep.value = state.lastStep;
      renderWizardFields(wizStep.value);
    }

    setHidden($('adminLogin'), state.adminActive);
    setHidden($('adminActive'), !state.adminActive);
    setHidden($('otaCard'), !state.adminActive);
    setHidden($('outputTestsCard'), !state.adminActive);
    setHidden($('logsCard'), !state.adminActive);

    setText('controlsHint', state.adminActive ? '' : 'Controls are available in Admin mode.');
    if (isSetupPage) {
      const why = 'Setup is required because this device is not fully configured yet. Complete the steps below to make it ready for use.';
      setText('topNote', state.setupRequired ? why : 'Setup completed.');
      setText('wizardHint', state.setupRequired ? 'Complete the steps below. Unknown values are OK until hardware is ready.' : '');
      setHidden($('wizardCard'), !state.setupRequired);
      setHidden($('setupCompleteCard'), state.setupRequired);
      const canRerun = state.adminActive;
      setDisabled($('btnSetupRerun'), !canRerun);
      setText('setupRerunHint', canRerun ? 'Re-run setup will log changes.' : 'Admin Authenticated required to re-run setup.');
    } else {
      setHidden($('wizardCard'), !state.setupRequired);
      setText('topNote', state.setupRequired ? 'Setup required.' : '');
    }

    await refreshConfig();
  }

  async function refreshEvents() {
    const host = $('events');
    if (!host) return;
    const r = await jget('/api/events?limit=12');
    if (!r.ok) return;
    const events = Array.isArray(r.json) ? r.json : [];
    host.innerHTML = '';
    for (const e of events) {
      const div = document.createElement('div');
      div.className = 'small';
      const ts = asUnknown(e.ts);
      const sev = e.severity || '';
      const src = e.source || '';
      const msg = e.msg || '';
      div.textContent = `${ts} [${sev}] ${src}: ${msg}`;
      host.appendChild(div);
    }
  }

  async function downloadLogs(range) {
    setText('logsError', '');
    const r = await fetch(`/api/logs/download?range=${encodeURIComponent(range)}`, {
      headers: hdrs(),
    });
    if (r.status === 409) {
      let msg = 'Too large to download. Choose a shorter range.';
      try {
        const j = await r.json();
        if (j && j.message) msg = j.message;
      } catch (e) {}
      setText('logsError', msg);
      return;
    }
    if (!r.ok) {
      if (r.status === 403) {
        setText('logsError', 'Admin mode required.');
      } else {
        setText('logsError', `Error: ${r.status}`);
      }
      return;
    }
    const blob = await r.blob();
    const url = URL.createObjectURL(blob);
    const a = document.createElement('a');
    const stamp = new Date().toISOString().slice(0, 10);
    a.href = url;
    a.download = `logs_${range}_${stamp}.txt`;
    document.body.appendChild(a);
    a.click();
    a.remove();
    URL.revokeObjectURL(url);
  }

  function setOtaFileInfo(file) {
    if (!file) {
      setText('otaFileName', '—');
      setText('otaFileSize', '—');
      return;
    }
    setText('otaFileName', file.name || '—');
    setText('otaFileSize', `${file.size} bytes`);
  }

  on($('wizStep'), 'change', () => {
    renderWizardFields($('wizStep').value);
    setText('wizError', '');
  });

  on($('wizSave'), 'click', async () => {
    setText('wizError', '');
    const step = $('wizStep').value;
    const data = {};

    if (step === 'network') {
      data.wifi_sta_enabled = !!$('wiz_sta_enabled').checked;
      data.wifi_sta_ssid = $('wiz_sta_ssid').value || '';
      data.wifi_sta_password = $('wiz_sta_password').value || '';
      data.wifi_ap_password = $('wiz_ap_password').value || '';
    }
    if (step === 'time') {
      data.timezone = $('wiz_timezone').value || '';
      if ($('wiz_set_time_now').checked) {
        data.rtc_set_epoch_s = Math.floor(Date.now() / 1000);
      }
    }
    if (step === 'storage') {
      data.sd_required = !!$('wiz_sd_required').checked;
      data.log_retention_days = parseInt($('wiz_log_retention_days').value || '365', 10) || 365;
    }
    if (step === 'controls') {
      data.control_web_enabled = !!$('wiz_web_enabled').checked;
      data.control_nfc_enabled = !!$('wiz_nfc_enabled').checked;
    }
    if (step === 'sensors') {
      data.motion_enabled = !!$('wiz_motion_enabled').checked;
      data.door_enabled = !!$('wiz_door_enabled').checked;
    }
    if (step === 'power') {
      data.battery_measure_enabled = !!$('wiz_batt_measure').checked;
      data.battery_low_v = parseFloat($('wiz_batt_low').value || '0') || 0;
      data.battery_critical_v = parseFloat($('wiz_batt_crit').value || '0') || 0;
      data.battery_wifi_disable_v = parseFloat($('wiz_batt_wifi').value || '0') || 0;
    }
    if (step === 'outputs') {
      data.horn_enabled = !!$('wiz_horn_enabled').checked;
      data.light_enabled = !!$('wiz_light_enabled').checked;
    }
    if (step === 'security') {
      data.admin_web_password = $('wiz_admin_password').value || '';
      data.admin_mode_timeout_s = parseInt($('wiz_admin_timeout').value || '600', 10) || 600;
    }

    const r = await jpost('/api/wizard/step', { step, data });
    if (!r.ok) {
      setText('wizError', `Error: ${(r.json && r.json.error) || r.status}`);
      return;
    }
    await refreshStatus();
    await refreshEvents();
  });

  on($('wizComplete'), 'click', async () => {
    setText('wizError', '');
    const r = await jpost('/api/wizard/complete', {});
    if (!r.ok) {
      setText('wizError', `Error: ${(r.json && r.json.error) || r.status}`);
      return;
    }
    await refreshStatus();
    await refreshEvents();
  });

  on($('btnAdminLogin'), 'click', async () => {
    setText('adminError', '');
    const pw = $('adminPassword').value || '';
    const r = await jpost('/api/admin/login', { password: pw });
    if (!r.ok) {
      setText('adminError', `Error: ${(r.json && r.json.error) || r.status}`);
      return;
    }
    state.adminToken = r.json.token || '';
    localStorage.setItem('wss_admin_token', state.adminToken);
    await refreshStatus();
    await refreshEvents();
  });

  on($('btnAdminLogout'), 'click', async () => {
    setText('adminError', '');
    await jpost('/api/admin/logout', {});
    state.adminToken = '';
    localStorage.removeItem('wss_admin_token');
    await refreshStatus();
    await refreshEvents();
  });

  on($('btnArm'), 'click', async () => {
    setText('controlsError', '');
    const r = await jpost('/api/control/arm', {});
    if (!r.ok) setText('controlsError', `Error: ${(r.json && r.json.stub) ? 'stub' : r.status}`);
    await refreshEvents();
  });
  on($('btnDisarm'), 'click', async () => {
    setText('controlsError', '');
    const r = await jpost('/api/control/disarm', {});
    if (!r.ok) setText('controlsError', `Error: ${(r.json && r.json.stub) ? 'stub' : r.status}`);
    await refreshEvents();
  });
  on($('btnSilence'), 'click', async () => {
    setText('controlsError', '');
    const r = await jpost('/api/control/silence', {});
    if (!r.ok) setText('controlsError', `Error: ${(r.json && r.json.stub) ? 'stub' : r.status}`);
    await refreshEvents();
  });

  on($('btnLogsToday'), 'click', async () => {
    await downloadLogs('today');
  });
  on($('btnLogs7d'), 'click', async () => {
    await downloadLogs('7d');
  });
  on($('btnLogsAll'), 'click', async () => {
    await downloadLogs('all');
  });

  async function runTest(endpoint, okMessage) {
    setText('testError', '');
    const r = await jpost(endpoint, {});
    if (!r.ok) {
      const msg = (r.json && r.json.error) ? `Error: ${r.json.error}` : `Error: ${r.status}`;
      setText('testError', msg);
      return;
    }
    setText('testError', okMessage || 'OK');
  }

  on($('btnTestHorn'), 'click', async () => {
    await runTest('/api/test/horn', 'Horn test started.');
  });
  on($('btnTestLight'), 'click', async () => {
    await runTest('/api/test/light', 'Light test started.');
  });
  on($('btnTestStop'), 'click', async () => {
    await runTest('/api/test/stop', 'Tests stopped.');
  });

  on($('otaFile'), 'change', () => {
    const file = $('otaFile').files && $('otaFile').files[0];
    setOtaFileInfo(file);
  });

  on($('btnOtaUpload'), 'click', async () => {
    const file = $('otaFile').files && $('otaFile').files[0];
    if (!file) return;
    setText('otaResult', '—');
    const fd = new FormData();
    fd.append('firmware', file, file.name || 'firmware.bin');
    const r = await fetch('/api/ota/upload', { method: 'POST', headers: authHeader(), body: fd });
    let msg = '';
    try {
      const j = await r.json();
      if (j && j.message) msg = j.message;
      if (j && j.ok) {
        setText('otaResult', msg || 'Rebooting now. Reconnect to the device Wi-Fi.');
        return;
      }
      if (j && j.error && !msg) msg = `Error: ${j.error}`;
    } catch (e) {}
    if (!r.ok && !msg) {
      msg = r.status === 403 ? 'Admin mode required.' : `Error: ${r.status}`;
    }
    setText('otaResult', msg || 'Upload failed.');
  });

  async function restartWizard() {
    // In M1, restarting is implemented as setting setup_completed=false via wizard step.
    setText('adminError', '');
    setText('setupRerunHint', '');
    const r = await jpost('/api/wizard/step', { step: 'welcome', data: { setup_completed: false } });
    if (!r.ok) {
      const msg = `Error: ${(r.json && r.json.error) || r.status}`;
      setText('adminError', msg);
      setText('setupRerunHint', msg);
      return;
    }
    await refreshStatus();
    await refreshEvents();
  }

  on($('btnWizardRestart'), 'click', restartWizard);
  on($('btnSetupRerun'), 'click', async () => {
    if (!state.adminActive) {
      setText('setupRerunHint', 'Admin Authenticated required to re-run setup.');
      return;
    }
    await restartWizard();
  });

  (function setupHoldFactoryRestore() {
    const btn = $('btnFactoryRestore');
    if (!btn) return;
    let downAt = 0;
    btn.addEventListener('pointerdown', () => {
      downAt = Date.now();
      setText('factoryError', '');
    });
    btn.addEventListener('pointerup', async () => {
      const held = Date.now() - downAt;
      const phrase = $('factoryPhrase').value || '';
      const r = await jpost('/api/factory_restore', { confirm_phrase: phrase, hold_ms: held });
      if (!r.ok) {
        setText('factoryError', `Error: ${(r.json && r.json.error) || r.status}`);
      } else {
        state.adminToken = '';
        localStorage.removeItem('wss_admin_token');
      }
      await refreshStatus();
      await refreshEvents();
    });
    btn.addEventListener('pointerleave', () => {
      downAt = 0;
    });
  })();

  async function loop() {
    try {
      await refreshStatus();
      await refreshEvents();
    } catch (e) {
      // Silent UI; offline behavior is expected.
    }
    setTimeout(loop, 2000);
  }

  loop();
})();
