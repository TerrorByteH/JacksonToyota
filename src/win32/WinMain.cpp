#include <windows.h>
#include <string>
#include <vector>
#include <filesystem>
#include <commctrl.h>
#include <gdiplus.h>
#include <shlobj.h>

#include "../app/Database.h"

namespace fs = std::filesystem;

static const wchar_t* kClassName = L"VSRMMainWindow";
static const int kHeaderHeight = 56;

struct AppState {
	vsrm::Database db;
	std::wstring dbPath;
    HFONT hFont{};
    HBRUSH hBg{};           // main background
    HBRUSH hHeaderBg{};     // banner background
    HBRUSH hButtonBg{};     // welcome button background
    Gdiplus::Image* logo{};
    // Welcome panel
    bool showWelcome{true};
    HWND hBtnAdd{};
    HWND hBtnQuery{};
    HWND hBtnMechanics{};
    HWND hBtnExport{};
    HWND hBtnRefresh{};
};

static void ShowError(HWND hwnd, const std::wstring& title, const std::string& msg) {
	MessageBoxW(hwnd, std::wstring(msg.begin(), msg.end()).c_str(), title.c_str(), MB_ICONERROR | MB_OK);
}

static std::wstring W(const std::string& s) {
	return std::wstring(s.begin(), s.end());
}

static std::wstring GetExecutableDir() {
	wchar_t buffer[MAX_PATH];
	GetModuleFileNameW(nullptr, buffer, MAX_PATH);
	fs::path p(buffer);
	return p.parent_path().wstring();
}

static void AppendText(HWND edit, const std::wstring& text) {
	int len = GetWindowTextLengthW(edit);
	SendMessageW(edit, EM_SETSEL, (WPARAM)len, (LPARAM)len);
	SendMessageW(edit, EM_REPLACESEL, FALSE, (LPARAM)text.c_str());
}

