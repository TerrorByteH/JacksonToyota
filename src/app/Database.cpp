#include "Database.h"

#ifdef VSRM_HAS_SQLITE3
#include <sqlite3.h>
#endif

#include <filesystem>
#include <sstream>
#include <fstream>

namespace fs = std::filesystem;

namespace vsrm {

Database::Database() : handle(nullptr) {}

Database::~Database() { close(); }

Database::Database(Database&& other) noexcept : handle(other.handle), lastError(std::move(other.lastError)) {
	other.handle = nullptr;
}

Database& Database::operator=(Database&& other) noexcept {
	if (this != &other) {
		close();
		handle = other.handle;
		lastError = std::move(other.lastError);
		other.handle = nullptr;
	}
	return *this;
}

bool Database::openOrCreate(const std::string& dbPath) {
#ifndef VSRM_HAS_SQLITE3
	lastError = "SQLite not available. Build with vcpkg manifest.";
	return false;
#else
	if (int rc = sqlite3_open(dbPath.c_str(), &handle); rc != SQLITE_OK) {
		lastError = "Failed to open DB: ";
		lastError += sqlite3_errmsg(handle);
		return false;
	}
	return true;
#endif
}

void Database::close() {
#ifdef VSRM_HAS_SQLITE3
	if (handle) {
		sqlite3_close(handle);
		handle = nullptr;
	}
#else
	(void)handle;
#endif
}

bool Database::initializeSchema(const std::string& schemaFilePath) {
#ifndef VSRM_HAS_SQLITE3
	(void)schemaFilePath;
	lastError = "SQLite not available.";
	return false;
#else
	std::ifstream in(schemaFilePath);
	if (!in) {
		lastError = "Cannot open schema file: " + schemaFilePath;
		return false;
	}
	std::ostringstream oss;
	oss << in.rdbuf();
	const std::string sql = oss.str();

	char* errMsg = nullptr;
	int rc = sqlite3_exec(handle, sql.c_str(), nullptr, nullptr, &errMsg);
	if (rc != SQLITE_OK) {
		lastError = errMsg ? errMsg : "Unknown SQL error";
		sqlite3_free(errMsg);
		return false;
	}
	return true;
#endif
}

std::optional<int> Database::addServiceRecord(const ServiceRecord& record) {
#ifndef VSRM_HAS_SQLITE3
	(void)record;
	lastError = "SQLite not available.";
	return std::nullopt;
#else
	const char* sql =
		"INSERT INTO service_records (vin, customer_name, service_date, description, mechanic) "
		"VALUES (?1, ?2, ?3, ?4, ?5);";
	sqlite3_stmt* stmt = nullptr;
	if (sqlite3_prepare_v2(handle, sql, -1, &stmt, nullptr) != SQLITE_OK) {
		lastError = sqlite3_errmsg(handle);
		return std::nullopt;
	}
	sqlite3_bind_text(stmt, 1, record.vin.c_str(), -1, SQLITE_TRANSIENT);
	sqlite3_bind_text(stmt, 2, record.customerName.c_str(), -1, SQLITE_TRANSIENT);
	sqlite3_bind_text(stmt, 3, record.serviceDate.c_str(), -1, SQLITE_TRANSIENT);
	sqlite3_bind_text(stmt, 4, record.description.c_str(), -1, SQLITE_TRANSIENT);
	sqlite3_bind_text(stmt, 5, record.mechanic.c_str(), -1, SQLITE_TRANSIENT);

	if (sqlite3_step(stmt) != SQLITE_DONE) {
		lastError = sqlite3_errmsg(handle);
		sqlite3_finalize(stmt);
		return std::nullopt;
	}
	int id = static_cast<int>(sqlite3_last_insert_rowid(handle));
	sqlite3_finalize(stmt);
	return id;
#endif
}

std::vector<ServiceRecord> Database::listServiceRecordsByVin(const std::string& vin) {
	std::vector<ServiceRecord> result;
#ifndef VSRM_HAS_SQLITE3
	(void)vin;
	lastError = "SQLite not available.";
	return result;
#else
	const char* sql =
		"SELECT id, vin, customer_name, service_date, description, mechanic "
		"FROM service_records WHERE vin = ?1 ORDER BY service_date DESC, id DESC;";
	sqlite3_stmt* stmt = nullptr;
	if (sqlite3_prepare_v2(handle, sql, -1, &stmt, nullptr) != SQLITE_OK) {
		lastError = sqlite3_errmsg(handle);
		return result;
	}
	sqlite3_bind_text(stmt, 1, vin.c_str(), -1, SQLITE_TRANSIENT);
	while (sqlite3_step(stmt) == SQLITE_ROW) {
		ServiceRecord r{};
		r.id = sqlite3_column_int(stmt, 0);
		r.vin = reinterpret_cast<const char*>(sqlite3_column_text(stmt, 1));
		r.customerName = reinterpret_cast<const char*>(sqlite3_column_text(stmt, 2));
		r.serviceDate = reinterpret_cast<const char*>(sqlite3_column_text(stmt, 3));
		r.description = reinterpret_cast<const char*>(sqlite3_column_text(stmt, 4));
		r.mechanic = reinterpret_cast<const char*>(sqlite3_column_text(stmt, 5));
		result.push_back(std::move(r));
	}
	sqlite3_finalize(stmt);
	return result;
#endif
}

std::optional<int> Database::addMechanic(const Mechanic& mech) {
#ifndef VSRM_HAS_SQLITE3
	(void)mech;
	lastError = "SQLite not available.";
	return std::nullopt;
#else
	const char* sql = "INSERT INTO mechanics (name, skill, active) VALUES (?1, ?2, ?3);";
	sqlite3_stmt* stmt = nullptr;
	if (sqlite3_prepare_v2(handle, sql, -1, &stmt, nullptr) != SQLITE_OK) { lastError = sqlite3_errmsg(handle); return std::nullopt; }
	sqlite3_bind_text(stmt, 1, mech.name.c_str(), -1, SQLITE_TRANSIENT);
	sqlite3_bind_text(stmt, 2, mech.skill.c_str(), -1, SQLITE_TRANSIENT);
	sqlite3_bind_int(stmt, 3, mech.active ? 1 : 0);
	if (sqlite3_step(stmt) != SQLITE_DONE) { lastError = sqlite3_errmsg(handle); sqlite3_finalize(stmt); return std::nullopt; }
	int id = (int)sqlite3_last_insert_rowid(handle);
	sqlite3_finalize(stmt);
	return id;
#endif
}

std::vector<Mechanic> Database::listMechanics(bool onlyActive) {
	std::vector<Mechanic> result;
#ifndef VSRM_HAS_SQLITE3
	(void)onlyActive;
	lastError = "SQLite not available.";
	return result;
#else
	const char* sqlAll = "SELECT id, name, skill, active FROM mechanics ORDER BY name;";
	const char* sqlAct = "SELECT id, name, skill, active FROM mechanics WHERE active = 1 ORDER BY name;";
	sqlite3_stmt* stmt = nullptr;
	if (sqlite3_prepare_v2(handle, onlyActive ? sqlAct : sqlAll, -1, &stmt, nullptr) != SQLITE_OK) { lastError = sqlite3_errmsg(handle); return result; }
	while (sqlite3_step(stmt) == SQLITE_ROW) {
		Mechanic m{};
		m.id = sqlite3_column_int(stmt, 0);
		m.name = reinterpret_cast<const char*>(sqlite3_column_text(stmt, 1));
		m.skill = reinterpret_cast<const char*>(sqlite3_column_text(stmt, 2));
		m.active = sqlite3_column_int(stmt, 3) != 0;
		result.push_back(std::move(m));
	}
	sqlite3_finalize(stmt);
	return result;
#endif
}

bool Database::updateMechanic(const Mechanic& mech) {
#ifndef VSRM_HAS_SQLITE3
	(void)mech; lastError = "SQLite not available."; return false;
#else
	const char* sql = "UPDATE mechanics SET name = ?1, skill = ?2, active = ?3 WHERE id = ?4;";
	sqlite3_stmt* stmt = nullptr;
	if (sqlite3_prepare_v2(handle, sql, -1, &stmt, nullptr) != SQLITE_OK) { lastError = sqlite3_errmsg(handle); return false; }
	sqlite3_bind_text(stmt, 1, mech.name.c_str(), -1, SQLITE_TRANSIENT);
	sqlite3_bind_text(stmt, 2, mech.skill.c_str(), -1, SQLITE_TRANSIENT);
	sqlite3_bind_int(stmt, 3, mech.active ? 1 : 0);
	sqlite3_bind_int(stmt, 4, mech.id);
	bool ok = sqlite3_step(stmt) == SQLITE_DONE;
	if (!ok) lastError = sqlite3_errmsg(handle);
	sqlite3_finalize(stmt);
	return ok;
#endif
}

bool Database::deleteMechanic(int mechanicId) {
#ifndef VSRM_HAS_SQLITE3
	(void)mechanicId; lastError = "SQLite not available."; return false;
#else
	const char* sql = "DELETE FROM mechanics WHERE id = ?1;";
	sqlite3_stmt* stmt = nullptr;
	if (sqlite3_prepare_v2(handle, sql, -1, &stmt, nullptr) != SQLITE_OK) { lastError = sqlite3_errmsg(handle); return false; }
	sqlite3_bind_int(stmt, 1, mechanicId);
	bool ok = sqlite3_step(stmt) == SQLITE_DONE;
	if (!ok) lastError = sqlite3_errmsg(handle);
	sqlite3_finalize(stmt);
	return ok;
#endif
}

std::optional<int> Database::addAppointment(const Appointment& appt) {
#ifndef VSRM_HAS_SQLITE3
	(void)appt;
	lastError = "SQLite not available.";
	return std::nullopt;
#else
	const char* sql = "INSERT INTO appointments (vin, customer_name, scheduled_at, status) VALUES (?1, ?2, ?3, ?4);";
	sqlite3_stmt* stmt = nullptr;
	if (sqlite3_prepare_v2(handle, sql, -1, &stmt, nullptr) != SQLITE_OK) { lastError = sqlite3_errmsg(handle); return std::nullopt; }
	sqlite3_bind_text(stmt, 1, appt.vin.c_str(), -1, SQLITE_TRANSIENT);
	sqlite3_bind_text(stmt, 2, appt.customerName.c_str(), -1, SQLITE_TRANSIENT);
	sqlite3_bind_text(stmt, 3, appt.scheduledAt.c_str(), -1, SQLITE_TRANSIENT);
	sqlite3_bind_text(stmt, 4, appt.status.c_str(), -1, SQLITE_TRANSIENT);
	if (sqlite3_step(stmt) != SQLITE_DONE) { lastError = sqlite3_errmsg(handle); sqlite3_finalize(stmt); return std::nullopt; }
	int id = (int)sqlite3_last_insert_rowid(handle);
	sqlite3_finalize(stmt);
	return id;
#endif
}

std::vector<Appointment> Database::listAppointmentsByVin(const std::string& vin) {
	std::vector<Appointment> result;
#ifndef VSRM_HAS_SQLITE3
	(void)vin; lastError = "SQLite not available."; return result;
#else
	const char* sql = "SELECT id, vin, customer_name, scheduled_at, status FROM appointments WHERE vin = ?1 ORDER BY scheduled_at DESC, id DESC;";
	sqlite3_stmt* stmt = nullptr;
	if (sqlite3_prepare_v2(handle, sql, -1, &stmt, nullptr) != SQLITE_OK) { lastError = sqlite3_errmsg(handle); return result; }
	sqlite3_bind_text(stmt, 1, vin.c_str(), -1, SQLITE_TRANSIENT);
	while (sqlite3_step(stmt) == SQLITE_ROW) {
		Appointment a{};
		a.id = sqlite3_column_int(stmt, 0);
		a.vin = reinterpret_cast<const char*>(sqlite3_column_text(stmt, 1));
		a.customerName = reinterpret_cast<const char*>(sqlite3_column_text(stmt, 2));
		a.scheduledAt = reinterpret_cast<const char*>(sqlite3_column_text(stmt, 3));
		a.status = reinterpret_cast<const char*>(sqlite3_column_text(stmt, 4));
		result.push_back(std::move(a));
	}
	sqlite3_finalize(stmt);
	return result;
#endif
}

std::optional<int> Database::addAssignment(const Assignment& asg) {
#ifndef VSRM_HAS_SQLITE3
	(void)asg; lastError = "SQLite not available."; return std::nullopt;
#else
	const char* sql = "INSERT INTO assignments (appointment_id, mechanic_id, assigned_at, completed_at) VALUES (?1, ?2, ?3, ?4);";
	sqlite3_stmt* stmt = nullptr;
	if (sqlite3_prepare_v2(handle, sql, -1, &stmt, nullptr) != SQLITE_OK) { lastError = sqlite3_errmsg(handle); return std::nullopt; }
	sqlite3_bind_int(stmt, 1, asg.appointmentId);
	sqlite3_bind_int(stmt, 2, asg.mechanicId);
	sqlite3_bind_text(stmt, 3, asg.assignedAt.c_str(), -1, SQLITE_TRANSIENT);
	if (asg.completedAt.has_value())
		sqlite3_bind_text(stmt, 4, asg.completedAt->c_str(), -1, SQLITE_TRANSIENT);
	else
		sqlite3_bind_null(stmt, 4);
	if (sqlite3_step(stmt) != SQLITE_DONE) { lastError = sqlite3_errmsg(handle); sqlite3_finalize(stmt); return std::nullopt; }
	int id = (int)sqlite3_last_insert_rowid(handle);
	sqlite3_finalize(stmt);
	return id;
#endif
}

std::vector<Assignment> Database::listAssignmentsByMechanic(int mechanicId) {
	std::vector<Assignment> result;
#ifndef VSRM_HAS_SQLITE3
	(void)mechanicId; lastError = "SQLite not available."; return result;
#else
	const char* sql = "SELECT id, appointment_id, mechanic_id, assigned_at, completed_at FROM assignments WHERE mechanic_id = ?1 ORDER BY assigned_at DESC, id DESC;";
	sqlite3_stmt* stmt = nullptr;
	if (sqlite3_prepare_v2(handle, sql, -1, &stmt, nullptr) != SQLITE_OK) { lastError = sqlite3_errmsg(handle); return result; }
	sqlite3_bind_int(stmt, 1, mechanicId);
	while (sqlite3_step(stmt) == SQLITE_ROW) {
		Assignment s{};
		s.id = sqlite3_column_int(stmt, 0);
		s.appointmentId = sqlite3_column_int(stmt, 1);
		s.mechanicId = sqlite3_column_int(stmt, 2);
		s.assignedAt = reinterpret_cast<const char*>(sqlite3_column_text(stmt, 3));
		if (sqlite3_column_type(stmt, 4) == SQLITE_NULL) s.completedAt.reset(); else s.completedAt = std::string(reinterpret_cast<const char*>(sqlite3_column_text(stmt, 4)));
		result.push_back(std::move(s));
	}
	sqlite3_finalize(stmt);
	return result;
#endif
}

} // namespace vsrm

