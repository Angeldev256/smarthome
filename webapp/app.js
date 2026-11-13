/* =====================================================================
 *  Smart Estate web app
 *  Three ways to reach the estate (chosen in Settings):
 *    local – REST API on the ESP32 (http://smartestate.local/api/...)
 *    mqtt  – online, through an MQTT broker over secure WebSockets
 *    demo  – a simulated estate running the same automation as the firmware
 *  Plus browser voice commands (Web Speech API).
 * ===================================================================== */
(() => {
  'use strict';

  const $ = (s, el = document) => el.querySelector(s);
  const $$ = (s, el = document) => [...el.querySelectorAll(s)];

  // ------------------------------------------------------------------
  //  Settings (remembered in this browser only)
  // ------------------------------------------------------------------
  const DEFAULTS = { mode: '', host: '', broker: 'wss://broker.hivemq.com:8884/mqtt', home: '', user: '', pass: '' };
  function loadCfg() {
    try { return { ...DEFAULTS, ...JSON.parse(localStorage.getItem('smartEstateCfg') || '{}') }; }
    catch { return { ...DEFAULTS }; }
  }
  function saveCfg(c) { try { localStorage.setItem('smartEstateCfg', JSON.stringify(c)); } catch {} }
  let cfg = loadCfg();

  // ------------------------------------------------------------------
  //  UI state + rendering
  // ------------------------------------------------------------------
  let state = {};
  const logEl = $('#log');
  let lastEvent = '';

  function addLog(msg) {
    const li = document.createElement('li');
    const t = document.createElement('time');
    t.textContent = new Date().toLocaleTimeString([], { hour: '2-digit', minute: '2-digit', second: '2-digit' });
    const span = document.createElement('span');
    span.textContent = msg;
    li.append(t, span);
    logEl.prepend(li);
    while (logEl.children.length > 60) logEl.lastChild.remove();
  }

  function setConn(kind, text) {
    const pill = $('#connPill');
    pill.className = 'pill ' + kind;
    $('#connText').textContent = text;
  }

  const LIGHT_KEYS = ['living', 'bedroom', 'garage_light', 'outdoor'];
  const FAN_NAMES = ['Off', 'Low', 'Medium', 'High'];
  let dragging = null;   // slider being dragged: don't overwrite it from the state

  function render(s) {
    state = s;
    // switches + dimmers
    $$('input[data-dev]').forEach(inp => { inp.checked = !!s[inp.dataset.dev]; });
    $$('input[data-dim]').forEach(r => {
      if (dragging !== r) r.value = s[r.dataset.dim + '_level'] || r.value || 100;
    });

    // gate
    const gs = s.gate_state || (s.gate ? 'open' : 'closed');
    const pos = s.gate_pos ?? (s.gate ? 100 : 0);
    const moving = gs === 'opening' || gs === 'closing';
    $('#st-gate').textContent = gs;
    $('#st-gate-bar').style.width = pos + '%';
    $('#gate-bar').style.width = pos + '%';
    const tg = $('#tag-gate');
    tg.textContent = gs + (moving ? ` ${pos}%` : '');
    tg.className = 'tag' + (gs === 'open' ? ' on' : '');
    const f = s.features || {};
    $('#sub-gate').textContent = f.gate_motor === 'dc' ? 'DC motor, timed run' : `Motor turns ${f.gate_degrees ?? 1080}° to open`;

    // fan
    const sp = s.fan_speed ?? (s.fan ? 2 : 0);
    const fanText = s.fan_breeze ? 'Breeze' : FAN_NAMES[sp];
    $('#st-fan').textContent = fanText;
    $('#sub-fan').textContent = fanText;
    $$('#fanSeg button').forEach(b => b.classList.toggle('active', !s.fan_breeze && b.dataset.cmd === 'fan:' + sp));
    $('#fanBreeze').checked = !!s.fan_breeze;
    $('#sub-fantimer').textContent = s.fan_timer ? `Off in about ${s.fan_timer} min` : 'No timer';

    // lights
    const on = LIGHT_KEYS.filter(k => s[k]).length;
    $('#st-lights').innerHTML = on + '<small>/4</small>';

    // strip
    const col = (s.strip_color || '#00FF60').toLowerCase();
    $$('#swatches button').forEach(b => b.classList.toggle('active', !!s.strip && b.dataset.color === col));
    if (document.activeElement !== $('#stripPicker')) $('#stripPicker').value = col;
    $$('#fxSeg button').forEach(b => b.classList.toggle('active', !!s.strip && b.dataset.cmd === 'strip_effect:' + s.strip_effect));
    if (dragging !== $('#stripBright')) $('#stripBright').value = s.strip_brightness ?? 60;
    $('#stripBrightVal').textContent = (s.strip_brightness ?? '–') + '%';

    // speaker
    $('#st-vol').innerHTML = (s.volume ?? '–') + '<small>/30</small>';
    if (dragging !== $('#volume')) $('#volume').value = s.volume ?? 20;
    $('#volumeVal').textContent = s.volume ?? '–';

    // legend under the map
    $('#lg-gate').textContent = gs;
    $('#lg-fan').textContent = fanText;
    $('#lg-strip').textContent = s.strip ? (s.strip_effect === 'status' ? 'steady' : s.strip_effect || 'on') : 'off';
    $('#lg-lights').textContent = on;

    if (s.event && s.event !== lastEvent) {
      lastEvent = s.event;
      $('#st-event').textContent = s.event;
      addLog(s.event);
      pulseSpeaker();
    }
    renderMap(s);
  }

  let waveTimer;
  function pulseSpeaker() {  // the little speaker on the map "talks"
    const w = document.getElementById('m-waves');
    w.style.opacity = 1;
    clearTimeout(waveTimer);
    waveTimer = setTimeout(() => { w.style.opacity = 0; }, 1500);
  }

  function renderMap(s) {
    const op = (id, v) => { const el = document.getElementById(id); if (el) el.style.opacity = v; };
    const lvl = k => (s[k] ? (s[k + '_level'] ?? 100) / 100 : 0);
    op('m-living', s.living ? 0.15 + 0.45 * lvl('living') : 0);
    op('m-bedroom', s.bedroom ? 0.15 + 0.45 * lvl('bedroom') : 0);
    op('m-garage-glow', s.garage_light ? 0.2 + 0.45 * lvl('garage_light') : 0);
    op('m-outdoor-glow-l', s.outdoor ? 0.3 + 0.7 * lvl('outdoor') : 0);
    op('m-outdoor-glow-r', s.outdoor ? 0.3 + 0.7 * lvl('outdoor') : 0);
    $$('.m-outdoor').forEach(c => c.setAttribute('fill', s.outdoor ? '#ffd85a' : '#55605a'));

    // gate leaves swing in by position
    const gs = s.gate_state || (s.gate ? 'open' : 'closed');
    const moving = gs === 'opening' || gs === 'closing';
    const pos = (s.gate_pos ?? (s.gate ? 100 : 0)) / 100;
    document.getElementById('m-gate-l').setAttribute('transform', `rotate(${-80 * pos} 245 390)`);
    document.getElementById('m-gate-r').setAttribute('transform', `rotate(${80 * pos} 355 390)`);
    document.getElementById('m-gate-label').textContent = gs.toUpperCase();

    // fan spins faster with speed
    const sp = s.fan_speed ?? (s.fan ? 2 : 0);
    const blades = $('#m-fan .blades');
    blades.style.transformOrigin = '0 0';
    blades.style.animation = sp ? `spin ${s.fan_breeze ? 0.8 : [0, 1.2, 0.6, 0.3][sp]}s linear infinite` : 'none';
    document.getElementById('m-fan-label').textContent = s.fan_breeze ? 'fan: breeze' : sp ? `fan speed ${sp}` : 'fan off';

    // RGB strip around the walls
    const strip = document.getElementById('m-strip');
    const fx = s.strip_effect;
    strip.style.opacity = s.strip ? 0.35 + 0.65 * ((s.strip_brightness ?? 60) / 100) : 0;
    strip.style.animation = !s.strip ? 'none'
      : fx === 'rainbow' ? 'hue 4s linear infinite'
      : fx === 'party' ? 'hue .9s steps(4) infinite'
      : fx === 'breathe' ? 'breathe 2.6s ease-in-out infinite'
      : moving ? 'breathe .6s ease-in-out infinite' : 'none';
    const c = fx === 'status' && moving ? '#ff7000'
      : fx === 'rainbow' || fx === 'party' ? '#ff0000' : (s.strip_color || '#00ff60');
    document.getElementById('m-strip-line').setAttribute('stroke', c);
    document.getElementById('m-strip-glow').setAttribute('stroke', c);
  }
  const spinCss = document.createElement('style');
  spinCss.textContent = '@keyframes spin{to{transform:rotate(360deg)}}' +
    '@keyframes hue{to{filter:hue-rotate(360deg)}}@keyframes breathe{50%{opacity:.25}}';
  document.head.append(spinCss);

  // ------------------------------------------------------------------
  //  Transports
  // ------------------------------------------------------------------
  let transport = null;

  // ---- local REST -------------------------------------------------------
  function LocalTransport(host) {
    const base = (host || '').replace(/\/+$/, '');
    let timer = null, fails = 0;
    async function poll() {
      try {
        const r = await fetch(base + '/api/state', { cache: 'no-store' });
        if (!r.ok) throw new Error(r.status);
        render(await r.json());
        fails = 0;
        setConn('ok', 'Local');
      } catch {
        if (++fails > 2) setConn('bad', 'ESP32 offline');
      }
    }
    return {
      start() { poll(); timer = setInterval(poll, 1000); },
      stop() { clearInterval(timer); },
      async send(device, value) {
        const r = await fetch(`${base}/api/cmd?device=${encodeURIComponent(device)}&value=${encodeURIComponent(value)}`, { method: 'POST' });
        if (r.ok) render(await r.json());
        return r.ok;
      }
    };
  }

  // ---- MQTT (online) ----------------------------------------------------
  function loadScript(src) {
    return new Promise((res, rej) => {
      if (window.mqtt) return res();
      const s = document.createElement('script');
      s.src = src; s.onload = res; s.onerror = rej;
      document.head.append(s);
    });
  }
  function MqttTransport(c) {
    let client = null;
    const base = `smartestate/${c.home}`;
    return {
      async start() {
        if (!c.home) { setConn('bad', 'Set a Home ID'); openSettings(); return; }
        setConn('demo', 'Connecting…');
        try { await loadScript('https://unpkg.com/mqtt@5.10.1/dist/mqtt.min.js'); }
        catch { setConn('bad', 'Offline'); addLog('Could not load the MQTT library (no internet?)'); return; }
        client = window.mqtt.connect(c.broker, {
          username: c.user || undefined, password: c.pass || undefined,
          clientId: 'estate-web-' + Math.random().toString(16).slice(2, 10), reconnectPeriod: 3000
        });
        client.on('connect', () => {
          setConn('demo', 'Broker ok · waiting for ESP32');
          client.subscribe([`${base}/state`, `${base}/online`]);
          client.publish(`${base}/cmd`, JSON.stringify({ device: 'ping', value: '1' }));
        });
        client.on('message', (topic, payload) => {
          const text = payload.toString();
          if (topic.endsWith('/online')) {
            setConn(text === '1' ? 'ok' : 'bad', text === '1' ? 'Online' : 'ESP32 offline');
            return;
          }
          try { render(JSON.parse(text)); setConn('ok', 'Online'); } catch {}
        });
        client.on('error', e => { setConn('bad', 'Broker error'); addLog('MQTT: ' + e.message); });
        client.on('offline', () => setConn('bad', 'Broker offline'));
      },
      stop() { if (client) client.end(true); },
      async send(device, value) {
        if (!client || !client.connected) return false;
        client.publish(`${base}/cmd`, JSON.stringify({ device, value: String(value) }));
        return true;
      }
    };
  }

  // ---- Demo: a simulated estate that follows the firmware's rules -------
  function DemoTransport() {
    const S = {
      living: false, bedroom: false, garage_light: false, outdoor: false,
      living_level: 0, bedroom_level: 0, garage_light_level: 0, outdoor_level: 0,
      fan: false, fan_speed: 0, fan_breeze: false, fan_timer: 0,
      gate: false, gate_state: 'closed', gate_pos: 0,
      strip: true, strip_color: '#00FF60', strip_effect: 'status', strip_brightness: 60,
      volume: 22, announce: true, event: 'Demo estate ready',
      features: { speaker: true, strip: true, strip_type: 'analog', gate_motor: 'stepper', gate_degrees: 1080, alexa: true }
    };
    const last = { living: 100, bedroom: 100, garage_light: 100, outdoor: 100 };
    const NAMES = { living: 'Living room light', bedroom: 'Bedroom light', garage_light: 'Garage light', outdoor: 'Outdoor light' };
    const COLORS = { red: '#FF0000', green: '#00FF40', blue: '#0030FF', white: '#FFFFFF', warm: '#FF9A40', 'warm white': '#FF9A40',
      yellow: '#FFD000', orange: '#FF6000', purple: '#9000FF', violet: '#9000FF', pink: '#FF2080', cyan: '#00FFFF',
      aqua: '#00FFC0', gold: '#FFB000', magenta: '#FF00FF' };
    const FX = ['status', 'breathe', 'rainbow', 'party'];
    let gatePos = 0, gateTarget = 0, fanOffAt = 0, lastFan = 2, timer, lastTick = Date.now();
    const ev = m => { S.event = m; };
    const isNum = v => /^\d+$/.test(v);
    const parse = (v, cur) => {
      if (v === 'toggle') return !cur;
      if (['on', '1', 'true', 'open', 'yes', 'start'].includes(v)) return true;
      if (['off', '0', 'false', 'close', 'closed', 'shut', 'no', 'stop'].includes(v)) return false;
      return null;
    };
    function setLight(k, level) {
      level = Math.max(0, Math.min(100, level));
      const was = S[k];
      S[k + '_level'] = level; S[k] = level > 0;
      if (level) last[k] = level;
      ev(was !== S[k] ? `${NAMES[k]} ${level ? 'ON' : 'OFF'}` : `${NAMES[k]} ${level}%`);
    }
    function setFan(sp, breeze = false) {
      sp = Math.max(0, Math.min(3, sp));
      if (!sp) { breeze = false; fanOffAt = 0; }
      S.fan_speed = sp; S.fan = sp > 0; S.fan_breeze = breeze;
      if (sp) lastFan = sp;
      ev(breeze ? 'Fan breeze mode' : sp ? `Fan speed ${sp}` : 'Fan OFF');
    }
    const gateWantsOpen = () => gateTarget === 100 || (gateTarget > gatePos && gatePos !== gateTarget);
    function setGate(open) {
      const t = open ? 100 : 0;
      if (gateTarget === t) return;
      gateTarget = t;
      ev(open ? 'Gate opening' : 'Gate closing');
    }
    const strip = o => Object.assign(S, { strip: true }, o);
    function scene(n) {
      n = n.replace(/[ _]/g, '');
      if (n === 'goodnight' || n === 'night') {
        LIGHT_KEYS.forEach(k => setLight(k, 0)); setGate(false); setFan(1);
        strip({ strip_effect: 'status', strip_color: '#0030FF', strip_brightness: 10 }); ev('Scene: good night');
      } else if (n === 'welcome' || n === 'welcomehome' || n === 'home') {
        setGate(true); setLight('living', 100); setLight('outdoor', 100);
        strip({ strip_effect: 'status', strip_color: '#00FF60', strip_brightness: 60 }); ev('Scene: welcome home');
      } else if (n === 'party') {
        LIGHT_KEYS.forEach(k => setLight(k, last[k])); setFan(2);
        strip({ strip_effect: 'party', strip_brightness: 100 }); ev('Scene: party');
      } else if (n === 'normal' || n === 'partyoff') {
        strip({ strip_effect: 'status', strip_color: '#00FF60', strip_brightness: 60 }); ev('Scene: normal');
      }
    }

    function apply(device, value) {
      device = String(device).toLowerCase().trim().replace(/ /g, '_');
      const v = String(value).toLowerCase().trim();
      if (LIGHT_KEYS.includes(device)) {
        if (isNum(v)) { setLight(device, +v); return true; }
        const on = parse(v, S[device]); if (on == null) return false;
        setLight(device, on ? last[device] : 0); return true;
      }
      if (device === 'all_lights' || device === 'lights') {
        const on = parse(v, LIGHT_KEYS.some(k => S[k])); if (on == null) return false;
        LIGHT_KEYS.forEach(k => setLight(k, on ? last[k] : 0));
        ev(on ? 'All lights ON' : 'All lights OFF'); return true;
      }
      if (device === 'gate') {
        if (v === 'stop') { if (gateTarget !== gatePos) { gateTarget = gatePos; ev('Gate stopped'); } return true; }
        const on = parse(v, gateWantsOpen()); if (on == null) return false;
        setGate(on); return true;
      }
      if (device === 'fan') {
        if (v === 'next' || v === 'cycle') { setFan((S.fan_speed + 1) % 4); return true; }
        if (['breeze', 'natural', 'wind'].includes(v)) { setFan(S.fan_speed || 2, true); return true; }
        const named = { low: 1, slow: 1, medium: 2, mid: 2, high: 3, fast: 3, max: 3 };
        if (v in named) { setFan(named[v]); return true; }
        if (v === 'faster' || v === 'up') { setFan(Math.min(S.fan_speed + 1, 3)); return true; }
        if (v === 'slower' || v === 'down') { setFan(Math.max(S.fan_speed - 1, 0)); return true; }
        if (isNum(v)) { setFan(+v); return true; }
        const on = parse(v, S.fan); if (on == null) return false;
        setFan(on ? lastFan : 0); return true;
      }
      if (device === 'fan_timer') {
        const m = parseInt(v, 10) || 0;
        if (m <= 0) { fanOffAt = 0; ev('Fan timer cancelled'); return true; }
        if (!S.fan_speed) setFan(lastFan);
        fanOffAt = Date.now() + m * 60000; ev(`Fan switches off in ${m} min`); return true;
      }
      if (device.startsWith('strip')) {
        if (device === 'strip_brightness' || (device === 'strip' && isNum(v))) {
          const b = Math.max(0, Math.min(100, +v));
          S.strip = b > 0; if (b) S.strip_brightness = b;
          ev(`Strip brightness ${b}%`); return true;
        }
        if (v === 'next') { S.strip_effect = FX[(FX.indexOf(S.strip_effect) + (S.strip ? 1 : 0)) % 4]; S.strip = true; ev('Strip effect: ' + S.strip_effect); return true; }
        if (FX.includes(v) || v === 'solid') { S.strip_effect = v === 'solid' ? 'status' : v; S.strip = true; ev('Strip effect: ' + S.strip_effect); return true; }
        const hex = v.replace('#', '');
        const col = COLORS[v] || (/^[0-9a-f]{6}$/.test(hex) ? '#' + hex.toUpperCase() : null);
        if (col) { S.strip_color = col; S.strip = true; if (S.strip_effect !== 'breathe') S.strip_effect = 'status'; ev('Strip colour ' + col); return true; }
        const on = parse(v, S.strip); if (on == null) return false;
        S.strip = on; ev(on ? 'Strip lights ON' : 'Strip lights OFF'); return true;
      }
      if (device === 'volume') {
        const n = v === 'up' || v === 'louder' ? S.volume + 4
          : v === 'down' || v === 'quieter' || v === 'softer' ? S.volume - 4
          : v === 'mute' || v === 'off' ? 0 : v === 'unmute' || v === 'on' ? 22 : isNum(v) ? +v : null;
        if (n == null) return false;
        S.volume = Math.max(0, Math.min(30, n)); ev('Speaker volume ' + S.volume); return true;
      }
      if (device === 'announce') {
        const on = parse(v, S.announce); if (on == null) return false;
        S.announce = on; ev(on ? 'Announcements ON' : 'Announcements OFF'); return true;
      }
      if (device === 'say') { ev(`Speaker plays “${v}”`); return true; }
      if (device === 'beep') { ev('Beep!'); return true; }
      if (device === 'doorbell') { ev('Ding dong! Someone is at the gate'); return true; }
      if (device === 'scene') { scene(v); return true; }
      if (['goodnight', 'welcome', 'party'].includes(device)) {
        const on = parse(v, false);
        if (device === 'party' && on === false) { scene('normal'); return true; }
        if (on !== false) scene(device);
        return true;
      }
      return false;
    }

    function tick() {
      const now = Date.now(), dt = now - lastTick;
      lastTick = now;
      // the gate takes ~12 s, like the stepper turning 1080° at 12 rpm
      if (gatePos !== gateTarget) {
        const step = dt / 120;
        gatePos = gateTarget > gatePos ? Math.min(gateTarget, gatePos + step) : Math.max(gateTarget, gatePos - step);
        if (gatePos === gateTarget && (gateTarget === 0 || gateTarget === 100)) ev(gateTarget ? 'Gate OPEN' : 'Gate CLOSED');
      }
      S.gate_state = gatePos !== gateTarget ? (gateTarget > gatePos ? 'opening' : 'closing')
        : gatePos >= 100 ? 'open' : gatePos <= 0 ? 'closed' : 'stopped';
      S.gate_pos = Math.round(gatePos);
      S.gate = gateWantsOpen();
      if (fanOffAt && now >= fanOffAt) { setFan(0); ev('Fan timer finished – fan OFF'); }
      S.fan_timer = fanOffAt ? Math.ceil((fanOffAt - now) / 60000) : 0;
      render({ ...S });
    }

    return {
      start() { setConn('demo', 'Demo'); tick(); timer = setInterval(tick, 250); },
      stop() { clearInterval(timer); },
      async send(device, value) { const ok = apply(device, value); tick(); return ok; }
    };
  }

  // ------------------------------------------------------------------
  //  Start the right transport
  // ------------------------------------------------------------------
  async function detectMode() {
    if (cfg.mode) return cfg.mode;
    // opened from the ESP32 itself? then go local automatically
    if (location.protocol === 'http:' && location.hostname) {
      try {
        const r = await fetch('/api/state', { cache: 'no-store' });
        if (r.ok) return 'local';
      } catch {}
    }
    return 'demo';
  }

  async function startTransport() {
    if (transport) transport.stop();
    const mode = await detectMode();
    document.body.classList.toggle('mode-demo', mode === 'demo');
    transport = mode === 'local' ? LocalTransport(cfg.host)
              : mode === 'mqtt' ? MqttTransport(cfg)
              : DemoTransport();
    transport.mode = mode;
    transport.start();
  }

  async function send(device, value) {
    if (!transport) return false;
    try {
      const ok = await transport.send(device, value);
      if (!ok) addLog(`Could not send "${device} ${value}"`);
      return ok;
    } catch (e) { addLog('Send failed: ' + e.message); return false; }
  }

  // ------------------------------------------------------------------
  //  Controls
  // ------------------------------------------------------------------
  $$('input[data-dev]').forEach(inp => {
    inp.addEventListener('change', () => send(inp.dataset.dev, inp.checked ? 'on' : 'off'));
  });
  $$('[data-cmd]').forEach(btn => {
    btn.addEventListener('click', () => { const [d, v] = btn.dataset.cmd.split(':'); send(d, v); });
  });
  // sliders send when released (and every 300 ms while dragging)
  function slider(el, onSend, onShow) {
    let t = 0;
    el.addEventListener('pointerdown', () => { dragging = el; });
    el.addEventListener('pointerup', () => setTimeout(() => { if (dragging === el) dragging = null; }, 400));
    el.addEventListener('input', () => {
      dragging = el; if (onShow) onShow(el.value);
      const now = Date.now(); if (now - t > 300) { t = now; onSend(el.value); }
    });
    el.addEventListener('change', () => { onSend(el.value); dragging = null; });
  }
  $$('input[data-dim]').forEach(r => slider(r, v => send(r.dataset.dim, v)));
  slider($('#stripBright'), v => send('strip_brightness', v), v => { $('#stripBrightVal').textContent = v + '%'; });
  slider($('#volume'), v => send('volume', v), v => { $('#volumeVal').textContent = v; });
  $$('#swatches button').forEach(b => b.addEventListener('click', () => send('strip_color', b.dataset.color)));
  $('#stripPicker').addEventListener('change', e => send('strip_color', e.target.value));
  $('#fanBreeze').addEventListener('change', e => send('fan', e.target.checked ? 'breeze' : String(state.fan_speed || 2)));
  $('#fanTimer').addEventListener('change', e => send('fan_timer', e.target.value));

  // theme
  $('#themeBtn').addEventListener('click', () => {
    const root = document.documentElement;
    const dark = root.dataset.theme ? root.dataset.theme === 'dark' : matchMedia('(prefers-color-scheme: dark)').matches;
    root.dataset.theme = dark ? 'light' : 'dark';
    try { localStorage.setItem('smartEstateTheme', root.dataset.theme); } catch {}
  });
  try { const t = localStorage.getItem('smartEstateTheme'); if (t) document.documentElement.dataset.theme = t; } catch {}

  // settings dialog
  const dlg = $('#settings');
  function showFields() {
    const m = $('#cfgMode').value;
    $$('[data-for]', dlg).forEach(f => { f.hidden = f.dataset.for !== m; });
  }
  function openSettings() {
    $('#cfgMode').value = transport?.mode || cfg.mode || 'demo';
    $('#cfgHost').value = cfg.host; $('#cfgBroker').value = cfg.broker; $('#cfgHome').value = cfg.home;
    $('#cfgUser').value = cfg.user; $('#cfgPass').value = cfg.pass;
    showFields();
    if (!dlg.open) dlg.showModal();
  }
  $('#cfgMode').addEventListener('change', showFields);
  $('#settingsBtn').addEventListener('click', openSettings);
  dlg.addEventListener('close', () => {
    if (dlg.returnValue !== 'save') return;
    cfg = { mode: $('#cfgMode').value, host: $('#cfgHost').value.trim(), broker: $('#cfgBroker').value.trim() || DEFAULTS.broker,
            home: $('#cfgHome').value.trim(), user: $('#cfgUser').value.trim(), pass: $('#cfgPass').value };
    saveCfg(cfg);
    addLog('Switched to ' + cfg.mode + ' mode');
    startTransport();
  });

  // ------------------------------------------------------------------
  //  Voice commands
  // ------------------------------------------------------------------
  const LIGHT_WORDS = [
    { dev: 'living', words: ['living room', 'living', 'lounge', 'sitting room', 'parlour', 'parlor'] },
    { dev: 'bedroom', words: ['bedroom', 'bed room'] },
    { dev: 'garage_light', words: ['garage'] },
    { dev: 'outdoor', words: ['outdoor', 'outside', 'compound', 'porch', 'security light', 'gate light', 'pillar'] }
  ];
  const LABEL = { living: 'living room light', bedroom: 'bedroom light', garage_light: 'garage light', outdoor: 'outdoor light' };
  const COLOR_WORDS = ['warm white', 'red', 'green', 'blue', 'white', 'warm', 'yellow', 'orange', 'purple', 'violet', 'pink', 'cyan', 'aqua', 'gold', 'magenta'];
  const NUM_WORDS = { zero: 0, one: 1, two: 2, to: 2, too: 2, three: 3, four: 4, five: 5, ten: 10, fifteen: 15, twenty: 20, thirty: 30, forty: 40, fifty: 50, sixty: 60 };

  function interpret(raw) {
    let t = ' ' + raw.toLowerCase().replace(/%/g, ' percent').replace(/[^a-z0-9' ]/g, ' ').replace(/\s+/g, ' ') + ' ';
    const num = () => { const m = t.match(/\b(\d+)\b/); if (m) return +m[1]; for (const w in NUM_WORDS) if (t.includes(' ' + w + ' ')) return NUM_WORDS[w]; return null; };
    const OFF = /\b(turn off|switch off|off|disable|deactivate|kill|shut down)\b/;
    const ON = /\b(turn on|switch on|on|light up|start|enable|activate)\b/;

    // ---- scenes and questions --------------------------------------
    if (/good ?night|bed ?time|sleep mode|going to sleep/.test(t)) return { cmds: [['scene', 'goodnight']], say: 'Good night. Lights off, gate closing, fan on low.' };
    if (/i'?m home|welcome home|good morning|arrived/.test(t)) return { cmds: [['scene', 'welcome']], say: 'Welcome home. Gate opening, lights on.' };
    if (/party/.test(t)) return OFF.test(t) ? { cmds: [['party', 'off']], say: 'Party is over.' } : { cmds: [['scene', 'party']], say: "Party mode. Let's go!" };
    if (/door ?bell|ring|ding dong|visitor/.test(t)) return { cmds: [['doorbell', 'ring']], say: 'Ding dong.' };
    if (/status|report|how is the (house|estate)|what is on/.test(t)) {
      const on = LIGHT_KEYS.filter(k => state[k]).map(k => LABEL[k]);
      return { say: `The gate is ${state.gate_state || 'closed'}. The fan is ${state.fan_speed ? 'on speed ' + state.fan_speed : 'off'}. ` +
        (on.length ? `Lights on: ${on.join(', ')}.` : 'All lights are off.') };
    }

    // ---- speaker ----------------------------------------------------
    if (/volume|louder|quieter|softer|mute|speaker/.test(t)) {
      if (/unmute/.test(t)) return { cmds: [['volume', 'unmute']], say: 'Speaker on.' };
      if (/mute|silence/.test(t)) return { cmds: [['volume', 'mute']], say: 'Speaker muted.' };
      if (/up|louder|increase|raise/.test(t)) return { cmds: [['volume', 'up']], say: 'Louder.' };
      if (/down|quieter|softer|lower|decrease/.test(t)) return { cmds: [['volume', 'down']], say: 'Quieter.' };
      const n = num(); if (n != null) return { cmds: [['volume', String(Math.min(30, n))]], say: `Volume ${Math.min(30, n)}.` };
      if (/hello|say|talk|test/.test(t)) return { cmds: [['say', 'hello']], say: 'Testing the speaker.' };
    }
    if (/announce/.test(t)) return { cmds: [['announce', OFF.test(t) ? 'off' : 'on']], say: OFF.test(t) ? 'Announcements off.' : 'Announcements on.' };
    if (/\b(say|speak) hello\b|introduce yourself/.test(t)) return { cmds: [['say', 'hello']], say: 'Saying hello on the estate speaker.' };

    // ---- gate --------------------------------------------------------
    if (/\bgate\b/.test(t) && !/gate light/.test(t)) {
      if (/\b(stop|halt|pause|freeze)\b/.test(t)) return { cmds: [['gate', 'stop']], say: 'Stopping the gate.' };
      if (/\b(close|shut|lock)\b/.test(t) || OFF.test(t)) return { cmds: [['gate', 'close']], say: 'Closing the gate.' };
      if (/\b(open|unlock)\b/.test(t) || ON.test(t)) return { cmds: [['gate', 'open']], say: 'Opening the gate.' };
      return { say: state.gate_state ? `The gate is ${state.gate_state}.` : 'Should I open or close the gate?' };
    }

    // ---- fan ---------------------------------------------------------
    if (/\b(fan|breeze|wind)\b/.test(t)) {
      const mins = t.match(/\bin (\d+|\w+) (minute|minutes|min|hour|hours)\b/);
      if (mins) {
        let m = /^\d+$/.test(mins[1]) ? +mins[1] : (NUM_WORDS[mins[1]] ?? (mins[1] === 'an' || mins[1] === 'a' ? 1 : 0));
        if (/hour/.test(mins[2])) m *= 60;
        if (m > 0) return { cmds: [['fan_timer', String(m)]], say: `The fan will switch off in ${m} minutes.` };
      }
      if (/cancel.*timer|timer.*(off|cancel)/.test(t)) return { cmds: [['fan_timer', '0']], say: 'Fan timer cancelled.' };
      if (/breeze|natural|wind/.test(t)) return { cmds: [['fan', 'breeze']], say: 'Fan breeze mode.' };
      if (/\b(high|fast|full|max|maximum)\b/.test(t)) return { cmds: [['fan', 'high']], say: 'Fan on high.' };
      if (/\b(medium|mid|middle)\b/.test(t)) return { cmds: [['fan', 'medium']], say: 'Fan on medium.' };
      if (/\b(low|slow|gentle)\b/.test(t) && !/\bslower\b/.test(t)) return { cmds: [['fan', 'low']], say: 'Fan on low.' };
      if (/faster|speed up|increase|more/.test(t)) return { cmds: [['fan', 'faster']], say: 'Fan faster.' };
      if (/slower|slow down|decrease|less/.test(t)) return { cmds: [['fan', 'slower']], say: 'Fan slower.' };
      if (/speed|level|number/.test(t)) { const n = num(); if (n != null && n <= 3) return { cmds: [['fan', String(n)]], say: n ? `Fan speed ${n}.` : 'Fan off.' }; }
      if (OFF.test(t)) return { cmds: [['fan', 'off']], say: 'Turning the fan off.' };
      if (ON.test(t)) return { cmds: [['fan', 'on']], say: 'Turning the fan on.' };
      return { cmds: [['fan', 'next']], say: 'Next fan speed.' };
    }

    // ---- RGB strip ----------------------------------------------------
    const color = COLOR_WORDS.find(c => t.includes(' ' + c + ' '));
    const isStrip = /\b(strip|rgb|led strip|wall light|wall lights|colou?r|rainbow|breathe|breathing)\b/.test(t);
    if (isStrip || (color && !LIGHT_WORDS.some(l => l.words.some(w => t.includes(' ' + w + ' '))))) {
      if (/rainbow/.test(t)) return { cmds: [['strip_effect', 'rainbow']], say: 'Rainbow on the strip.' };
      if (/breath/.test(t)) return { cmds: [['strip_effect', 'breathe']], say: 'Breathing effect.' };
      if (/steady|solid|normal|status/.test(t)) return { cmds: [['strip_effect', 'status']], say: 'Steady strip light.' };
      if (color) return { cmds: [['strip_color', color]], say: `Strip lights ${color}.` };
      if (/bright|dim|percent/.test(t)) { const n = num(); if (n != null) return { cmds: [['strip_brightness', String(n)]], say: `Strip at ${n} percent.` }; }
      if (/next|change|another/.test(t)) return { cmds: [['strip', 'next']], say: 'Next strip effect.' };
      if (OFF.test(t)) return { cmds: [['strip', 'off']], say: 'Strip lights off.' };
      return { cmds: [['strip', 'on']], say: 'Strip lights on.' };
    }

    // ---- building lights ---------------------------------------------
    let target = null;
    for (const l of LIGHT_WORDS) if (l.words.some(w => t.includes(' ' + w + ' '))) { target = l.dev; break; }
    if (!target && /\b(all|every|everything)\b/.test(t) && /\blights?\b/.test(t)) target = 'all_lights';
    if (!target && /\blights?\b/.test(t)) target = 'all_lights';
    if (!target) return { say: "Sorry, I didn't catch that. Try “open the gate” or “fan speed two”." };
    const name = target === 'all_lights' ? 'all the lights' : 'the ' + LABEL[target];
    if (target !== 'all_lights' && /\b(dim|brightness|percent|bright|brighter|dimmer)\b/.test(t)) {
      const n = num();
      if (n != null) return { cmds: [[target, String(Math.max(5, Math.min(100, n)))]], say: `Setting ${name} to ${n} percent.` };
      if (/brighter|full/.test(t)) return { cmds: [[target, '100']], say: `${name} to full brightness.` };
      return { cmds: [[target, '30']], say: `Dimming ${name}.` };
    }
    if (OFF.test(t)) return { cmds: [[target, 'off']], say: `Turning off ${name}.` };
    if (ON.test(t) || /\b(switch|turn)\b/.test(t)) return { cmds: [[target, 'on']], say: `Turning on ${name}.` };
    if (/toggle/.test(t)) return { cmds: [[target, 'toggle']], say: `Toggling ${name}.` };
    return { say: `Should I turn ${name} on or off?` };
  }

  function speak(text) {
    $('#reply').textContent = text;
    if ('speechSynthesis' in window) {
      speechSynthesis.cancel();
      const u = new SpeechSynthesisUtterance(text);
      u.rate = 1.05;
      speechSynthesis.speak(u);
    }
  }

  async function runPhrase(text) {
    $('#transcript').textContent = '“' + text + '”';
    addLog('Voice: ' + text);
    const r = interpret(text);
    if (r.cmds) for (const [d, v] of r.cmds) await send(d, v);
    if (r.say) speak(r.say);
  }

  $('#textCmd').addEventListener('submit', e => {
    e.preventDefault();
    const v = $('#textCmdInput').value.trim();
    if (v) { runPhrase(v); $('#textCmdInput').value = ''; }
  });

  const SR = window.SpeechRecognition || window.webkitSpeechRecognition;
  const mic = $('#micBtn');
  if (!SR) {
    mic.disabled = true;
    mic.title = 'Voice needs Chrome, Edge or Safari';
    $('#transcript').textContent = 'Voice recognition needs Chrome, Edge or Safari – you can still type commands below.';
  } else {
    const rec = new SR();
    rec.lang = 'en-US';
    rec.interimResults = true;
    rec.maxAlternatives = 1;
    let listening = false;
    rec.onstart = () => { listening = true; mic.classList.add('listening'); $('#transcript').textContent = 'Listening…'; };
    rec.onend = () => { listening = false; mic.classList.remove('listening'); };
    rec.onerror = e => {
      $('#transcript').textContent = e.error === 'not-allowed'
        ? 'Microphone blocked. Voice needs https (GitHub Pages) or localhost – on the ESP32 page, type commands instead.'
        : 'Voice error: ' + e.error;
    };
    rec.onresult = e => {
      const res = e.results[e.results.length - 1];
      const text = res[0].transcript.trim();
      $('#transcript').textContent = '“' + text + '”';
      if (res.isFinal) runPhrase(text);
    };
    mic.addEventListener('click', () => { if (listening) rec.stop(); else { try { rec.start(); } catch {} } });
  }

  // expose for debugging in the console
  window.smartEstate = { send, interpret, get state() { return state; } };

  startTransport();
})();
