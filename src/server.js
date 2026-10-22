require('dotenv').config();
const express = require('express');
const cors = require('cors');
const bcrypt = require('bcryptjs');
const jwt = require('jsonwebtoken');
const crypto = require('crypto');
const pool = require('./db');

if (!process.env.JWT_SECRET || !process.env.DATABASE_URL) {
  console.error('Variables JWT_SECRET et DATABASE_URL requises (copier .env.example vers .env)');
  process.exit(1);
}

const OFFLINE_S = 30; // sans donnees depuis 30 s => hors ligne
const app = express();
app.use(cors());
app.use(express.json({ limit: '10kb' }));

const sha = (s) => crypto.createHash('sha256').update(s).digest('hex');
const wrap = (fn) => (req, res, next) => Promise.resolve(fn(req, res, next)).catch(next);
const signToken = (id, name) => jwt.sign({ id, name }, process.env.JWT_SECRET, { expiresIn: '30d' });

function auth(req, res, next) {
  const h = req.headers.authorization || '';
  const t = h.startsWith('Bearer ') ? h.slice(7) : null;
  if (!t) return res.status(401).json({ error: 'token manquant' });
  try {
    req.user = jwt.verify(t, process.env.JWT_SECRET);
    next();
  } catch (e) {
    res.status(401).json({ error: 'token invalide' });
  }
}

async function canView(viewerId, watchedId) {
  if (viewerId === watchedId) return true;
  const r = await pool.query(
    "SELECT 1 FROM colleague_links WHERE viewer_id=$1 AND watched_id=$2 AND status='accepted'",
    [viewerId, watchedId]
  );
  return r.rowCount > 0;
}

async function pushToViewers(watchedId, title, body) {
  const r = await pool.query(
    `SELECT u.push_token FROM colleague_links l JOIN users u ON u.id = l.viewer_id
     WHERE l.watched_id = $1 AND l.status = 'accepted' AND u.push_token IS NOT NULL`,
    [watchedId]
  );
  const msgs = r.rows.map((x) => ({ to: x.push_token, title, body, sound: 'default', priority: 'high' }));
  if (!msgs.length) return;
  await fetch('https://exp.host/--/api/v2/push/send', {
    method: 'POST',
    headers: { 'Content-Type': 'application/json' },
    body: JSON.stringify(msgs),
  });
}

app.get('/health', (req, res) => res.json({ ok: true }));

// ---------- Comptes ----------
app.post('/api/auth/register', wrap(async (req, res) => {
  const b = req.body || {};
  const name = String(b.name || '').trim();
  const email = String(b.email || '').trim().toLowerCase();
  const password = String(b.password || '');
  if (!name || !/^\S+@\S+\.\S+$/.test(email) || password.length < 6) {
    return res.status(400).json({ error: 'nom, email valide et mot de passe (6 caracteres min) requis' });
  }
  const hash = await bcrypt.hash(password, 10);
  const deviceKey = crypto.randomBytes(24).toString('hex');
  const client = await pool.connect();
  try {
    await client.query('BEGIN');
    const u = await client.query(
      'INSERT INTO users(name,email,password_hash) VALUES($1,$2,$3) RETURNING id',
      [name, email, hash]
    );
    await client.query('INSERT INTO devices(user_id,api_key_hash) VALUES($1,$2)', [u.rows[0].id, sha(deviceKey)]);
    await client.query('COMMIT');
    res.status(201).json({ token: signToken(u.rows[0].id, name), deviceKey });
  } catch (e) {
    await client.query('ROLLBACK');
    if (e.code === '23505') return res.status(409).json({ error: 'email deja utilise' });
    throw e;
  } finally {
    client.release();
  }
}));

app.post('/api/auth/login', wrap(async (req, res) => {
  const b = req.body || {};
  const email = String(b.email || '').trim().toLowerCase();
  const r = await pool.query('SELECT id,name,password_hash FROM users WHERE email=$1', [email]);
  if (!r.rowCount || !(await bcrypt.compare(String(b.password || ''), r.rows[0].password_hash))) {
    return res.status(401).json({ error: 'identifiants incorrects' });
  }
  res.json({ token: signToken(r.rows[0].id, r.rows[0].name) });
}));