#include <iomanip>
#include <random>
#include <windows.h>
#include <bcrypt.h>

namespace vsrm {

static std::string escapeCsv(const std::string& in) {
	bool needsQuotes = in.find_first_of(",\n\r\"") != std::string::npos;
	std::string out;
	out.reserve(in.size() + 4);
	if (!needsQuotes) return in;
	out.push_back('"');
	for (char c : in) {
		if (c == '"') out.push_back('"');
		out.push_back(c);
	}
	out.push_back('"');
	return out;
}

bool Database::exportServiceHistoryCsv(const std::string& vin, const std::string& outputFilePath) {
#ifndef VSRM_HAS_SQLITE3
	(void)vin; (void)outputFilePath; lastError = "SQLite not available."; return false;
#else
	std::ofstream out(outputFilePath, std::ios::binary);
	if (!out) { lastError = "Failed to open output file"; return false; }
	out << "id,vin,customer_name,service_date,description,mechanic\n";
	const char* sql = "SELECT id, vin, customer_name, service_date, description, mechanic FROM service_records WHERE vin = ?1 ORDER BY service_date, id;";
	sqlite3_stmt* stmt = nullptr;
	if (sqlite3_prepare_v2(handle, sql, -1, &stmt, nullptr) != SQLITE_OK) { lastError = sqlite3_errmsg(handle); return false; }
	sqlite3_bind_text(stmt, 1, vin.c_str(), -1, SQLITE_TRANSIENT);
	while (sqlite3_step(stmt) == SQLITE_ROW) {
		std::string id = std::to_string(sqlite3_column_int(stmt, 0));
		std::string cvin = reinterpret_cast<const char*>(sqlite3_column_text(stmt, 1));
		std::string cust = reinterpret_cast<const char*>(sqlite3_column_text(stmt, 2));
		std::string date = reinterpret_cast<const char*>(sqlite3_column_text(stmt, 3));
		std::string desc = reinterpret_cast<const char*>(sqlite3_column_text(stmt, 4));
		std::string mech = reinterpret_cast<const char*>(sqlite3_column_text(stmt, 5));
		out << id << ',' << escapeCsv(cvin) << ',' << escapeCsv(cust) << ',' << escapeCsv(date) << ',' << escapeCsv(desc) << ',' << escapeCsv(mech) << "\n";
	}
	sqlite3_finalize(stmt);
	return true;
#endif
}

int Database::countServiceRecordsByDateRange(const std::string& startDateInclusive, const std::string& endDateInclusive) {
#ifndef VSRM_HAS_SQLITE3
	(void)startDateInclusive; (void)endDateInclusive; lastError = "SQLite not available."; return 0;
#else
	const char* sql = "SELECT COUNT(*) FROM service_records WHERE service_date >= ?1 AND service_date <= ?2;";
	sqlite3_stmt* stmt = nullptr;
	if (sqlite3_prepare_v2(handle, sql, -1, &stmt, nullptr) != SQLITE_OK) { lastError = sqlite3_errmsg(handle); return 0; }
	sqlite3_bind_text(stmt, 1, startDateInclusive.c_str(), -1, SQLITE_TRANSIENT);
	sqlite3_bind_text(stmt, 2, endDateInclusive.c_str(), -1, SQLITE_TRANSIENT);
	int count = 0;
	if (sqlite3_step(stmt) == SQLITE_ROW) {
		count = sqlite3_column_int(stmt, 0);
	}
	sqlite3_finalize(stmt);
	return count;
#endif
}

} // namespace vsrm