static LRESULT CALLBACK WndProc(HWND hwnd, UINT msg, WPARAM wParam, LPARAM lParam) {
    AppState* state = reinterpret_cast<AppState*>(GetWindowLongPtrW(hwnd, GWLP_USERDATA));
	static HWND hEdit;
	static HWND hToolbar;
	static HWND hStatus;
	switch (msg) {
    case WM_CREATE: {
        // Set up per-window state pointer from lpCreateParams first
        LPCREATESTRUCTW cs = reinterpret_cast<LPCREATESTRUCTW>(lParam);
        if (cs && cs->lpCreateParams) {
            SetWindowLongPtrW(hwnd, GWLP_USERDATA, (LONG_PTR)cs->lpCreateParams);
            state = reinterpret_cast<AppState*>(cs->lpCreateParams);
        }

        InitCommonControls();
        ULONG_PTR gdipToken = 0; Gdiplus::GdiplusStartupInput gdipSI; Gdiplus::GdiplusStartup(&gdipToken, &gdipSI, nullptr);
        SetPropW(hwnd, L"__gdipToken", (HANDLE)gdipToken);
        std::wstring exeDir = GetExecutableDir();
        std::wstring logoPath = exeDir + L"/logo.png";
        if (state) state->logo = new Gdiplus::Image(logoPath.c_str());
		hToolbar = CreateWindowExW(0, TOOLBARCLASSNAMEW, nullptr, WS_CHILD | WS_VISIBLE | TBSTYLE_FLAT | TBSTYLE_TOOLTIPS, 0, 0, 0, 0, hwnd, (HMENU)3001, GetModuleHandleW(nullptr), nullptr);
		SendMessageW(hToolbar, TB_BUTTONSTRUCTSIZE, (WPARAM)sizeof(TBBUTTON), 0);
        TBBUTTON tbb[5] = {};
		tbb[0] = { MAKELONG(STD_FILENEW, 0), 2001, TBSTATE_ENABLED, BTNS_BUTTON, {0}, 0, (INT_PTR)L"Add Record" };
		tbb[1] = { MAKELONG(STD_FIND, 0), 2002, TBSTATE_ENABLED, BTNS_BUTTON, {0}, 0, (INT_PTR)L"Query VIN" };
		tbb[2] = { MAKELONG(STD_PROPERTIES, 0), 2102, TBSTATE_ENABLED, BTNS_BUTTON, {0}, 0, (INT_PTR)L"Mechanics" };
		tbb[3] = { MAKELONG(STD_FILESAVE, 0), 2401, TBSTATE_ENABLED, BTNS_BUTTON, {0}, 0, (INT_PTR)L"Export CSV" };
        tbb[4] = { MAKELONG(STD_REDOW, 0), 2501, TBSTATE_ENABLED, BTNS_BUTTON, {0}, 0, (INT_PTR)L"Refresh" };
		SendMessageW(hToolbar, TB_SETIMAGELIST, 0, (LPARAM)ImageList_Create(16, 16, ILC_COLOR32 | ILC_MASK, 4, 4));
		SendMessageW(hToolbar, TB_LOADIMAGES, (WPARAM)IDB_STD_SMALL_COLOR, (LPARAM)HINST_COMMCTRL);
        SendMessageW(hToolbar, TB_ADDBUTTONS, (WPARAM)5, (LPARAM)&tbb);

		hStatus = CreateWindowExW(0, STATUSCLASSNAMEW, nullptr, WS_CHILD | WS_VISIBLE, 0, 0, 0, 0, hwnd, (HMENU)3002, GetModuleHandleW(nullptr), nullptr);

        LOGFONTW lf{}; SystemParametersInfoW(SPI_GETICONTITLELOGFONT, sizeof(lf), &lf, 0);
        lstrcpyW(lf.lfFaceName, L"Segoe UI"); lf.lfHeight = -16;
        if (state) {
            state->hFont = CreateFontIndirectW(&lf);
            state->hBg = CreateSolidBrush(RGB(248, 250, 252));
            state->hHeaderBg = CreateSolidBrush(RGB(226, 232, 240));
            state->hButtonBg = CreateSolidBrush(RGB(217, 239, 255));
        }

		hEdit = CreateWindowExW(WS_EX_CLIENTEDGE, L"EDIT", L"",
			WS_CHILD | WS_VISIBLE | ES_MULTILINE | ES_AUTOVSCROLL | ES_READONLY | WS_VSCROLL,
			0, 0, 0, 0, hwnd, (HMENU)1001, GetModuleHandleW(nullptr), nullptr);
        if (state) {
            SendMessageW(hEdit, WM_SETFONT, (WPARAM)state->hFont, TRUE);
            SendMessageW(hStatus, WM_SETFONT, (WPARAM)state->hFont, TRUE);
        }

		// Welcome buttons (large)
        state->hBtnAdd = CreateWindowExW(0, L"BUTTON", L"Add Sample Record", WS_CHILD | WS_VISIBLE | BS_PUSHBUTTON,
			0, 0, 0, 0, hwnd, (HMENU)4001, GetModuleHandleW(nullptr), nullptr);
		state->hBtnQuery = CreateWindowExW(0, L"BUTTON", L"Query Sample VIN", WS_CHILD | WS_VISIBLE | BS_PUSHBUTTON,
			0, 0, 0, 0, hwnd, (HMENU)4002, GetModuleHandleW(nullptr), nullptr);
		state->hBtnMechanics = CreateWindowExW(0, L"BUTTON", L"List Mechanics", WS_CHILD | WS_VISIBLE | BS_PUSHBUTTON,
			0, 0, 0, 0, hwnd, (HMENU)4003, GetModuleHandleW(nullptr), nullptr);
        state->hBtnExport = CreateWindowExW(0, L"BUTTON", L"Export CSV", WS_CHILD | WS_VISIBLE | BS_PUSHBUTTON,
            0, 0, 0, 0, hwnd, (HMENU)4004, GetModuleHandleW(nullptr), nullptr);
        state->hBtnRefresh = CreateWindowExW(0, L"BUTTON", L"Refresh", WS_CHILD | WS_VISIBLE | BS_PUSHBUTTON,
            0, 0, 0, 0, hwnd, (HMENU)4005, GetModuleHandleW(nullptr), nullptr);
        if (state) {
            SendMessageW(state->hBtnAdd, WM_SETFONT, (WPARAM)state->hFont, TRUE);
            SendMessageW(state->hBtnQuery, WM_SETFONT, (WPARAM)state->hFont, TRUE);
            SendMessageW(state->hBtnMechanics, WM_SETFONT, (WPARAM)state->hFont, TRUE);
            SendMessageW(state->hBtnExport, WM_SETFONT, (WPARAM)state->hFont, TRUE);
            SendMessageW(state->hBtnRefresh, WM_SETFONT, (WPARAM)state->hFont, TRUE);
        }
		return 0;
	}
	case WM_SIZE: {
		SendMessageW(hToolbar, TB_AUTOSIZE, 0, 0);
		SendMessageW(hStatus, WM_SIZE, 0, 0);
		RECT rc; GetClientRect(hwnd, &rc);
		RECT rcTb; GetWindowRect(hToolbar, &rcTb); int tbH = rcTb.bottom - rcTb.top;
		RECT rcSb; GetWindowRect(hStatus, &rcSb); int sbH = rcSb.bottom - rcSb.top;
		int startY = tbH + kHeaderHeight;
		MoveWindow(hEdit, 0, startY, rc.right - rc.left, rc.bottom - rc.top - startY - sbH, TRUE);
		// Layout welcome buttons centered
		if (state && state->showWelcome) {
        int btnW = 220, btnH = 40, gap = 12;
        int totalH = btnH * 3 + gap * 2;
			int y = startY + ((rc.bottom - rc.top - startY - sbH) - totalH) / 2;
			int xLeft = (rc.right - rc.left) / 2 - (btnW + gap/2);
			int xRight = (rc.right - rc.left) / 2 + (gap/2);
			MoveWindow(state->hBtnAdd, xLeft, y, btnW, btnH, TRUE);
			MoveWindow(state->hBtnQuery, xRight, y, btnW, btnH, TRUE);
        MoveWindow(state->hBtnMechanics, xLeft, y + btnH + gap, btnW, btnH, TRUE);
        MoveWindow(state->hBtnExport, xRight, y + btnH + gap, btnW, btnH, TRUE);
        MoveWindow(state->hBtnRefresh, (rc.right - rc.left - btnW) / 2, y + (btnH + gap) * 2, btnW, btnH, TRUE);
		}
		return 0;
	}
    case WM_PAINT: {
		PAINTSTRUCT ps; HDC hdc = BeginPaint(hwnd, &ps);
		RECT rc; GetClientRect(hwnd, &rc);
		int headerH = 56;
        HBRUSH headerBrush = state && state->hHeaderBg ? state->hHeaderBg : CreateSolidBrush(RGB(233, 238, 243));
		RECT hdr{ rc.left, rc.top, rc.right, rc.top + headerH };
        FillRect(hdc, &hdr, headerBrush);
        if (!(state && state->hHeaderBg)) DeleteObject(headerBrush);
		if (state && state->logo && state->logo->GetLastStatus() == Gdiplus::Ok) {
			Gdiplus::Graphics g(hdc);
			g.SetInterpolationMode(Gdiplus::InterpolationModeHighQualityBicubic);
			int lh = headerH - 12; int lw = (int)((double)state->logo->GetWidth() * (double)lh / (double)state->logo->GetHeight());
			g.DrawImage(state->logo, 8, 6, lw, lh);
		}
		EndPaint(hwnd, &ps);
		return 0;
	}
	case WM_CTLCOLORSTATIC: {
		HDC hdc = (HDC)wParam;
		SetBkColor(hdc, RGB(248, 250, 252));
		return (LRESULT)state->hBg;
	}
    case WM_CTLCOLORBTN: {
        HDC hdc = (HDC)wParam;
        SetBkColor(hdc, RGB(217, 239, 255));
        return (LRESULT)(state && state->hButtonBg ? state->hButtonBg : GetSysColorBrush(COLOR_BTNFACE));
    }
	case WM_COMMAND: {
		// Welcome button clicks mapped to existing commands
        if (LOWORD(wParam) >= 4001 && LOWORD(wParam) <= 4005) {
			if (state && state->showWelcome) {
				state->showWelcome = false;
				ShowWindow(state->hBtnAdd, SW_HIDE);
				ShowWindow(state->hBtnQuery, SW_HIDE);
				ShowWindow(state->hBtnMechanics, SW_HIDE);
				ShowWindow(state->hBtnExport, SW_HIDE);
                ShowWindow(state->hBtnRefresh, SW_HIDE);
			}
			if (LOWORD(wParam) == 4001) SendMessageW(hwnd, WM_COMMAND, 2001, 0);
			if (LOWORD(wParam) == 4002) SendMessageW(hwnd, WM_COMMAND, 2002, 0);
			if (LOWORD(wParam) == 4003) SendMessageW(hwnd, WM_COMMAND, 2102, 0);
			if (LOWORD(wParam) == 4004) SendMessageW(hwnd, WM_COMMAND, 2401, 0);
            if (LOWORD(wParam) == 4005) SendMessageW(hwnd, WM_COMMAND, 2501, 0);
			return 0;
		}
		if (LOWORD(wParam) == 2001) { // Add sample record
			vsrm::ServiceRecord rec{};
			rec.vin = "JT123TESTVIN00001";
			rec.customerName = "John Doe";
			rec.serviceDate = "2025-09-16";
			rec.description = "Oil change and filter replacement";
			rec.mechanic = "A. Mechanic";
			auto id = state->db.addServiceRecord(rec);
			if (!id) {
				ShowError(hwnd, L"DB Error", state->db.getLastError());
			} else {
				AppendText(hEdit, std::wstring(L"Inserted sample record with ID: ") + std::to_wstring(*id) + L"\r\n");
				SendMessageW(hStatus, SB_SETTEXT, 0, (LPARAM)L"Added sample record");
			}
			return 0;
		}
		if (LOWORD(wParam) == 2002) { // Query by VIN
			auto rows = state->db.listServiceRecordsByVin("JT123TESTVIN00001");
			AppendText(hEdit, L"Records for VIN JT123TESTVIN00001:\r\n");
			SendMessageW(hStatus, SB_SETTEXT, 0, (LPARAM)L"Queried VIN");
			for (const auto& r : rows) {
				std::wstring line = std::wstring(L"  [") + std::to_wstring(r.id) + L"] " + W(r.serviceDate) + L" - " + W(r.description) + L" (" + W(r.mechanic) + L")\r\n";
				AppendText(hEdit, line);
			}
			return 0;
		}
		if (LOWORD(wParam) == 2101) { // Add sample mechanic
			vsrm::Mechanic m{}; m.name = "Jane Smith"; m.skill = "Engine"; m.active = true;
			auto id = state->db.addMechanic(m);
			if (!id) ShowError(hwnd, L"DB Error", state->db.getLastError()); else { AppendText(hEdit, std::wstring(L"Inserted mechanic ID: ") + std::to_wstring(*id) + L"\r\n"); SendMessageW(hStatus, SB_SETTEXT, 0, (LPARAM)L"Added mechanic"); }
			return 0;
		}
		if (LOWORD(wParam) == 2102) { // List mechanics
			auto list = state->db.listMechanics(true);
			AppendText(hEdit, L"Active mechanics:\r\n");
			SendMessageW(hStatus, SB_SETTEXT, 0, (LPARAM)L"Listed mechanics");
			for (const auto& m : list) {
				std::wstring line = std::wstring(L"  [") + std::to_wstring(m.id) + L"] " + W(m.name) + L" - " + W(m.skill) + L"\r\n";
				AppendText(hEdit, line);
			}
			return 0;
		}
		if (LOWORD(wParam) == 2201) { // Add sample appointment
			vsrm::Appointment a{}; a.vin = "JT123TESTVIN00001"; a.customerName = "John Doe"; a.scheduledAt = "2025-09-20T09:00:00"; a.status = "scheduled";
			auto id = state->db.addAppointment(a);
			if (!id) ShowError(hwnd, L"DB Error", state->db.getLastError()); else { AppendText(hEdit, std::wstring(L"Inserted appointment ID: ") + std::to_wstring(*id) + L"\r\n"); SendMessageW(hStatus, SB_SETTEXT, 0, (LPARAM)L"Added appointment"); }
			return 0;
		}
		if (LOWORD(wParam) == 2202) { // List appointments by VIN
			auto list = state->db.listAppointmentsByVin("JT123TESTVIN00001");
			AppendText(hEdit, L"Appointments for sample VIN:\r\n");
			SendMessageW(hStatus, SB_SETTEXT, 0, (LPARAM)L"Listed appointments");
			for (const auto& a : list) {
				std::wstring line = std::wstring(L"  [") + std::to_wstring(a.id) + L"] " + W(a.scheduledAt) + L" - " + W(a.status) + L"\r\n";
				AppendText(hEdit, line);
			}
			return 0;
		}
		if (LOWORD(wParam) == 2301) { // Add sample assignment (mechanic 1 to appt 1)
			vsrm::Assignment s{}; s.appointmentId = 1; s.mechanicId = 1; s.assignedAt = "2025-09-19T12:00:00";
			auto id = state->db.addAssignment(s);
			if (!id) ShowError(hwnd, L"DB Error", state->db.getLastError()); else { AppendText(hEdit, std::wstring(L"Inserted assignment ID: ") + std::to_wstring(*id) + L"\r\n"); SendMessageW(hStatus, SB_SETTEXT, 0, (LPARAM)L"Added assignment"); }
			return 0;
		}
		if (LOWORD(wParam) == 2302) { // List assignments for mechanic 1
			auto list = state->db.listAssignmentsByMechanic(1);
			AppendText(hEdit, L"Assignments for mechanic 1:\r\n");
			SendMessageW(hStatus, SB_SETTEXT, 0, (LPARAM)L"Listed assignments");
			for (const auto& s : list) {
				std::wstring line = std::wstring(L"  [") + std::to_wstring(s.id) + L"] appt=" + std::to_wstring(s.appointmentId) + L" assigned=" + W(s.assignedAt) + L"\r\n";
				AppendText(hEdit, line);
			}
			return 0;
		}
		if (LOWORD(wParam) == 2401) { // Export CSV for sample VIN to Desktop
			PWSTR pDesktop = nullptr; std::wstring outPath;
			if (SUCCEEDED(SHGetKnownFolderPath(FOLDERID_Desktop, 0, nullptr, &pDesktop))) {
				std::wstring desktop(pDesktop); CoTaskMemFree(pDesktop);
				outPath = desktop + L"/vsrm_history_JT123TESTVIN00001.csv";
			} else {
				outPath = GetExecutableDir() + L"/vsrm_history_JT123TESTVIN00001.csv";
			}
			bool ok = state->db.exportServiceHistoryCsv("JT123TESTVIN00001", std::string(outPath.begin(), outPath.end()));
			if (!ok) ShowError(hwnd, L"Export Failed", state->db.getLastError()); else { AppendText(hEdit, L"Exported CSV to " + outPath + L"\r\n"); SendMessageW(hStatus, SB_SETTEXT, 0, (LPARAM)L"CSV exported"); }
			return 0;
		}
		if (LOWORD(wParam) == 2402) { // Count by date range (fixed sample)
			int cnt = state->db.countServiceRecordsByDateRange("2025-01-01", "2025-12-31");
			AppendText(hEdit, std::wstring(L"Records in 2025: ") + std::to_wstring(cnt) + L"\r\n");
			SendMessageW(hStatus, SB_SETTEXT, 0, (LPARAM)L"Report updated");
			return 0;
		}
        if (LOWORD(wParam) == 2501) { // Refresh: re-run key lists
            SendMessageW(hwnd, WM_COMMAND, 2002, 0);
            SendMessageW(hwnd, WM_COMMAND, 2102, 0);
            SendMessageW(hwnd, WM_COMMAND, 2402, 0);
            SendMessageW(hStatus, SB_SETTEXT, 0, (LPARAM)L"Refreshed");
            return 0;
        }
		return 0;
	}
	case WM_DESTROY:
		if (state && state->logo) { delete state->logo; state->logo = nullptr; }
		if (GetPropW(hwnd, L"__gdipToken")) { ULONG_PTR t = (ULONG_PTR)GetPropW(hwnd, L"__gdipToken"); Gdiplus::GdiplusShutdown(t); RemovePropW(hwnd, L"__gdipToken"); }
		PostQuitMessage(0);
		return 0;
	}
	return DefWindowProcW(hwnd, msg, wParam, lParam);
}