app.get('/api/me', auth, wrap(async (req, res) => {
  const r = await pool.query('SELECT id,name,email FROM users WHERE id=$1', [req.user.id]);
  if (!r.rowCount) return res.status(401).json({ error: 'utilisateur inconnu' });
  res.json(r.rows[0]);
}));

app.post('/api/me/push-token', auth, wrap(async (req, res) => {
  const t = String((req.body || {}).token || '');
  if (!t) return res.status(400).json({ error: 'token requis' });
  await pool.query('UPDATE users SET push_token=$1 WHERE id=$2', [t, req.user.id]);
  res.json({ ok: true });
}));

app.get('/api/me/device', auth, wrap(async (req, res) => {
  const r = await pool.query(
    `SELECT d.id AS device_id, d.label, s.state, s.reason, s.temp_c, s.gas_delta_mv, s.acc_g,
            s.lat, s.lon, s.gps_fix, s.source, s.last_seen,
            CASE WHEN s.last_seen IS NULL THEN NULL ELSE EXTRACT(EPOCH FROM (now() - s.last_seen))::int END AS age_s
     FROM devices d
     LEFT JOIN device_status s ON s.device_id = d.id
     WHERE d.user_id = $1`,
    [req.user.id]
  );
  if (!r.rowCount) {
    return res.json({ hasDevice: false });
  }
  const row = r.rows[0];
  res.json({
    hasDevice: true,
    deviceId: row.device_id,
    label: row.label,
    state: row.state || 'AUCUNE_DONNEE',
    reason: row.reason,
    temp_c: row.temp_c,
    gas_delta_mv: row.gas_delta_mv,
    acc_g: row.acc_g,
    lat: row.lat,
    lon: row.lon,
    gps_fix: row.gps_fix,
    source: row.source,
    last_seen: row.last_seen,
    age_s: row.age_s,
    online: row.age_s !== null && row.age_s <= OFFLINE_S
  });
}));

app.post('/api/me/device/regenerate-key', auth, wrap(async (req, res) => {
  const deviceKey = crypto.randomBytes(24).toString('hex');
  const client = await pool.connect();
  try {
    await client.query('BEGIN');
    const d = await client.query('SELECT id FROM devices WHERE user_id=$1', [req.user.id]);
    if (!d.rowCount) {
      await client.query('INSERT INTO devices(user_id,api_key_hash) VALUES($1,$2)', [req.user.id, sha(deviceKey)]);
    } else {
      await client.query('UPDATE devices SET api_key_hash=$1 WHERE id=$2', [sha(deviceKey), d.rows[0].id]);
    }
    await client.query('COMMIT');
    res.json({ ok: true, deviceKey });
  } catch (e) {
    await client.query('ROLLBACK');
    throw e;
  } finally {
    client.release();
  }
}));

// ---------- Collegues (avec consentement) ----------
app.post('/api/colleagues', auth, wrap(async (req, res) => {
  const email = String((req.body || {}).email || '').trim().toLowerCase();
  const t = await pool.query('SELECT id FROM users WHERE email=$1', [email]);
  if (!t.rowCount) return res.status(404).json({ error: 'aucun utilisateur avec cet email' });
  if (t.rows[0].id === req.user.id) return res.status(400).json({ error: 'impossible de vous ajouter vous-meme' });
  await pool.query(
    "INSERT INTO colleague_links(viewer_id,watched_id,status) VALUES($1,$2,'pending') ON CONFLICT (viewer_id,watched_id) DO NOTHING",
    [req.user.id, t.rows[0].id]
  );
  res.status(202).json({ status: 'pending' });
}));

app.get('/api/links/requests', auth, wrap(async (req, res) => {
  const r = await pool.query(
    `SELECT u.id,u.name,u.email FROM colleague_links l JOIN users u ON u.id = l.viewer_id
     WHERE l.watched_id=$1 AND l.status='pending' ORDER BY l.created_at`,
    [req.user.id]
  );
  res.json(r.rows);
}));

