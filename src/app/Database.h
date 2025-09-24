#pragma once

#include <string>
#include <optional>
#include <vector>

struct sqlite3;

namespace vsrm {

struct ServiceRecord {
	int id{};
	std::string vin;
	std::string customerName;
	std::string serviceDate; // ISO 8601 date string
	std::string description;
	std::string mechanic;
};

struct Mechanic {
	int id{};
	std::string name;
	std::string skill;
	bool active{true};
};

struct Appointment {
	int id{};
	std::string vin;
	std::string customerName;
	std::string scheduledAt; // ISO 8601
	std::string status; // scheduled, in_progress, done, cancelled
};

struct Assignment {
	int id{};
	int appointmentId{};
	int mechanicId{};
	std::string assignedAt; // ISO 8601
	std::optional<std::string> completedAt; // ISO 8601
};

struct VehicleSummary {
    std::string vin;
    std::string make;        // optional: left blank if unknown
    std::string model;       // optional: left blank if unknown
    std::string lastServiceDate;
    std::string mechanic;
    std::optional<std::string> nextService;
    std::string status;      // scheduled, ok
};

class Database {
public:
	Database();
	~Database();

	// Not copyable
	Database(const Database&) = delete;
	Database& operator=(const Database&) = delete;

	// Movable
	Database(Database&&) noexcept;
	Database& operator=(Database&&) noexcept;

	bool openOrCreate(const std::string& dbPath);
	void close();

	bool initializeSchema(const std::string& schemaFilePath);

	// CRUD for service records (minimal for demo)
	std::optional<int> addServiceRecord(const ServiceRecord& record);
	std::vector<ServiceRecord> listServiceRecordsByVin(const std::string& vin);
    bool updateServiceRecord(const ServiceRecord& record);

	// Mechanics
	std::optional<int> addMechanic(const Mechanic& mech);
	std::vector<Mechanic> listMechanics(bool onlyActive = true);
	bool updateMechanic(const Mechanic& mech);
	bool deleteMechanic(int mechanicId);

	// Appointments
	std::optional<int> addAppointment(const Appointment& appt);
	std::vector<Appointment> listAppointmentsByVin(const std::string& vin);

	// Assignments
	std::optional<int> addAssignment(const Assignment& asg);
	std::vector<Assignment> listAssignmentsByMechanic(int mechanicId);

	// Reports
	bool exportServiceHistoryCsv(const std::string& vin, const std::string& outputFilePath);
	int countServiceRecordsByDateRange(const std::string& startDateInclusive, const std::string& endDateInclusive);

	// Users/auth (local)
	bool ensureDefaultAdmin();
	bool createUser(const std::string& username, const std::string& password);
	bool verifyLogin(const std::string& username, const std::string& password);

    // Dashboard metrics
    int countDistinctCustomers();
    int countActiveMechanics();
    int countAppointments();
    int countServiceRecords();
    std::vector<ServiceRecord> fetchRecentServiceRecords(int limit);
    bool exportAllServiceRecordsCsv(const std::string& outputFilePath);

    // Vehicle summaries for the grid
    std::vector<VehicleSummary> listVehicleSummaries(
        const std::string& vinLike,
        const std::optional<std::string>& fromDate,
        const std::optional<std::string>& toDate,
        const std::optional<std::string>& mechanicLike,
        bool dueOnly);

	std::string getLastError() const { return lastError; }

private:
	sqlite3* handle;
	std::string lastError;
};

} // namespace vsrm


