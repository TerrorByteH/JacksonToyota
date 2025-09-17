## Architecture

VSRM follows a simple layered layout suitable for a native desktop MVP:

- Win32 GUI (Presentation): `src/win32/WinMain.cpp`
  - Event loop, window, menus, simple text output
  - Issues commands to the application layer

- Application / Data Access: `src/app/Database.*`
  - Encapsulates SQLite access and schema initialization
  - Provides typed operations: insert record, list by VIN

- Resources: `resources/sql/schema.sql`
  - Defines tables and indices

### Data Model (MVP)
- `service_records (id, vin, customer_name, service_date, description, mechanic)`
- `mechanics (id, name, skill, active)`
- `appointments (id, vin, customer_name, scheduled_at, status)`
- `assignments (id, appointment_id, mechanic_id, assigned_at, completed_at)`

### Extensibility Plan
- Add `appointments`, `mechanics`, and `job_assignments` tables
- Introduce domain services for scheduling and workload balancing
- Add reporting/analytics queries and CSV export
- Replace raw Win32 with a UI toolkit (e.g., WinUI or Qt) if needed

### Error Handling
- Database methods return `bool`/`optional` and keep a `lastError` string for display

### Build System
- CMake project with vcpkg manifest; `sqlite3` is automatically provided


