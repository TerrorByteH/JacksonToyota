## User Guide

### Launching
- Double-click `vsrm.exe`. The database is created on first run.

### Main Window
- Read-only text area shows log lines/results.

### Menu Actions
- File → Add Sample Record: Inserts a demo service record for VIN `JT123TESTVIN00001`.
- File → Query Sample VIN: Lists records for the sample VIN into the main window.

#### Data Menu (Local-Only)
- Data → Add Sample Mechanic: Adds `Jane Smith` (Engine) as an active mechanic.
- Data → List Mechanics: Displays all active mechanics.
- Data → Add Sample Appointment: Creates a local appointment for the sample VIN.
- Data → List Appointments (Sample VIN): Lists appointments for the sample VIN.
- Data → Add Sample Assignment (Mech 1 → Appt 1): Links mechanic id 1 to appointment id 1.
- Data → List Assignments (Mechanic 1): Lists assignments for mechanic id 1.

### Reports Menu
- Reports → Export Service History CSV (Sample VIN): writes a CSV to your Desktop named `vsrm_history_JT123TESTVIN00001.csv`.
- Reports → Count Records in 2025: shows the total number of service records between `2025-01-01` and `2025-12-31`.

### Typical Workflow (Future)
- Create vehicle and customer records
- Schedule an appointment and assign a mechanic
- Record performed work and diagnostics
- Review history by VIN


