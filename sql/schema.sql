-- SentinelBand - schema PostgreSQL 14+
CREATE TABLE users (
  id SERIAL PRIMARY KEY,
  name TEXT NOT NULL,
  email TEXT UNIQUE NOT NULL,
  password_hash TEXT NOT NULL,
  push_token TEXT,
  created_at TIMESTAMPTZ NOT NULL DEFAULT now()
);

-- un boitier par utilisateur ; la cle API n'est stockee que hachee (SHA-256)
CREATE TABLE devices (
  id SERIAL PRIMARY KEY,
  user_id INT NOT NULL UNIQUE REFERENCES users(id) ON DELETE CASCADE,
  label TEXT NOT NULL DEFAULT 'SentinelBand',
  api_key_hash TEXT UNIQUE NOT NULL,
  created_at TIMESTAMPTZ NOT NULL DEFAULT now()
);

-- viewer_id a le droit de voir watched_id une fois status = accepted (consentement du collegue)
CREATE TABLE colleague_links (
  viewer_id INT NOT NULL REFERENCES users(id) ON DELETE CASCADE,
  watched_id INT NOT NULL REFERENCES users(id) ON DELETE CASCADE,
  status TEXT NOT NULL DEFAULT 'pending' CHECK (status IN ('pending','accepted')),
  created_at TIMESTAMPTZ NOT NULL DEFAULT now(),
  PRIMARY KEY (viewer_id, watched_id),
  CHECK (viewer_id <> watched_id)
);

-- historique complet des mesures
CREATE TABLE telemetry (
  id BIGSERIAL PRIMARY KEY,
  device_id INT NOT NULL REFERENCES devices(id) ON DELETE CASCADE,
  ts TIMESTAMPTZ NOT NULL DEFAULT now(),
  state TEXT NOT NULL CHECK (state IN ('NORMAL','JAUNE','ROUGE')),
  reason TEXT,
  temp_c REAL,
  gas_delta_mv INT,
  acc_g REAL,
  lat DOUBLE PRECISION,
  lon DOUBLE PRECISION,
  gps_fix BOOLEAN NOT NULL DEFAULT false,
  uptime_s INT,
  source TEXT
);
CREATE INDEX telemetry_device_ts ON telemetry (device_id, ts DESC);

-- dernier etat connu (lecture rapide pour l'application)
CREATE TABLE device_status (
  device_id INT PRIMARY KEY REFERENCES devices(id) ON DELETE CASCADE,
  state TEXT NOT NULL CHECK (state IN ('NORMAL','JAUNE','ROUGE')),
  reason TEXT,
  temp_c REAL,
  gas_delta_mv INT,
  acc_g REAL,
  lat DOUBLE PRECISION,
  lon DOUBLE PRECISION,
  gps_fix BOOLEAN NOT NULL DEFAULT false,
  source TEXT,
  last_seen TIMESTAMPTZ NOT NULL DEFAULT now()
);

-- alertes (ouverte tant que ended_at est NULL)
CREATE TABLE alerts (
  id BIGSERIAL PRIMARY KEY,
  device_id INT NOT NULL REFERENCES devices(id) ON DELETE CASCADE,
  level TEXT NOT NULL CHECK (level IN ('JAUNE','ROUGE')),
  reason TEXT,
  lat DOUBLE PRECISION,
  lon DOUBLE PRECISION,
  started_at TIMESTAMPTZ NOT NULL DEFAULT now(),
  ended_at TIMESTAMPTZ,
  acked_by INT REFERENCES users(id),
  acked_at TIMESTAMPTZ
);
CREATE INDEX alerts_device_started ON alerts (device_id, started_at DESC);