app.post('/api/links/:viewerId/accept', auth, wrap(async (req, res) => {
  const r = await pool.query(
    "UPDATE colleague_links SET status='accepted' WHERE viewer_id=$1 AND watched_id=$2 AND status='pending'",
    [Number(req.params.viewerId), req.user.id]
  );
  if (!r.rowCount) return res.status(404).json({ error: 'demande introuvable' });
  res.json({ ok: true });
}));

app.get('/api/colleagues', auth, wrap(async (req, res) => {
  const r = await pool.query(
    `SELECT u.id, u.name, s.state, s.reason, s.temp_c, s.gas_delta_mv, s.acc_g, s.lat, s.lon, s.gps_fix, s.source,
            CASE WHEN s.last_seen IS NULL THEN NULL ELSE EXTRACT(EPOCH FROM (now() - s.last_seen))::int END AS age_s
     FROM colleague_links l
     JOIN users u ON u.id = l.watched_id
     LEFT JOIN devices d ON d.user_id = u.id
     LEFT JOIN device_status s ON s.device_id = d.id
     WHERE l.viewer_id = $1 AND l.status = 'accepted'
     ORDER BY u.name`,
    [req.user.id]
  );
  res.json(r.rows.map((x) => ({ ...x, online: x.age_s !== null && x.age_s <= OFFLINE_S })));
}));

app.get('/api/colleagues/:id/history', auth, wrap(async (req, res) => {
  const id = Number(req.params.id);
  if (!Number.isInteger(id)) return res.status(400).json({ error: 'id invalide' });
  if (!(await canView(req.user.id, id))) return res.status(403).json({ error: 'acces refuse' });
  const limit = Math.min(parseInt(req.query.limit, 10) || 50, 500);
  const r = await pool.query(
    `SELECT t.ts, t.state, t.reason, t.temp_c, t.gas_delta_mv, t.acc_g, t.lat, t.lon
     FROM telemetry t JOIN devices d ON d.id = t.device_id
     WHERE d.user_id = $1 ORDER BY t.ts DESC LIMIT $2`,
    [id, limit]
  );
  res.json(r.rows);
}));

app.get('/api/colleagues/:id/alerts', auth, wrap(async (req, res) => {
  const id = Number(req.params.id);
  if (!Number.isInteger(id)) return res.status(400).json({ error: 'id invalide' });
  if (!(await canView(req.user.id, id))) return res.status(403).json({ error: 'acces refuse' });
  const r = await pool.query(
    `SELECT a.id, a.level, a.reason, a.lat, a.lon, a.started_at, a.ended_at, a.acked_at, ab.name AS acked_by
     FROM alerts a JOIN devices d ON d.id = a.device_id LEFT JOIN users ab ON ab.id = a.acked_by
     WHERE d.user_id = $1 ORDER BY a.started_at DESC LIMIT 20`,
    [id]
  );
  res.json(r.rows);
}));

app.post('/api/alerts/:id/ack', auth, wrap(async (req, res) => {
  const alertId = Number(req.params.id);
  const a = await pool.query(
    'SELECT d.user_id FROM alerts a JOIN devices d ON d.id = a.device_id WHERE a.id=$1',
    [alertId]
  );
  if (!a.rowCount) return res.status(404).json({ error: 'alerte introuvable' });
  if (!(await canView(req.user.id, a.rows[0].user_id))) return res.status(403).json({ error: 'acces refuse' });
  await pool.query('UPDATE alerts SET acked_by=$1, acked_at=now() WHERE id=$2 AND acked_at IS NULL', [req.user.id, alertId]);
  res.json({ ok: true });
}));

