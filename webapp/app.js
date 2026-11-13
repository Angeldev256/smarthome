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

  const yesNo = (v, y, n) => (v ? y : n);

  function render(s) {
    state = s;
    // toggles
    $$('input[data-dev]').forEach(inp => { inp.checked = !!s[inp.dataset.dev]; });

    // stats
    $('#st-temp').innerHTML = (s.temp == null ? '–' : Number(s.temp).toFixed(1)) + '<small>°C</small>';
    $('#st-hum').innerHTML = (s.hum == null ? '–' : Math.round(s.hum)) + '<small>%</small>';
    $('#st-rain').textContent = s.raining ? `Rain ${s.rain_level ?? ''}%` : 'Dry';
    $('#st-rain-tile').classList.toggle('alert', !!s.raining);
    $('#st-motion').textContent = yesNo(s.motion, 'Detected', 'None');
    $('#st-motion-tile').classList.toggle('alert', !!(s.motion && s.security));
    $('#st-cargate').textContent = yesNo(s.car_gate, 'Yes', 'No');
    $('#st-door').textContent = yesNo(s.door_open, 'Open', 'Closed');
    $('#st-door-tile').classList.toggle('alert', !!(s.door_open && s.security));
    $('#sub-garage').textContent = s.car_garage ? 'Car parked inside' : 'Garage empty';

    const tagAlarm = $('#tag-alarm');
    tagAlarm.textContent = s.alarm ? 'SOUNDING' : 'Quiet';
    tagAlarm.className = 'tag' + (s.alarm ? ' alert' : '');
    const tagDoor = $('#tag-door');
    tagDoor.textContent = s.door_open ? 'Open' : 'Closed';
    tagDoor.className = 'tag' + (s.door_open ? ' alert' : ' on');
    $('#alarmStop').hidden = !s.alarm;
    if (!s.alarm) $('#st-event').classList.remove('alert');

    $('#lg-gate').textContent = s.gate ? 'Open' : 'Closed';
    $('#lg-garage').textContent = s.garage_door ? 'Open' : 'Closed';
    $('#lg-cover').textContent = s.cover ? 'Covering' : 'Rolled back';
    $('#lg-sec').textContent = s.security ? 'Armed' : 'Off';

    if (s.event && s.event !== lastEvent) {
      lastEvent = s.event;
      $('#st-event').textContent = s.event;
      $('#st-event').classList.toggle('alert', !!s.alarm);
      addLog(s.event);
    }
    renderMap(s);
  }

  function renderMap(s) {
    const op = (id, v) => { const el = document.getElementById(id); if (el) el.style.opacity = v; };
    op('m-living', s.living ? 0.55 : 0);
    op('m-bedroom', s.bedroom ? 0.55 : 0);
    op('m-garage-glow', s.garage_light ? 0.6 : 0);
    op('m-outdoor-glow', s.outdoor ? 1 : 0);
    document.getElementById('m-outdoor').setAttribute('fill', s.outdoor ? '#ffd85a' : '#55605a');
    op('m-cover', s.cover ? 0.95 : 0.15);
    op('m-rain', s.raining ? 1 : 0);
    op('m-motion', s.motion ? 1 : 0);
    op('m-car-gate', s.car_gate ? 1 : 0);
    op('m-car-garage', s.car_garage ? 1 : 0);
    op('m-alarm', s.alarm ? 1 : 0);
    // gate leaves swing inwards
    document.getElementById('m-gate-l').setAttribute('transform', s.gate ? 'rotate(-80 245 390)' : '');
    document.getElementById('m-gate-r').setAttribute('transform', s.gate ? 'rotate(80 355 390)' : '');
    // garage door rolls up
    const gd = document.getElementById('m-garage-door');
    gd.setAttribute('height', s.garage_door ? 3 : 12);
    gd.setAttribute('y', s.garage_door ? 148 : 148);
    // door opening
    document.getElementById('m-door').setAttribute('fill', s.door_open ? '#ff6b63' : '#6e5234');
    // fan spin
    const blades = $('#m-fan .blades');
    blades.style.transformOrigin = '0 0';
    blades.style.animation = s.fan ? 'spin .6s linear infinite' : 'none';
  }
  const spinCss = document.createElement('style');
  spinCss.textContent = '@keyframes spin{to{transform:rotate(360deg)}}';
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

  // ---- Demo: same automation rules as the firmware ----------------------
  function DemoTransport() {
    const S = {
      living: false, bedroom: false, garage_light: false, outdoor: false, fan: false,
      gate: false, garage_door: false, cover: false, security: false, auto: true, alarm: false,
      temp: 26, hum: 58, motion: false, car_gate: false, car_garage: false, door_open: false,
      raining: false, rain_level: 0, event: 'Demo estate ready'
    };
    let gateCloseAt = 0, outdoorOffAt = 0, fanByTemp = false, garageByCar = false, timer;
    const ev = m => { S.event = m; };
    const parse = (v, cur) => {
      v = String(v).toLowerCase().trim();
      if (v === 'toggle') return !cur;
      if (['on', '1', 'true', 'open', 'yes', 'arm'].includes(v)) return true;
      if (['off', '0', 'false', 'close', 'closed', 'shut', 'no', 'disarm', 'stop'].includes(v)) return false;
      return null;
    };
    const names = { living: 'Living room light', bedroom: 'Bedroom light', garage_light: 'Garage light', outdoor: 'Outdoor light', fan: 'Fan' };

    function apply(device, value) {
      if (device === 'beep') { ev('Beep!'); return true; }
      if (device === 'all_lights') {
        const v = parse(value, S.living); if (v == null) return false;
        ['living', 'bedroom', 'garage_light', 'outdoor'].forEach(k => (S[k] = v));
        ev('All lights ' + (v ? 'ON' : 'OFF')); return true;
      }
      if (!(device in S)) return false;
      let val = value;
      if (device === 'cover') val = value === 'open' ? 'off' : (value === 'close' ? 'on' : value);
      const v = parse(val, S[device]); if (v == null) return false;
      if (S[device] === v) return true;
      S[device] = v;
      if (names[device]) ev(names[device] + (v ? ' ON' : ' OFF'));
      else if (device === 'gate') { ev(v ? 'Gate OPEN' : 'Gate CLOSED'); gateCloseAt = 0; }
      else if (device === 'garage_door') ev(v ? 'Garage door OPEN' : 'Garage door CLOSED');
      else if (device === 'cover') ev(v ? 'Clothesline cover CLOSED' : 'Clothesline cover OPEN');
      else if (device === 'security') { if (!v) S.alarm = false; ev(v ? 'Security ARMED' : 'Security DISARMED'); }
      else if (device === 'alarm') ev(v ? 'ALARM!' : 'Alarm silenced');
      else if (device === 'auto') ev(v ? 'Automation ON' : 'Automation OFF');
      if (device === 'fan') fanByTemp = false;
      if (device === 'outdoor') outdoorOffAt = 0;
      if (device === 'garage_light') garageByCar = false;
      return true;
    }

    function sensor(key, value) {
      if (S[key] === value) return;
      S[key] = value;
      const now = Date.now();
      if (key === 'motion' && value) {
        ev('Motion detected');
        if (S.security) { S.alarm = true; ev('ALARM! Intruder detected'); }
        if (S.auto && !S.outdoor) { S.outdoor = true; outdoorOffAt = now + 15000; }
      }
      if (key === 'car_gate') {
        if (value) { ev('Car at the gate'); if (S.auto && !S.security) { S.gate = true; ev('Car at the gate – gate OPEN'); } }
      }
      if (key === 'car_garage') {
        ev(value ? 'Car parked in garage' : 'Garage is empty');
        if (S.auto && value && !S.garage_light) { S.garage_light = true; garageByCar = true; }
        else if (S.auto && !value && garageByCar) { S.garage_light = false; garageByCar = false; }
      }
      if (key === 'door_open') {
        ev(value ? 'Front door opened' : 'Front door closed');
        if (value && S.security) { S.alarm = true; ev('ALARM! Door opened while armed'); }
      }
      if (key === 'raining') {
        S.rain_level = value ? 78 : 0;
        ev(value ? 'Rain detected!' : 'Rain stopped');
        if (S.auto) { S.cover = value; if (value) ev('Rain detected – clothesline cover CLOSED'); }
      }
    }

    function tick() {
      const now = Date.now();
      if (S.car_gate && S.gate && S.auto) gateCloseAt = now + 8000;
      if (gateCloseAt && now >= gateCloseAt) { gateCloseAt = 0; S.gate = false; ev('Gate CLOSED (auto)'); }
      if (outdoorOffAt && now >= outdoorOffAt && !S.motion) { outdoorOffAt = 0; S.outdoor = false; ev('Outdoor light OFF (timer)'); }
      if (S.auto) {
        if (S.temp >= 30 && !S.fan) { S.fan = true; fanByTemp = true; ev(`Hot (${S.temp} °C) – fan ON`); }
        else if (S.temp <= 28 && S.fan && fanByTemp) { S.fan = false; fanByTemp = false; ev('Cooled down – fan OFF'); }
      }
      S.hum = Math.max(30, Math.min(95, S.hum + (S.raining ? 0.4 : -0.1) + (Math.random() - 0.5) * 0.4));
      render({ ...S });
    }

    return {
      start() { setConn('demo', 'Demo'); tick(); timer = setInterval(tick, 500); },
      stop() { clearInterval(timer); },
      async send(device, value) { const ok = apply(device, value); tick(); return ok; },
      sensor(key, value) { sensor(key, value); tick(); },
      toggleSensor(key) { sensor(key, !S[key]); tick(); },
      setTemp(t) { S.temp = t; tick(); }
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
  $('#alarmStop').addEventListener('click', () => send('alarm', 'off'));
  $$('[data-sim]').forEach(btn => btn.addEventListener('click', () => transport.toggleSensor && transport.toggleSensor(btn.dataset.sim)));
  $('#simTemp').addEventListener('input', e => {
    $('#simTempVal').textContent = e.target.value + ' °C';
    transport.setTemp && transport.setTemp(Number(e.target.value));
  });

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
  const TARGETS = [
    { dev: 'all_lights', words: ['all lights', 'all the lights', 'every light', 'everything'] },
    { dev: 'living', words: ['living room', 'living', 'lounge', 'sitting room', 'parlour', 'parlor'] },
    { dev: 'bedroom', words: ['bedroom', 'bed room'] },
    { dev: 'garage_light', words: ['garage light', 'garage lights', 'garage lamp'] },
    { dev: 'garage_door', words: ['garage door', 'garage'] },
    { dev: 'outdoor', words: ['outdoor', 'outside', 'security light', 'compound light', 'porch'] },
    { dev: 'fan', words: ['fan', 'air'] },
    { dev: 'gate', words: ['gate'] },
    { dev: 'cover', words: ['cover', 'clothes', 'clothesline', 'clothes line', 'washing', 'laundry', 'shade'] },
    { dev: 'alarm', words: ['alarm', 'siren', 'buzzer'] },
    { dev: 'security', words: ['security', 'guard'] },
    { dev: 'auto', words: ['automation', 'automatic', 'auto mode'] }
  ];
  const LABEL = { all_lights: 'all lights', living: 'living room light', bedroom: 'bedroom light', garage_light: 'garage light',
    garage_door: 'garage door', outdoor: 'outdoor light', fan: 'fan', gate: 'gate', cover: 'clothesline cover',
    alarm: 'alarm', security: 'security', auto: 'automation' };

  function interpret(raw) {
    const t = ' ' + raw.toLowerCase().replace(/[^a-z0-9' ]/g, ' ').replace(/\s+/g, ' ') + ' ';
    const has = w => t.includes(' ' + w + ' ') || t.includes(' ' + w);
    // questions
    if (/temperature|how hot|how cold|weather/.test(t)) {
      return { say: state.temp == null ? 'The temperature sensor has no reading yet.' :
        `It is ${Number(state.temp).toFixed(1)} degrees with ${Math.round(state.hum)} percent humidity.` };
    }
    if (/humid/.test(t)) return { say: `Humidity is ${Math.round(state.hum ?? 0)} percent.` };
    if (/raining|rain/.test(t) && !/cover|close|open/.test(t)) return { say: state.raining ? 'Yes, it is raining. The clothesline is covered.' : 'No, it is dry outside.' };
    if (/status|report|everything ok|how is the house/.test(t)) {
      const on = ['living', 'bedroom', 'garage_light', 'outdoor'].filter(k => state[k]).map(k => LABEL[k]);
      return { say: `Gate is ${state.gate ? 'open' : 'closed'}, garage is ${state.garage_door ? 'open' : 'closed'}, security is ${state.security ? 'armed' : 'off'}. ` +
        (on.length ? `Lights on: ${on.join(', ')}.` : 'All lights are off.') };
    }
    if (/good ?night|bed ?time|sleep mode/.test(t)) {
      return { cmds: [['all_lights', 'off'], ['gate', 'close'], ['garage_door', 'close'], ['security', 'on']], say: 'Good night. Lights off, gate and garage closed, security armed.' };
    }
    if (/i'?m home|welcome home|good morning|arrived/.test(t)) {
      return { cmds: [['security', 'off'], ['living', 'on'], ['gate', 'open']], say: 'Welcome home. Security off, living room light on, gate opening.' };
    }

    // action
    let value = null;
    if (/\b(turn on|switch on|on|light up|start|enable|activate)\b/.test(t)) value = 'on';
    if (/\b(turn off|switch off|off|stop|disable|deactivate|kill|silence|quiet)\b/.test(t)) value = 'off';
    if (/\b(open|raise|lift)\b/.test(t)) value = 'open';
    if (/\b(close|shut|lower)\b/.test(t)) value = 'close';
    if (/\b(arm)\b/.test(t)) value = 'on';
    if (/\b(disarm)\b/.test(t)) value = 'off';
    if (/\btoggle\b/.test(t)) value = 'toggle';

    // target: first match in priority order (garage light before garage door)
    let target = null;
    for (const tg of TARGETS) { if (tg.words.some(has)) { target = tg.dev; break; } }
    if (target === 'garage_door' && /light|lamp|bulb/.test(t)) target = 'garage_light';
    if (!target && /\blights?\b/.test(t)) target = 'all_lights';
    if (!target) return { say: "Sorry, I didn't catch which part of the estate you mean." };
    if (target === 'alarm' && !value) value = 'off';
    if (target === 'cover' && !value && /\b(cover|protect)\b/.test(t)) value = 'close';
    if (!value) return { say: `Should I turn the ${LABEL[target]} on or off?` };
    // light-type devices: open/close mean on/off
    if (['living', 'bedroom', 'garage_light', 'outdoor', 'fan', 'all_lights', 'security', 'auto', 'alarm'].includes(target)) {
      if (value === 'open') value = 'on';
      if (value === 'close') value = 'off';
    }
    const words = { on: 'on', off: 'off', open: 'opening', close: 'closing', toggle: 'toggling' };
    let say = target === 'gate' || target === 'garage_door' || target === 'cover'
      ? `${value === 'on' || value === 'open' ? 'Opening' : value === 'toggle' ? 'Toggling' : 'Closing'} the ${LABEL[target]}.`
      : `Turning ${words[value] || value} the ${LABEL[target]}.`;
    if (target === 'cover') say = value === 'on' || value === 'close' ? 'Covering the clothesline.' : 'Rolling back the clothesline cover.';
    if (target === 'security') say = value === 'on' ? 'Security armed.' : 'Security disarmed.';
    if (target === 'all_lights') say = `Turning all lights ${value === 'on' ? 'on' : 'off'}.`;
    if (target === 'gate' && value === 'on') value = 'open';
    if (target === 'garage_door' && value === 'on') value = 'open';
    if ((target === 'gate' || target === 'garage_door') && value === 'off') value = 'close';
    return { cmds: [[target, value]], say };
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