static HMENU CreateMainMenu() {
	HMENU hMenu = CreateMenu();
	HMENU hFile = CreatePopupMenu();
	AppendMenuW(hFile, MF_STRING, 2001, L"Add Sample Record\tCtrl+N");
	AppendMenuW(hFile, MF_STRING, 2002, L"Query Sample VIN\tCtrl+Q");
	AppendMenuW(hFile, MF_SEPARATOR, 0, nullptr);
	AppendMenuW(hFile, MF_STRING, 2003, L"Exit");
	AppendMenuW(hMenu, MF_POPUP, (UINT_PTR)hFile, L"&File");

	HMENU hData = CreatePopupMenu();
	AppendMenuW(hData, MF_STRING, 2101, L"Add Sample Mechanic");
	AppendMenuW(hData, MF_STRING, 2102, L"List Mechanics");
	AppendMenuW(hData, MF_SEPARATOR, 0, nullptr);
	AppendMenuW(hData, MF_STRING, 2201, L"Add Sample Appointment");
	AppendMenuW(hData, MF_STRING, 2202, L"List Appointments (Sample VIN)");
	AppendMenuW(hData, MF_SEPARATOR, 0, nullptr);
	AppendMenuW(hData, MF_STRING, 2301, L"Add Sample Assignment (Mech 1 → Appt 1)");
	AppendMenuW(hData, MF_STRING, 2302, L"List Assignments (Mechanic 1)");
	AppendMenuW(hMenu, MF_POPUP, (UINT_PTR)hData, L"&Data");

	HMENU hReports = CreatePopupMenu();
	AppendMenuW(hReports, MF_STRING, 2401, L"Export Service History CSV (Sample VIN)");
	AppendMenuW(hReports, MF_STRING, 2402, L"Count Records in 2025");
	AppendMenuW(hMenu, MF_POPUP, (UINT_PTR)hReports, L"&Reports");
	return hMenu;
}