namespace {
static std::string toHex(const unsigned char* data, size_t len) {
	static const char* hex = "0123456789abcdef";
	std::string out; out.resize(len * 2);
	for (size_t i = 0; i < len; ++i) { out[i*2] = hex[(data[i] >> 4) & 0xF]; out[i*2+1] = hex[data[i] & 0xF]; }
	return out;
}
static std::string sha256(const std::string& input) {
    BCRYPT_ALG_HANDLE hAlg = nullptr; BCRYPT_HASH_HANDLE hHash = nullptr;
    NTSTATUS status = BCryptOpenAlgorithmProvider(&hAlg, BCRYPT_SHA256_ALGORITHM, nullptr, 0);
    if (status != 0) return {};
    DWORD hashObjectSize = 0, cb = 0; BCryptGetProperty(hAlg, BCRYPT_OBJECT_LENGTH, (PUCHAR)&hashObjectSize, sizeof(hashObjectSize), &cb, 0);
    DWORD hashLen = 0; BCryptGetProperty(hAlg, BCRYPT_HASH_LENGTH, (PUCHAR)&hashLen, sizeof(hashLen), &cb, 0);
    std::vector<UCHAR> hashObject(hashObjectSize);
    std::vector<UCHAR> hash(hashLen);
    status = BCryptCreateHash(hAlg, &hHash, hashObject.data(), hashObjectSize, nullptr, 0, 0);
    if (status != 0) { BCryptCloseAlgorithmProvider(hAlg, 0); return {}; }
    status = BCryptHashData(hHash, (PUCHAR)input.data(), (ULONG)input.size(), 0);
    if (status == 0) status = BCryptFinishHash(hHash, hash.data(), hashLen, 0);
    BCryptDestroyHash(hHash); BCryptCloseAlgorithmProvider(hAlg, 0);
    if (status != 0) return {};
    return toHex(hash.data(), hash.size());
}
static std::string randomSalt() {
	std::random_device rd; std::mt19937_64 gen(rd()); std::uniform_int_distribution<unsigned long long> d;
	unsigned long long v = d(gen);
	return sha256(std::string(reinterpret_cast<char*>(&v), sizeof(v))).substr(0, 16);
}
}

