require('dotenv').config();
const crypto = require('crypto');
const pool = require('../src/db');
const sha = (s) => crypto.createHash('sha256').update(s).digest('hex');

async function main() {
  const hash = sha('collegue-key-123');
  await pool.query('UPDATE devices SET api_key_hash=$1 WHERE user_id=2', [hash]);
  console.log('SUCCESS: Collegue device key is set to: collegue-key-123');
  await pool.end();
}
main().catch(console.error);