int APIENTRY wWinMain(HINSTANCE hInstance, HINSTANCE, LPWSTR, int nCmdShow) {
	AppState state{};

	std::wstring exeDir = GetExecutableDir();
	state.dbPath = exeDir + L"/vsrm.db";

	// Open DB
	if (!state.db.openOrCreate(std::string(state.dbPath.begin(), state.dbPath.end()))) {
		const std::string err = state.db.getLastError();
		MessageBoxW(nullptr, W(err).c_str(), L"Failed to open database", MB_ICONERROR);
		return -1;
	}
	// Initialize schema
	std::wstring schemaPath = exeDir + L"/schema.sql";
	state.db.initializeSchema(std::string(schemaPath.begin(), schemaPath.end()));

	WNDCLASSEXW wc{ sizeof(WNDCLASSEXW) };
	wc.style = CS_HREDRAW | CS_VREDRAW;
	wc.lpfnWndProc = WndProc;
	wc.hInstance = hInstance;
	wc.hIcon = LoadIcon(nullptr, IDI_APPLICATION);
	wc.hCursor = LoadCursor(nullptr, IDC_ARROW);
	wc.hbrBackground = (HBRUSH)(COLOR_WINDOW + 1);
	wc.lpszClassName = kClassName;
	wc.hIconSm = LoadIcon(nullptr, IDI_APPLICATION);

	if (!RegisterClassExW(&wc)) {
		MessageBoxW(nullptr, L"Failed to register window class", L"Error", MB_ICONERROR);
		return -1;
	}

    HWND hwnd = CreateWindowExW(0, kClassName, L"Toyota Zambia - Vehicle Service Records Manager", WS_OVERLAPPEDWINDOW,
        CW_USEDEFAULT, CW_USEDEFAULT, 900, 600, nullptr, nullptr, hInstance, &state);

	if (!hwnd) {
		MessageBoxW(nullptr, L"Failed to create window", L"Error", MB_ICONERROR);
		return -1;
	}

	SetWindowLongPtrW(hwnd, GWLP_USERDATA, (LONG_PTR)&state);
	SetMenu(hwnd, CreateMainMenu());
	ShowWindow(hwnd, nCmdShow);
	UpdateWindow(hwnd);

	MSG msg;
	while (GetMessageW(&msg, nullptr, 0, 0)) {
		TranslateMessage(&msg);
		DispatchMessageW(&msg);
	}
	return (int)msg.wParam;
}


