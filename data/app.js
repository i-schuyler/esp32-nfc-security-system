// data/app.js
(function () {
  const $ = (id) => document.getElementById(id);

  const LD2410B_DEFAULT_BAUD = 256000;
  const LD2410B_SAFE_PINS = [4, 5, 16, 17, 18, 19, 21, 22, 23, 25, 26, 27, 32, 33];
  const LD2410B_SAFE_TX_PINS = LD2410B_SAFE_PINS.filter((pin) => pin < 34);
  const PN532_CS_SAFE_PINS = [16, 17, 25, 26, 27, 32, 33];
  const PN532_RST_SAFE_PINS = PN532_CS_SAFE_PINS.slice();
  const PN532_IRQ_SAFE_PINS = [32, 33, 34, 35, 36, 39];
  const SD_CS_SAFE_PINS = [13, 16, 17, 25, 26, 27, 32, 33];
  const OUTPUT_GPIO_SAFE_PINS = [13, 14, 16, 17, 25, 26, 27, 32, 33];

  const state = {
    adminToken: '',
    setupRequired: true,
    lastStep: 'welcome',
    adminMode: 'off',
    adminActive: false,
    adminRemainingS: 0,
    wizInitialized: false,
    motionKind: 'gpio',
    ld2410b: { rx: 16, tx: 17, baud: LD2410B_DEFAULT_BAUD },
    nfc: { iface: 'spi', cs: 27, irq: 32, rst: 33 },
    sd: { enabled: true, cs: 13 },
    outputs: {
      hornPin: -1,
      lightPin: -1,
      hornActiveLow: false,
      lightActiveLow: false,
      hornEnabled: true,
      lightEnabled: true,
      lightPattern: 'steady',
      silencedLightPattern: 'steady',
    },
    nfcProvision: { stage: 'idle', startScanMs: 0, firstScanMs: 0, confirmedRole: 'unknown' },
  };
  const APP_PAGE = (window.APP_PAGE || 'main');
  const isSetupPage = APP_PAGE === 'setup';
  const WIZARD_STEPS = [
    { id: 'welcome', label: 'Welcome + Admin Password' },
    { id: 'network', label: 'Network' },
    { id: 'sensors', label: 'Inputs (NFC + Sensors)' },
    { id: 'time', label: 'Time & RTC' },
    { id: 'storage', label: 'Storage' },
    { id: 'outputs', label: 'Outputs' },
    { id: 'review', label: 'Review & Complete' },
  ];
  const WIZARD_STEP_IDS = WIZARD_STEPS.map((step) => step.id);
  const WIZARD_VISITED_KEY = 'wss_setup_visited_steps_v1';
  const WIZARD_STEP_TOUCHED_KEY = 'wss_setup_step_touched_v1';
  const WIZARD_ADMIN_PW_SET_KEY = 'wss_setup_admin_pw_set_v1';
  const WIZARD_AP_PW_SET_KEY = 'wss_setup_ap_pw_set_v1';
  const WIZARD_SENSOR_SET_KEY = 'wss_setup_primary_sensor_enabled_v1';
  const WIZARD_STEP_ALIAS = {
    security: 'welcome',
    nfc: 'sensors',
    controls: 'sensors',
    power: 'outputs',
  };

  function getBool(key) {
    return localStorage.getItem(key) === '1';
  }

  function setBool(key, value) {
    if (value) localStorage.setItem(key, '1');
    else localStorage.removeItem(key);
  }

  function loadVisitedSteps() {
    const raw = localStorage.getItem(WIZARD_VISITED_KEY) || '[]';
    try {
      const parsed = JSON.parse(raw);
      if (Array.isArray(parsed)) return new Set(parsed);
    } catch (e) {}
    return new Set();
  }

  const visitedSteps = loadVisitedSteps();

  function markStepVisited(step) {
    if (!WIZARD_STEP_IDS.includes(step)) return;
    visitedSteps.add(step);
    localStorage.setItem(WIZARD_VISITED_KEY, JSON.stringify(Array.from(visitedSteps)));
  }

  function hasVisitedAllSteps() {
    return WIZARD_STEP_IDS.every((step) => visitedSteps.has(step));
  }

  function setStepTouched() {
    setBool(WIZARD_STEP_TOUCHED_KEY, true);
  }

  function stepTouched() {
    return getBool(WIZARD_STEP_TOUCHED_KEY);
  }

  function normalizeStep(step) {
    if (WIZARD_STEP_IDS.includes(step)) return step;
    if (WIZARD_STEP_ALIAS[step]) return WIZARD_STEP_ALIAS[step];
    return WIZARD_STEP_IDS[0];
  }

  function isNonDefaultApPassword(pw) {
    if (!pw || pw.length < 8) return false;
    return !pw.startsWith('ChangeMe-');
  }

  function updateAdminPasswordFlag(onlySetTrue) {
    const pw = $('wiz_admin_password');
    const ok = !!(pw && pw.value && pw.value.length >= 8);
    if (ok) setBool(WIZARD_ADMIN_PW_SET_KEY, true);
    else if (!onlySetTrue) setBool(WIZARD_ADMIN_PW_SET_KEY, false);
  }

  function updateApPasswordFlag(onlySetTrue) {
    const pw = $('wiz_ap_password');
    const ok = !!(pw && isNonDefaultApPassword(pw.value || ''));
    if (ok) setBool(WIZARD_AP_PW_SET_KEY, true);
    else if (!onlySetTrue) setBool(WIZARD_AP_PW_SET_KEY, false);
  }

  function updatePrimarySensorFlag(onlySetTrue) {
    const motion = $('wiz_motion_enabled');
    const door = $('wiz_door_enabled');
    const ok = !!((motion && motion.checked) || (door && door.checked));
    if (ok) setBool(WIZARD_SENSOR_SET_KEY, true);
    else if (!onlySetTrue) setBool(WIZARD_SENSOR_SET_KEY, false);
  }

  function completionStatus() {
    return {
      adminPasswordSet: getBool(WIZARD_ADMIN_PW_SET_KEY),
      apPasswordSet: getBool(WIZARD_AP_PW_SET_KEY),
      primarySensorEnabled: getBool(WIZARD_SENSOR_SET_KEY),
      allVisited: hasVisitedAllSteps(),
    };
  }

  function updateWizardActions() {
    const stepEl = $('wizStep');
    if (!stepEl) return;
    const current = normalizeStep(stepEl.value);
    const isLast = current === WIZARD_STEP_IDS[WIZARD_STEP_IDS.length - 1];
    const status = completionStatus();
    const conflicts = detectPinConflicts();
    const hasConflicts = conflicts.length > 0;
    const canComplete = isLast && status.allVisited
      && status.adminPasswordSet && status.apPasswordSet && status.primarySensorEnabled
      && !hasConflicts;
    setHidden($('wizComplete'), !canComplete);

    if (!$('wizCompleteHint')) return;
    if (!isLast) {
      setText('wizCompleteHint', 'Complete setup is available on the last step.');
      return;
    }
    const missing = [];
    if (!status.allVisited) missing.push('visit all steps');
    if (!status.adminPasswordSet) missing.push('set admin password');
    if (!status.apPasswordSet) missing.push('change AP password from default');
    if (!status.primarySensorEnabled) missing.push('enable a primary sensor');
    const base = missing.length ? `To complete: ${missing.join(', ')}.` : '';
    if (hasConflicts) {
      const conflictMsg = `Pin conflicts: ${conflicts.join('; ')}.`;
      setText('wizCompleteHint', base.length ? `${base} ${conflictMsg}` : conflictMsg);
      return;
    }
    setText('wizCompleteHint', base);
  }

  function setWizardStep(step, opts) {
    const stepEl = $('wizStep');
    if (!stepEl) return;
    const normalized = normalizeStep(step);
    stepEl.value = normalized;
    if (opts && opts.touched) setStepTouched();
    renderWizardFields(normalized);
    markStepVisited(normalized);
    state.wizInitialized = true;
    updateWizardActions();
  }

  function goToNextWizardStep() {
    const stepEl = $('wizStep');
    if (!stepEl) return;
    const current = normalizeStep(stepEl.value);
    const idx = WIZARD_STEP_IDS.indexOf(current);
    if (idx < 0 || idx >= WIZARD_STEP_IDS.length - 1) return;
    setWizardStep(WIZARD_STEP_IDS[idx + 1], { touched: true });
  }

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

  function clearAdminSession() {
    state.adminToken = '';
  }

  function saveFailedMessage(detail) {
    return 'Settings could not be saved to flash. Try Save again. If it still fails, reboot the device and try again.';
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

  function nfcWizardHealthText(nfc) {
    if (!nfc) return 'NFC: Unknown';
    const health = nfc.health || '';
    let label = 'Degraded';
    if (health === 'ok') label = 'OK';
    else if (health === 'unavailable' || health === 'disabled_cfg' || health === 'disabled_build') label = 'Unavailable';
    return `NFC: ${label}`;
  }

  function nfcRoleLabel(role) {
    if (role === 'admin') return 'Admin';
    if (role === 'user') return 'User';
    return 'Unknown';
  }

  function updateNfcProvisionState(nfc) {
    if (!nfc) return;
    const lastScanMs = nfc.last_scan_ms || 0;
    if (state.nfcProvision.stage === 'waiting_first') {
      if (lastScanMs > state.nfcProvision.startScanMs && nfc.last_scan_result === 'ok') {
        state.nfcProvision.stage = 'waiting_confirm';
        state.nfcProvision.firstScanMs = lastScanMs;
      }
      return;
    }
    if (state.nfcProvision.stage === 'waiting_confirm') {
      if (lastScanMs > state.nfcProvision.firstScanMs && nfc.last_scan_result === 'ok') {
        state.nfcProvision.stage = 'confirmed';
        state.nfcProvision.confirmedRole = nfc.last_role || 'unknown';
      }
    }
  }

  function updateNfcProvisionUi(nfc) {
    const statusEl = $('wiz_nfc_admin_status');
    if (!statusEl) return;
    const healthEl = $('wiz_nfc_health');
    if (healthEl) healthEl.textContent = nfcWizardHealthText(nfc);

    if (!nfc || nfc.provisioning_active !== true) {
      if (state.nfcProvision.stage !== 'idle' && state.nfcProvision.stage !== 'confirmed') {
        state.nfcProvision.stage = 'idle';
        state.nfcProvision.startScanMs = 0;
        state.nfcProvision.firstScanMs = 0;
        setText('wiz_nfc_admin_error', 'Provisioning ended. Start Admin-card scan again.');
      }
    }

    const stage = state.nfcProvision.stage;
    if (stage === 'waiting_first') {
      const rem = (nfc && nfc.provisioning_remaining_s > 0)
        ? ` (${nfc.provisioning_remaining_s}s remaining)` : '';
      setText('wiz_nfc_admin_status', `Scan the Admin card now${rem}.`);
      setText('wiz_nfc_admin_role', '');
      return;
    }
    if (stage === 'waiting_confirm') {
      const rem = (nfc && nfc.provisioning_remaining_s > 0)
        ? ` (${nfc.provisioning_remaining_s}s remaining)` : '';
      setText('wiz_nfc_admin_status', `Scan the same card again to confirm${rem}.`);
      setText('wiz_nfc_admin_role', '');
      return;
    }
    if (stage === 'confirmed') {
      const role = nfcRoleLabel(state.nfcProvision.confirmedRole);
      setText('wiz_nfc_admin_status', 'Admin card confirmed.');
      setText('wiz_nfc_admin_role', `Role: ${role}`);
      return;
    }
    setText('wiz_nfc_admin_status', 'Enter the admin password to start the Admin-card scan.');
    setText('wiz_nfc_admin_role', '');
  }

  function storageStatusText(storage) {
    if (!storage) return 'Unknown';
    const status = storage.status || storage.sd_status || '';
    if (status === 'DISABLED') return 'SD Disabled (Using Flash Fallback)';
    if (status === 'OK') return 'SD OK';
    if (storage.fallback_active) return 'SD Missing (Using Flash Fallback)';
    if (status.length) return `SD ${status}`;
    return 'Unknown';
  }

  function isInputOnlyPin(pin) {
    return pin >= 34 && pin <= 39;
  }

  function pinValueText(pin, emptyText) {
    if (pin === undefined || pin === null || pin < 0) return emptyText || 'Not used';
    return String(pin);
  }

  function detectPinConflicts() {
    const conflicts = [];
    const claims = {};

    function claim(pin, role) {
      if (pin === undefined || pin === null || pin < 0) return;
      const key = String(pin);
      if (!claims[key]) claims[key] = [];
      claims[key].push(role);
    }

    function checkOutput(role, pin) {
      if (pin === undefined || pin === null || pin < 0) return;
      if (isInputOnlyPin(pin)) {
        conflicts.push(`${role} uses input-only GPIO ${pin}`);
      }
    }

    const sdEnabled = state.sd.enabled !== false;
    const sdCs = sdEnabled ? state.sd.cs : -1;
    claim(sdCs, 'SD CS');
    checkOutput('SD CS', sdCs);

    const nfcIface = state.nfc.iface || 'spi';
    if (nfcIface === 'spi') {
      const nfcCs = state.nfc.cs;
      const nfcRst = state.nfc.rst;
      const nfcIrq = state.nfc.irq;
      claim(nfcCs, 'NFC CS');
      claim(nfcRst, 'NFC RST');
      claim(nfcIrq, 'NFC IRQ');
      checkOutput('NFC CS', nfcCs);
      checkOutput('NFC RST', nfcRst);
    }

    const hornPin = state.outputs.hornPin;
    const lightPin = state.outputs.lightPin;
    claim(hornPin, 'Horn');
    claim(lightPin, 'Light');
    checkOutput('Horn', hornPin);
    checkOutput('Light', lightPin);

    if (state.motionKind === 'ld2410b_uart') {
      const rx = state.ld2410b.rx;
      const tx = state.ld2410b.tx;
      claim(rx, 'LD2410B RX');
      claim(tx, 'LD2410B TX');
      checkOutput('LD2410B TX', tx);
    }

    for (const pin in claims) {
      const roles = claims[pin];
      if (roles.length > 1) {
        conflicts.push(`GPIO ${pin} used by ${roles.join(' + ')}`);
      }
    }
    return conflicts;
  }

  function buildPinMapRows() {
    const rows = [
      { label: 'SPI bus (fixed)', value: 'SCK 18, MISO 19, MOSI 23' },
      { label: 'I2C bus (fixed)', value: 'SDA 21, SCL 22' },
    ];

    const sdEnabled = state.sd.enabled !== false;
    rows.push({
      label: 'SD CS',
      value: sdEnabled ? pinValueText(state.sd.cs) : 'Disabled',
    });

    const nfcIface = state.nfc.iface || 'spi';
    if (nfcIface === 'spi') {
      rows.push({ label: 'NFC CS', value: pinValueText(state.nfc.cs) });
      rows.push({ label: 'NFC RST', value: pinValueText(state.nfc.rst) });
      rows.push({ label: 'NFC IRQ', value: pinValueText(state.nfc.irq) });
    }

    rows.push({ label: 'Horn GPIO', value: pinValueText(state.outputs.hornPin) });
    rows.push({ label: 'Light GPIO', value: pinValueText(state.outputs.lightPin) });

    if (state.motionKind === 'ld2410b_uart') {
      rows.push({ label: 'LD2410B RX', value: pinValueText(state.ld2410b.rx) });
      rows.push({ label: 'LD2410B TX', value: pinValueText(state.ld2410b.tx) });
    }

    return rows;
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

    function addSelect(id, label, options, value) {
      const l = document.createElement('label');
      l.className = 'small';
      l.textContent = label;
      host.appendChild(l);
      const s = document.createElement('select');
      s.id = id;
      for (const opt of options) {
        const o = document.createElement('option');
        o.value = opt.value;
        o.textContent = opt.label;
        s.appendChild(o);
      }
      if (value !== undefined) s.value = value;
      host.appendChild(s);
      return s;
    }

    function addKv(label, value) {
      const row = document.createElement('div');
      row.className = 'kv';
      const k = document.createElement('div');
      k.className = 'k';
      k.textContent = label;
      const v = document.createElement('div');
      v.className = 'v';
      v.textContent = value;
      row.appendChild(k);
      row.appendChild(v);
      host.appendChild(row);
    }

    if (step === 'welcome') {
      addLabel('Setup is required until all steps are completed.');
      addLabel('Set the admin password to protect changes.');
      addInput('wiz_admin_password', 'Admin password (min 8)', 'password');
      addInput('wiz_admin_timeout', 'Admin mode timeout (seconds)', 'number', '600');
      addLabel('Passwords are never shown or logged.');
      const pw = $('wiz_admin_password');
      if (pw) pw.addEventListener('input', () => { updateAdminPasswordFlag(); updateWizardActions(); });
      updateAdminPasswordFlag(true);
      return;
    }

    if (step === 'network') {
      addCheckbox('wiz_sta_enabled', 'Enable STA (join existing Wi‑Fi)');
      addInput('wiz_sta_ssid', 'STA SSID', 'text');
      addInput('wiz_sta_password', 'STA Password', 'password');
      addInput('wiz_ap_password', 'AP Password (min 8)', 'password');
      addLabel('AP password must be changed from the default to complete setup.');
      const ap = $('wiz_ap_password');
      if (ap) ap.addEventListener('input', () => { updateApPasswordFlag(); updateWizardActions(); });
      updateApPasswordFlag(true);
      return;
    }

    if (step === 'time') {
      addLabel('Accurate timestamps improve logs and incident history.');
      addLabel('Set timezone and confirm RTC is detected.');
      addInput('wiz_timezone', 'Timezone (IANA name)', 'text', 'America/Los_Angeles');
      addCheckbox('wiz_set_time_now', 'Set device time to browser time');
      addLabel('RTC: Unknown until detected.');
      return;
    }

    if (step === 'storage') {
      addLabel('Logs are stored on SD when available.');
      addCheckbox('wiz_sd_enabled', 'Enable SD logging');
      addLabel('Not using SD? Disable SD logging to use flash ring.');
      const sdEnabled = $('wiz_sd_enabled');
      if (sdEnabled) sdEnabled.checked = state.sd.enabled !== false;
      const csDefault = SD_CS_SAFE_PINS.includes(state.sd.cs) ? state.sd.cs : 13;
      addSelect('wiz_sd_cs', 'SD CS (SPI)', SD_CS_SAFE_PINS.map((pin) => ({
        value: String(pin),
        label: `GPIO ${pin}`,
      })), String(csDefault));
      const sdCs = $('wiz_sd_cs');
      if (sdEnabled && sdCs) {
        sdCs.disabled = !sdEnabled.checked;
        sdEnabled.addEventListener('change', () => {
          sdCs.disabled = !sdEnabled.checked;
        });
      }
      addCheckbox('wiz_sd_required', 'Require SD for normal operation');
      addInput('wiz_log_retention_days', 'Log retention (days)', 'number', '365');
      addLabel('SD: Unknown until detected.');
      return;
    }

    if (step === 'sensors') {
      addLabel('Inputs detect activity and authorize control.');
      addLabel('Enable at least one primary sensor.');
      addLabel('Sensors: Unknown until configured.');
      const nfcHealthLine = document.createElement('div');
      nfcHealthLine.className = 'small';
      nfcHealthLine.id = 'wiz_nfc_health';
      nfcHealthLine.textContent = 'NFC: Unknown until a reader is detected.';
      host.appendChild(nfcHealthLine);
      addLabel('NFC module: PN532 (SPI).');
      const csDefault = PN532_CS_SAFE_PINS.includes(state.nfc.cs) ? state.nfc.cs : 27;
      const irqDefault = PN532_IRQ_SAFE_PINS.includes(state.nfc.irq) ? state.nfc.irq : -1;
      const rstDefault = PN532_RST_SAFE_PINS.includes(state.nfc.rst) ? state.nfc.rst : -1;
      addSelect('wiz_nfc_cs', 'NFC CS (SPI)', PN532_CS_SAFE_PINS.map((pin) => ({
        value: String(pin),
        label: `GPIO ${pin}`,
      })), String(csDefault));
      const irqOptions = [{ value: '-1', label: 'Not used' }].concat(PN532_IRQ_SAFE_PINS.map((pin) => ({
        value: String(pin),
        label: `GPIO ${pin}`,
      })));
      addSelect('wiz_nfc_irq', 'NFC IRQ (optional)', irqOptions, String(irqDefault));
      const rstOptions = [{ value: '-1', label: 'Not used' }].concat(PN532_RST_SAFE_PINS.map((pin) => ({
        value: String(pin),
        label: `GPIO ${pin}`,
      })));
      addSelect('wiz_nfc_rst', 'NFC RST (optional)', rstOptions, String(rstDefault));

      addLabel('Add the first Admin card to enable Admin access.');
      addLabel('Enter the admin password, start the scan, then scan the same card twice to confirm.');
      addInput('wiz_nfc_admin_password', 'Admin password (for first Admin card)', 'password');
      const nfcAdminStart = document.createElement('button');
      nfcAdminStart.id = 'wiz_nfc_admin_start';
      nfcAdminStart.textContent = 'Start Admin-card scan';
      host.appendChild(nfcAdminStart);
      const nfcAdminStatus = document.createElement('div');
      nfcAdminStatus.className = 'small';
      nfcAdminStatus.id = 'wiz_nfc_admin_status';
      host.appendChild(nfcAdminStatus);
      const nfcAdminRole = document.createElement('div');
      nfcAdminRole.className = 'small';
      nfcAdminRole.id = 'wiz_nfc_admin_role';
      host.appendChild(nfcAdminRole);
      const nfcAdminError = document.createElement('div');
      nfcAdminError.className = 'small muted';
      nfcAdminError.id = 'wiz_nfc_admin_error';
      host.appendChild(nfcAdminError);

      on(nfcAdminStart, 'click', async () => {
        setText('wiz_nfc_admin_error', '');
        setText('wiz_nfc_admin_role', '');
        const pwInput = $('wiz_nfc_admin_password');
        const pw = (pwInput && pwInput.value) ? pwInput.value : '';
        if (!pw.length) {
          setText('wiz_nfc_admin_error', 'Admin password required.');
          return;
        }
        const login = await jpost('/api/admin/login', { password: pw });
        if (!login.ok) {
          const err = (login.json && login.json.error) || login.status;
          if (err === 'admin_password_not_set') {
            setText('wiz_nfc_admin_error', 'Set the admin password in Step 1 first.');
          } else if (err === 'admin_nfc_required') {
            setText('wiz_nfc_admin_error', 'Scan an Admin card to enable login.');
          } else if (err === 'invalid_password') {
            setText('wiz_nfc_admin_error', 'Incorrect password.');
          } else {
            setText('wiz_nfc_admin_error', `Error: ${err}`);
          }
          return;
        }
        state.adminToken = login.json.token || '';
        if (pwInput) pwInput.value = '';
        const start = await jpost('/api/nfc/provision/start', { mode: 'add_admin' });
        if (!start.ok) {
          const err = (start.json && start.json.error) || start.status;
          if (start.status === 403 && err === 'admin_token_invalid') {
            clearAdminSession();
            setText('wiz_nfc_admin_error', 'Admin session expired. Log in again.');
          } else if (start.status === 403 && err === 'admin_required') {
            clearAdminSession();
            setText('wiz_nfc_admin_error', 'Admin Authenticated required.');
          } else if (err === 'provision_start_failed') {
            setText('wiz_nfc_admin_error', 'Provisioning could not start. Check the NFC reader.');
          } else {
            setText('wiz_nfc_admin_error', `Error: ${err}`);
          }
          return;
        }
        state.nfcProvision.stage = 'waiting_first';
        state.nfcProvision.startScanMs = state.nfc.lastScanMs || 0;
        state.nfcProvision.firstScanMs = 0;
        state.nfcProvision.confirmedRole = 'unknown';
        await refreshStatus();
      });

      const motionKind = addSelect('wiz_motion_kind', 'Motion sensor type', [
        { value: 'gpio', label: 'GPIO motion inputs' },
        { value: 'ld2410b_uart', label: 'LD2410B UART' },
      ], state.motionKind || 'gpio');

      const ldWrap = document.createElement('div');
      ldWrap.className = 'row';
      ldWrap.id = 'ld2410bFields';
      host.appendChild(ldWrap);

      function addLdLabel(text) {
        const p = document.createElement('div');
        p.className = 'small';
        p.textContent = text;
        ldWrap.appendChild(p);
      }

      function addLdSelect(id, label, options, value) {
        const l = document.createElement('label');
        l.className = 'small';
        l.textContent = label;
        ldWrap.appendChild(l);
        const s = document.createElement('select');
        s.id = id;
        for (const opt of options) {
          const o = document.createElement('option');
          o.value = opt.value;
          o.textContent = opt.label;
          s.appendChild(o);
        }
        if (value !== undefined) s.value = value;
        ldWrap.appendChild(s);
        return s;
      }

      function addLdInput(id, label, type, placeholder) {
        const l = document.createElement('label');
        l.className = 'small';
        l.textContent = label;
        ldWrap.appendChild(l);
        const i = document.createElement('input');
        i.id = id;
        i.type = type || 'text';
        if (placeholder) i.placeholder = placeholder;
        ldWrap.appendChild(i);
        return i;
      }

      addLdLabel('LD2410B: Unknown until UART data is seen.');
      addLdLabel('Check power and RX/TX if status stays Unknown.');
      const rxDefault = LD2410B_SAFE_PINS.includes(state.ld2410b.rx) ? state.ld2410b.rx : 16;
      const txDefault = LD2410B_SAFE_TX_PINS.includes(state.ld2410b.tx) ? state.ld2410b.tx : 17;
      addLdSelect('wiz_ld2410b_rx', 'LD2410B RX (ESP32 RX2)', LD2410B_SAFE_PINS.map((pin) => ({
        value: String(pin),
        label: `GPIO ${pin}`,
      })), String(rxDefault));
      addLdSelect('wiz_ld2410b_tx', 'LD2410B TX (ESP32 TX2)', LD2410B_SAFE_TX_PINS.map((pin) => ({
        value: String(pin),
        label: `GPIO ${pin}`,
      })), String(txDefault));
      const baudInput = addLdInput('wiz_ld2410b_baud', 'LD2410B baud', 'number', String(LD2410B_DEFAULT_BAUD));
      baudInput.value = String(state.ld2410b.baud || LD2410B_DEFAULT_BAUD);

      addCheckbox('wiz_motion_enabled', 'Motion sensor enabled');
      addCheckbox('wiz_door_enabled', 'Door/window sensor enabled');
      addLabel('At least one primary sensor must be enabled to complete setup.');
      addLabel('Control interfaces (optional).');
      addCheckbox('wiz_web_enabled', 'Enable Web UI controls');
      addCheckbox('wiz_nfc_enabled', 'Enable NFC controls');
      const motion = $('wiz_motion_enabled');
      const door = $('wiz_door_enabled');
      if (motion) motion.addEventListener('change', () => { updatePrimarySensorFlag(); updateWizardActions(); });
      if (door) door.addEventListener('change', () => { updatePrimarySensorFlag(); updateWizardActions(); });
      if (motionKind) {
        motionKind.addEventListener('change', () => {
          ldWrap.classList.toggle('hidden', motionKind.value !== 'ld2410b_uart');
        });
        ldWrap.classList.toggle('hidden', motionKind.value !== 'ld2410b_uart');
      }
      updatePrimarySensorFlag(true);
      return;
    }

    if (step === 'outputs') {
      addLabel('Outputs define alert behavior.');
      addLabel('Set output pins and polarity, then test before arming.');
      const pinOptions = [{ value: '-1', label: 'Use default' }].concat(OUTPUT_GPIO_SAFE_PINS.map((pin) => ({
        value: String(pin),
        label: `GPIO ${pin}`,
      })));
      addSelect('wiz_horn_gpio', 'Horn GPIO', pinOptions, String(state.outputs.hornPin || -1));
      addSelect('wiz_light_gpio', 'Light GPIO', pinOptions, String(state.outputs.lightPin || -1));
      addCheckbox('wiz_horn_active_low', 'Horn active-low (LOW = ON)');
      addCheckbox('wiz_light_active_low', 'Light active-low (LOW = ON)');
      addSelect('wiz_light_pattern', 'Light mode (Triggered)', [
        { value: 'off', label: 'Off' },
        { value: 'steady', label: 'Steady' },
        { value: 'strobe', label: 'Strobe' },
      ], state.outputs.lightPattern || 'steady');
      addSelect('wiz_silenced_light_pattern', 'Light mode (Silenced)', [
        { value: 'off', label: 'Off' },
        { value: 'steady', label: 'Steady' },
        { value: 'strobe', label: 'Strobe' },
      ], state.outputs.silencedLightPattern || 'steady');
      addCheckbox('wiz_horn_enabled', 'Horn enabled');
      addCheckbox('wiz_light_enabled', 'Light enabled');
      const hornPol = $('wiz_horn_active_low');
      const lightPol = $('wiz_light_active_low');
      if (hornPol) hornPol.checked = !!state.outputs.hornActiveLow;
      if (lightPol) lightPol.checked = !!state.outputs.lightActiveLow;
      const hornEnabled = $('wiz_horn_enabled');
      const lightEnabled = $('wiz_light_enabled');
      if (hornEnabled) hornEnabled.checked = !!state.outputs.hornEnabled;
      if (lightEnabled) lightEnabled.checked = !!state.outputs.lightEnabled;
      addLabel('Confirm polarity and light mode with Test outputs before arming.');
      return;
    }

    if (step === 'review') {
      addLabel('Review settings and complete setup.');
      addLabel('Final validation happens on the device.');
      addLabel('Pin Map (read-only).');
      const rows = buildPinMapRows();
      for (const row of rows) {
        addKv(row.label, row.value);
      }
      addLabel('If hardware does not respond after changing pins, reboot the device to reinitialize drivers.');
      return;
    }
  }

  async function refreshStatus() {
    const r = await jget('/api/status');
    if (!r.ok) return;
    const j = r.json || {};

    state.setupRequired = !!j.setup_required;
    state.lastStep = normalizeStep(j.setup_last_step || 'welcome');
    state.adminMode = j.admin_mode || (j.admin_mode_active ? 'authenticated' : 'off');
    state.adminRemainingS = j.admin_mode_remaining_s || 0;
    state.adminActive = state.adminMode === 'authenticated' && !!state.adminToken;
    const sens = j.sensors || {};
    state.motionKind = sens.motion_kind || 'gpio';
    const ld = sens.ld2410b || {};
    state.ld2410b = {
      rx: (ld.rx_gpio !== undefined) ? ld.rx_gpio : 16,
      tx: (ld.tx_gpio !== undefined) ? ld.tx_gpio : 17,
      baud: (ld.baud !== undefined) ? ld.baud : LD2410B_DEFAULT_BAUD,
    };

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
    state.sd = {
      enabled: (s.sd_enabled !== undefined) ? s.sd_enabled : true,
      cs: (s.sd_cs_gpio !== undefined) ? s.sd_cs_gpio : 13,
    };
    setText('storageStatus', storageStatusText(s));
    setText('storageBackend', asUnknown(s.active_backend));
    if (typeof j.flash_fs_ok === 'boolean') {
      setText('fs', j.flash_fs_ok ? 'OK' : 'Missing');
    } else {
      setText('fs', 'Unknown');
    }

    const nfc = j.nfc || null;
    if (nfc) {
      state.nfc = {
        iface: nfc.interface || 'spi',
        cs: (nfc.spi_cs_gpio !== undefined) ? nfc.spi_cs_gpio : 27,
        irq: (nfc.spi_irq_gpio !== undefined) ? nfc.spi_irq_gpio : -1,
        rst: (nfc.spi_rst_gpio !== undefined) ? nfc.spi_rst_gpio : -1,
        lastScanMs: nfc.last_scan_ms || 0,
        lastScanResult: nfc.last_scan_result || '',
        lastRole: nfc.last_role || 'unknown',
        provisioningActive: nfc.provisioning_active === true,
        provisioningMode: nfc.provisioning_mode || 'none',
        provisioningRemainingS: nfc.provisioning_remaining_s || 0,
        health: nfc.health || '',
        healthState: nfc.health_state || '',
      };
    }
    setText('nfcStatus', nfcHealthText(nfc));
    if (nfc && nfc.lockout_active) {
      const rem = (nfc.lockout_remaining_s !== undefined) ? nfc.lockout_remaining_s : '';
      setText('lockout', rem !== '' ? `Active (${rem}s)` : 'Active');
    } else if (nfc) {
      setText('lockout', 'None');
    } else {
      setText('lockout', 'Unknown');
    }

    const out = j.outputs || {};
    state.outputs = {
      hornPin: (out.horn_gpio !== undefined) ? out.horn_gpio : state.outputs.hornPin,
      lightPin: (out.light_gpio !== undefined) ? out.light_gpio : state.outputs.lightPin,
      hornActiveLow: (out.horn_active_low !== undefined) ? out.horn_active_low : state.outputs.hornActiveLow,
      lightActiveLow: (out.light_active_low !== undefined) ? out.light_active_low : state.outputs.lightActiveLow,
      hornEnabled: (out.horn_enabled_cfg !== undefined) ? out.horn_enabled_cfg : state.outputs.hornEnabled,
      lightEnabled: (out.light_enabled_cfg !== undefined) ? out.light_enabled_cfg : state.outputs.lightEnabled,
      lightPattern: out.light_pattern || state.outputs.lightPattern,
      silencedLightPattern: out.silenced_light_pattern || state.outputs.silencedLightPattern,
    };

    const stepLabel = WIZARD_STEPS.find((step) => step.id === state.lastStep);
    const setupStepText = stepLabel ? stepLabel.label : state.lastStep;
    setText('setup', state.setupRequired ? `REQUIRED (${setupStepText})` : 'COMPLETE');
    setText('admin', adminModeText());
    if (j.state === 'TRIGGERED') {
      setText('stateHelper', 'Alarm is latched until cleared by Admin.');
    } else if (j.state === 'FAULT') {
      setText('stateHelper', 'Some guarantees may be reduced. Review faults below.');
    } else {
      setText('stateHelper', '');
    }

    const wizStep = $('wizStep');
    if (wizStep && isSetupPage) {
      if (!state.wizInitialized) {
        const initialStep = stepTouched() ? wizStep.value : state.lastStep;
        setWizardStep(initialStep);
      } else {
        updateWizardActions();
      }
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
      setText('wizActionHint', state.setupRequired ? 'Save step stores current inputs. Next moves to the next step. Complete setup appears on the last step after all steps are visited.' : '');
      setHidden($('wizardCard'), !state.setupRequired);
      setHidden($('setupCompleteCard'), state.setupRequired);
      const canRerun = state.adminActive;
      setDisabled($('btnSetupRerun'), !canRerun);
      setText('setupRerunHint', canRerun ? 'Re-run setup will log changes.' : 'Admin Authenticated required to re-run setup.');
    } else {
      setHidden($('wizardCard'), !state.setupRequired);
      setText('topNote', state.setupRequired ? 'Setup required.' : '');
    }

    if (isSetupPage) {
      updateNfcProvisionState(nfc);
      updateNfcProvisionUi(nfc);
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

  async function downloadConfigRedacted() {
    setText('configExportHint', '');
    const r = await fetch('/api/config', { headers: hdrs(), cache: 'no-store' });
    if (!r.ok) {
      const msg = r.status === 403 ? 'Admin mode required.' : 'Config download failed.';
      setText('configExportHint', msg);
      return;
    }
    const text = await r.text();
    const blob = new Blob([text], { type: 'application/json' });
    const url = URL.createObjectURL(blob);
    const a = document.createElement('a');
    a.href = url;
    a.download = 'esp32-nfc-security-config-redacted.json';
    document.body.appendChild(a);
    a.click();
    a.remove();
    URL.revokeObjectURL(url);
    setText('configExportHint', 'Config downloaded.');
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
    setWizardStep($('wizStep').value, { touched: true });
    setText('wizError', '');
  });

  on($('wizNext'), 'click', () => {
    setText('wizError', '');
    goToNextWizardStep();
  });

  on($('wizSave'), 'click', async () => {
    setText('wizError', '');
    const step = $('wizStep').value;
    const data = {};

    if (step === 'welcome') {
      data.admin_web_password = $('wiz_admin_password').value || '';
      data.admin_mode_timeout_s = parseInt($('wiz_admin_timeout').value || '600', 10) || 600;
      updateAdminPasswordFlag();
    }
    if (step === 'network') {
      data.wifi_sta_enabled = !!$('wiz_sta_enabled').checked;
      data.wifi_sta_ssid = $('wiz_sta_ssid').value || '';
      data.wifi_sta_password = $('wiz_sta_password').value || '';
      data.wifi_ap_password = $('wiz_ap_password').value || '';
      updateApPasswordFlag();
    }
    if (step === 'time') {
      data.timezone = $('wiz_timezone').value || '';
      if ($('wiz_set_time_now').checked) {
        data.rtc_set_epoch_s = Math.floor(Date.now() / 1000);
      }
    }
    if (step === 'storage') {
      if ($('wiz_sd_enabled')) data.sd_enabled = !!$('wiz_sd_enabled').checked;
      if ($('wiz_sd_cs')) data.sd_cs_gpio = parseInt($('wiz_sd_cs').value || '13', 10) || 13;
      data.sd_required = !!$('wiz_sd_required').checked;
      data.log_retention_days = parseInt($('wiz_log_retention_days').value || '365', 10) || 365;
    }
    if (step === 'sensors') {
      data.motion_enabled = !!$('wiz_motion_enabled').checked;
      data.door_enabled = !!$('wiz_door_enabled').checked;
      data.nfc_interface = 'spi';
      if ($('wiz_nfc_cs')) data.nfc_spi_cs_gpio = parseInt($('wiz_nfc_cs').value || '27', 10) || 27;
      if ($('wiz_nfc_irq')) data.nfc_spi_irq_gpio = parseInt($('wiz_nfc_irq').value || '-1', 10);
      if ($('wiz_nfc_rst')) data.nfc_spi_rst_gpio = parseInt($('wiz_nfc_rst').value || '-1', 10);
      const kind = ($('wiz_motion_kind') && $('wiz_motion_kind').value) || 'gpio';
      data.motion_kind = kind;
      if (kind === 'ld2410b_uart') {
        data.motion_ld2410b_rx_gpio = parseInt($('wiz_ld2410b_rx').value || '16', 10) || 16;
        data.motion_ld2410b_tx_gpio = parseInt($('wiz_ld2410b_tx').value || '17', 10) || 17;
        data.motion_ld2410b_baud = parseInt($('wiz_ld2410b_baud').value || String(LD2410B_DEFAULT_BAUD), 10) || LD2410B_DEFAULT_BAUD;
      }
      if ($('wiz_web_enabled')) data.control_web_enabled = !!$('wiz_web_enabled').checked;
      if ($('wiz_nfc_enabled')) data.control_nfc_enabled = !!$('wiz_nfc_enabled').checked;
      updatePrimarySensorFlag();
    }
    if (step === 'outputs') {
      if ($('wiz_horn_gpio')) {
        const hornPin = parseInt($('wiz_horn_gpio').value || '-1', 10);
        data.horn_gpio = Number.isNaN(hornPin) ? -1 : hornPin;
      }
      if ($('wiz_light_gpio')) {
        const lightPin = parseInt($('wiz_light_gpio').value || '-1', 10);
        data.light_gpio = Number.isNaN(lightPin) ? -1 : lightPin;
      }
      if ($('wiz_horn_active_low')) data.horn_active_low = !!$('wiz_horn_active_low').checked;
      if ($('wiz_light_active_low')) data.light_active_low = !!$('wiz_light_active_low').checked;
      if ($('wiz_light_pattern')) data.light_pattern = $('wiz_light_pattern').value || 'steady';
      if ($('wiz_silenced_light_pattern')) data.silenced_light_pattern = $('wiz_silenced_light_pattern').value || 'steady';
      data.horn_enabled = !!$('wiz_horn_enabled').checked;
      data.light_enabled = !!$('wiz_light_enabled').checked;
    }

    const r = await jpost('/api/wizard/step', { step, data });
    if (!r.ok) {
      const err = r.json && r.json.error;
      if (err === 'save_failed') {
        setText('wizError', saveFailedMessage(r.json && r.json.detail));
      } else {
        setText('wizError', `Error: ${err || r.status}`);
      }
      return;
    }
    await refreshStatus();
    await refreshEvents();
  });

  on($('wizComplete'), 'click', async () => {
    setText('wizError', '');
    const r = await jpost('/api/wizard/complete', {});
    if (!r.ok) {
      const err = r.json && r.json.error;
      if (err === 'save_failed') {
        setText('wizError', saveFailedMessage(r.json && r.json.detail));
      } else {
        setText('wizError', `Error: ${err || r.status}`);
      }
      return;
    }
    window.location = '/';
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
    const pwField = $('adminPassword');
    if (pwField) pwField.value = '';
    await refreshStatus();
    await refreshEvents();
  });

  on($('btnAdminLogout'), 'click', async () => {
    setText('adminError', '');
    await jpost('/api/admin/logout', {});
    state.adminToken = '';
    await refreshStatus();
    await refreshEvents();
  });

  on($('btnConfigExport'), 'click', async () => {
    await downloadConfigRedacted();
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
        setText('otaResult', msg || 'Rebooting now. Device will disconnect. Reconnect to the device Wi-Fi.');
        setDisabled($('btnOtaUpload'), true);
        setDisabled($('otaFile'), true);
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
      const err = r.json && r.json.error;
      if (r.status === 403 && err === 'admin_token_invalid') {
        clearAdminSession();
        setText('adminError', 'Admin session expired. Log in again.');
        setText('setupRerunHint', 'Admin session expired. Log in again.');
        return;
      }
      if (r.status === 403 && err === 'admin_required') {
        clearAdminSession();
        setText('adminError', 'Admin Authenticated required.');
        setText('setupRerunHint', 'Admin Authenticated required.');
        return;
      }
      const msg = `Error: ${err || r.status}`;
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
        setText('factoryError', 'Factory restore complete. Device will disconnect. Reconnect to the device Wi-Fi.');
        setDisabled(btn, true);
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
