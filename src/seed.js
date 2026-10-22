// Donnees de demonstration. ATTENTION : efface toutes les donnees existantes.
require('dotenv').config();
const bcrypt = require('bcryptjs');
const crypto = require('crypto');
const pool = require('./db');

const sha = (s) => crypto.createHash('sha256').update(s).digest('hex');

(async () => {
  await pool.query('TRUNCATE users RESTART IDENTITY CASCADE');
  const demo = [
    { name: 'Adem', email: 'adem@demo.tn', key: 'adem_dev_key_0123456789abcdef01234567' },
    { name: 'Collegue', email: 'collegue@demo.tn', key: 'collegue_dev_key_0123456789abcdef012345' },
  ];
  const ids = [];
  for (const item of demo) {
    const hash = await bcrypt.hash('demo1234', 10);
    const key = item.key;
    const u = await pool.query('INSERT INTO users(name,email,password_hash) VALUES($1,$2,$3) RETURNING id', [item.name, item.email, hash]);
    await pool.query('INSERT INTO devices(user_id,api_key_hash) VALUES($1,$2)', [u.rows[0].id, sha(key)]);
    ids.push(u.rows[0].id);
    console.log(item.name + ' | ' + item.email + ' | mot de passe: demo1234 | cle boitier: ' + key);
  }
  await pool.query("INSERT INTO colleague_links(viewer_id,watched_id,status) VALUES($1,$2,'accepted'),($2,$1,'accepted')", [ids[0], ids[1]]);
  console.log('Liens collegues crees (dans les deux sens).');
  await pool.end();
})().catch((e) => { console.error(e); process.exit(1); });