namespace vsrm {

bool Database::ensureDefaultAdmin() {
#ifndef VSRM_HAS_SQLITE3
	lastError = "SQLite not available."; return false;
#else
	const char* q = "SELECT 1 FROM users WHERE username = 'admin' LIMIT 1;";
	sqlite3_stmt* stmt = nullptr; if (sqlite3_prepare_v2(handle, q, -1, &stmt, nullptr) != SQLITE_OK) return false;
	int rc = sqlite3_step(stmt); bool exists = (rc == SQLITE_ROW);
	sqlite3_finalize(stmt);
	if (exists) return true;
	return createUser("admin", "admin");
#endif
}

bool Database::createUser(const std::string& username, const std::string& password) {
#ifndef VSRM_HAS_SQLITE3
	(void)username; (void)password; lastError = "SQLite not available."; return false;
#else
	std::string salt = randomSalt();
	std::string hash = sha256(password + salt);
	const char* sql = "INSERT INTO users (username, password_hash, salt) VALUES (?1, ?2, ?3);";
	sqlite3_stmt* stmt = nullptr; if (sqlite3_prepare_v2(handle, sql, -1, &stmt, nullptr) != SQLITE_OK) { lastError = sqlite3_errmsg(handle); return false; }
	sqlite3_bind_text(stmt, 1, username.c_str(), -1, SQLITE_TRANSIENT);
	sqlite3_bind_text(stmt, 2, hash.c_str(), -1, SQLITE_TRANSIENT);
	sqlite3_bind_text(stmt, 3, salt.c_str(), -1, SQLITE_TRANSIENT);
	bool ok = sqlite3_step(stmt) == SQLITE_DONE; if (!ok) lastError = sqlite3_errmsg(handle);
	sqlite3_finalize(stmt); return ok;
#endif
}

bool Database::verifyLogin(const std::string& username, const std::string& password) {
#ifndef VSRM_HAS_SQLITE3
	(void)username; (void)password; lastError = "SQLite not available."; return false;
#else
	const char* sql = "SELECT password_hash, salt FROM users WHERE username = ?1 LIMIT 1;";
	sqlite3_stmt* stmt = nullptr; if (sqlite3_prepare_v2(handle, sql, -1, &stmt, nullptr) != SQLITE_OK) { lastError = sqlite3_errmsg(handle); return false; }
	sqlite3_bind_text(stmt, 1, username.c_str(), -1, SQLITE_TRANSIENT);
	std::string dbHash, salt; bool found = false;
	if (sqlite3_step(stmt) == SQLITE_ROW) {
		found = true;
		dbHash = reinterpret_cast<const char*>(sqlite3_column_text(stmt, 0));
		salt = reinterpret_cast<const char*>(sqlite3_column_text(stmt, 1));
	}
	sqlite3_finalize(stmt);
	if (!found) return false;
	return sha256(password + salt) == dbHash;
#endif
}

} // namespace vsrm