// ---------- Boitier (cle API) ----------
app.post('/api/telemetry', wrap(async (req, res) => {
  const b = req.body || {};
  const key = req.headers['x-api-key'] || b.api_key;
  if (!key) return res.status(401).json({ error: 'cle API manquante' });
  const d = await pool.query('SELECT id,user_id FROM devices WHERE api_key_hash=$1', [sha(String(key))]);
  if (!d.rowCount) return res.status(401).json({ error: 'cle API invalide' });
  const deviceId = d.rows[0].id;
  const userId = d.rows[0].user_id;

  if (!['NORMAL', 'JAUNE', 'ROUGE'].includes(b.state)) return res.status(400).json({ error: 'state invalide' });
  const state = b.state;
  const num = (v, min, max) => {
    if (v === null || v === undefined || v === '') return null;
    const n = Number(v);
    return Number.isFinite(n) && n >= min && n <= max ? n : null;
  };
  const reason = typeof b.reason === 'string' ? b.reason.slice(0, 64) : null;
  const temp = num(b.temp_c, -40, 150);
  const gas = num(b.gas_delta_mv, -5000, 5000);
  const acc = num(b.acc_g, 0, 50);
  const lat = num(b.lat, -90, 90);
  const lon = num(b.lon, -180, 180);
  const up = num(b.uptime_s, 0, 1e9);
  const src = b.source === 'SIM' ? 'SIM' : 'REEL';
  const fix = b.gps_fix === true && lat !== null && lon !== null;
  const plat = lat !== null ? lat : null;
  const plon = lon !== null ? lon : null;

  let prevState = 'NORMAL';
  const client = await pool.connect();
  try {
    await client.query('BEGIN');
    const prev = await client.query('SELECT state FROM device_status WHERE device_id=$1 FOR UPDATE', [deviceId]);
    if (prev.rowCount) prevState = prev.rows[0].state;
    await client.query(
      `INSERT INTO telemetry(device_id,state,reason,temp_c,gas_delta_mv,acc_g,lat,lon,gps_fix,uptime_s,source)
       VALUES($1,$2,$3,$4,$5,$6,$7,$8,$9,$10,$11)`,
      [deviceId, state, reason, temp, gas, acc, plat, plon, fix, up, src]
    );
    await client.query(
      `INSERT INTO device_status(device_id,state,reason,temp_c,gas_delta_mv,acc_g,lat,lon,gps_fix,source,last_seen)
       VALUES($1,$2,$3,$4,$5,$6,$7,$8,$9,$10,now())
       ON CONFLICT (device_id) DO UPDATE SET
         state=EXCLUDED.state, reason=EXCLUDED.reason, temp_c=EXCLUDED.temp_c,
         gas_delta_mv=EXCLUDED.gas_delta_mv, acc_g=EXCLUDED.acc_g,
         lat=COALESCE(EXCLUDED.lat, device_status.lat), lon=COALESCE(EXCLUDED.lon, device_status.lon),
         gps_fix=EXCLUDED.gps_fix, source=EXCLUDED.source, last_seen=now()`,
      [deviceId, state, reason, temp, gas, acc, plat, plon, fix, src]
    );
    if (state !== prevState) {
      await client.query('UPDATE alerts SET ended_at=now() WHERE device_id=$1 AND ended_at IS NULL', [deviceId]);
      if (state !== 'NORMAL') {
        await client.query('INSERT INTO alerts(device_id,level,reason,lat,lon) VALUES($1,$2,$3,$4,$5)', [deviceId, state, reason, plat, plon]);
      }
    }
    await client.query('COMMIT');
  } catch (e) {
    await client.query('ROLLBACK');
    throw e;
  } finally {
    client.release();
  }

  if (state === 'ROUGE' && prevState !== 'ROUGE') {
    const who = await pool.query('SELECT name FROM users WHERE id=$1', [userId]);
    pushToViewers(userId, 'ALERTE ROUGE', (who.rows[0] ? who.rows[0].name : 'Un collegue') + ' : ' + (reason || 'danger')).catch((e) => console.error('push:', e.message));
  }
  res.json({ ok: true });
}));

app.use((err, req, res, next) => {
  console.error(err);
  res.status(500).json({ error: 'erreur serveur' });
});

const port = Number(process.env.PORT) || 3000;
app.listen(port, '0.0.0.0', () => console.log('SentinelBand API sur le port ' + port));
