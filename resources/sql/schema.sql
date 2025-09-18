PRAGMA foreign_keys = ON;

CREATE TABLE IF NOT EXISTS service_records (
	id INTEGER PRIMARY KEY AUTOINCREMENT,
	vin TEXT NOT NULL,
	customer_name TEXT NOT NULL,
	service_date TEXT NOT NULL,
	description TEXT NOT NULL,
	mechanic TEXT NOT NULL
);

CREATE INDEX IF NOT EXISTS idx_service_records_vin_date
ON service_records (vin, service_date DESC);

-- Mechanics roster
CREATE TABLE IF NOT EXISTS mechanics (
	id INTEGER PRIMARY KEY AUTOINCREMENT,
	name TEXT NOT NULL,
	skill TEXT NOT NULL,
	active INTEGER NOT NULL DEFAULT 1
);

-- Appointments (local-only scheduling)
CREATE TABLE IF NOT EXISTS appointments (
	id INTEGER PRIMARY KEY AUTOINCREMENT,
	vin TEXT NOT NULL,
	customer_name TEXT NOT NULL,
	scheduled_at TEXT NOT NULL, -- ISO 8601
	status TEXT NOT NULL -- e.g., scheduled, in_progress, done, cancelled
);

-- Job assignments linking appointments to mechanics
CREATE TABLE IF NOT EXISTS assignments (
	id INTEGER PRIMARY KEY AUTOINCREMENT,
	appointment_id INTEGER NOT NULL,
	mechanic_id INTEGER NOT NULL,
	assigned_at TEXT NOT NULL,
	completed_at TEXT,
	FOREIGN KEY (appointment_id) REFERENCES appointments(id) ON DELETE CASCADE,
	FOREIGN KEY (mechanic_id) REFERENCES mechanics(id) ON DELETE RESTRICT
);

CREATE INDEX IF NOT EXISTS idx_appointments_vin_date ON appointments (vin, scheduled_at DESC);
CREATE INDEX IF NOT EXISTS idx_assignments_mechanic ON assignments (mechanic_id);

-- Users for local authentication (simple, single-machine)
CREATE TABLE IF NOT EXISTS users (
	id INTEGER PRIMARY KEY AUTOINCREMENT,
	username TEXT NOT NULL UNIQUE,
	password_hash TEXT NOT NULL,
	salt TEXT NOT NULL
);


