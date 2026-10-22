// Simulateur de boitier SentinelBand (aucun materiel necessaire).
// Usage : node src/simulate.js CLE_DU_BOITIER [URL_API] [mode_initial]
// Exemple : node src/simulate.js 0ef4cfb045... http://localhost:3000 auto
// Touches : 1 normal | 2 jaune (chute) | 3 rouge | 4 gaz critique | 5 temperature critique
//           a scenario automatique | 0 simuler un boitier hors ligne | Ctrl+C quitter
const readline = require('readline');

const key = process.argv[2];
const base = process.argv[3] || 'http://localhost:3000';
let mode = (process.argv[4] || 'normal').toLowerCase();
if (!key) {
  console.error('Usage : node src/simulate.js CLE_DU_BOITIER [URL_API] [normal|jaune|rouge|gaz|temp|auto]');
  process.exit(1);
}

const MODES = { 1: 'normal', 2: 'jaune', 3: 'rouge', 4: 'gaz', 5: 'temp', a: 'auto', 0: 'offline' };
const rnd = (a, b) => a + Math.random() * (b - a);
const t0 = Date.now();
let autoStart = Date.now();

// scenario automatique (boucle de 70 s) : normal -> jaune -> rouge -> annulation
function autoMode() {
  const s = ((Date.now() - autoStart) / 1000) % 70;
  if (s < 15) return 'normal';
  if (s < 30) return 'jaune';
  if (s < 55) return 'rouge';
  return 'normal';
}

function build(m) {
  const lat = 36.8065 + rnd(-0.0002, 0.0002);
  const lon = 10.1815 + rnd(-0.0002, 0.0002);
  const b = { state: 'NORMAL', reason: '', temp_c: +rnd(26.5, 27.5).toFixed(1), gas_delta_mv: Math.round(rnd(-10, 15)),
    acc_g: +rnd(0.98, 1.03).toFixed(2), gps_fix: true, lat, lon, uptime_s: Math.round((Date.now() - t0) / 1000), source: 'SIM' };
  if (m === 'jaune') { b.state = 'JAUNE'; b.reason = 'CHUTE+IMMOBILITE'; }
  if (m === 'rouge') { b.state = 'ROUGE'; b.reason = 'NON_CONFIRME_CHUTE+IMMOBILITE'; }
  if (m === 'gaz') { b.state = 'ROUGE'; b.reason = 'GAZ_CRITIQUE'; b.gas_delta_mv = Math.round(rnd(850, 950)); }
  if (m === 'temp') { b.state = 'ROUGE'; b.reason = 'TEMP_CRITIQUE'; b.temp_c = +rnd(62, 68).toFixed(1); }
  return b;
}

async function tick() {
  const m = mode === 'auto' ? autoMode() : mode;
  if (m === 'offline') { console.log(new Date().toLocaleTimeString() + ' | hors ligne (aucun envoi)'); return; }
  const body = build(m);
  try {
    const ctrl = new AbortController();
    const timer = setTimeout(() => ctrl.abort(), 5000);
    const res = await fetch(base + '/api/telemetry', {
      method: 'POST',
      headers: { 'Content-Type': 'application/json', 'x-api-key': key },
      body: JSON.stringify(body),
      signal: ctrl.signal,
    });
    clearTimeout(timer);
    console.log(new Date().toLocaleTimeString() + ' | ' + body.state + (body.reason ? ' (' + body.reason + ')' : '') + ' | T=' + body.temp_c + ' gaz=' + body.gas_delta_mv + ' | HTTP ' + res.status + (res.status === 401 ? ' (cle invalide)' : ''));
  } catch (e) {
    console.log(new Date().toLocaleTimeString() + ' | ERREUR reseau : ' + e.message);
  }
}

if (mode === 'auto') autoStart = Date.now();
console.log('Simulateur -> ' + base + ' | mode: ' + mode + ' | touches 1 2 3 4 5 a 0, Ctrl+C pour quitter');
setInterval(tick, 2000);
tick();

if (process.stdin.isTTY) {
  readline.emitKeypressEvents(process.stdin);
  process.stdin.setRawMode(true);
  process.stdin.on('keypress', (str, k) => {
    if (k && k.ctrl && k.name === 'c') process.exit(0);
    if (MODES[str]) {
      mode = MODES[str];
      if (mode === 'auto') autoStart = Date.now();
      console.log('>>> mode : ' + mode);
    }
  });
}
