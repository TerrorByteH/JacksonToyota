#include <windows.h>
#include <string>
#include <vector>
#include <filesystem>
#include <commctrl.h>
#include <gdiplus.h>
#include <shlobj.h>
#include <fstream>
#include <codecvt>
#include <locale>

#include "../app/Database.h"

namespace fs = std::filesystem;

static std::wstring W(const std::string& s) {
    if (s.empty()) return std::wstring();
    int size_needed = MultiByteToWideChar(CP_UTF8, 0, &s[0], (int)s.size(), NULL, 0);
    std::wstring wstrTo(size_needed, 0);
    MultiByteToWideChar(CP_UTF8, 0, &s[0], (int)s.size(), &wstrTo[0], size_needed);
    return wstrTo;
}

static std::string A(const std::wstring& w) {
    if (w.empty()) return std::string();
    int size_needed = WideCharToMultiByte(CP_UTF8, 0, &w[0], (int)w.size(), NULL, 0, NULL, NULL);
    std::string strTo(size_needed, 0);
    WideCharToMultiByte(CP_UTF8, 0, &w[0], (int)w.size(), &strTo[0], size_needed, NULL, NULL);
    return strTo;
}

// Edit control helpers for placeholders/padding (may require Common Controls v6)
#ifndef ECM_FIRST
#define ECM_FIRST 0x1500
#endif
#ifndef EM_SETCUEBANNER
#define EM_SETCUEBANNER (ECM_FIRST + 1)
#endif
#ifndef EM_SETMARGINS
#define EM_SETMARGINS 0xD3
#endif
#ifndef EC_LEFTMARGIN
#define EC_LEFTMARGIN 0x1
#endif
#ifndef EC_RIGHTMARGIN
#define EC_RIGHTMARGIN 0x2
#endif

static void ShowError(HWND hwnd, const wchar_t* title, const std::string& msg) {
	std::wstring wmsg(msg.begin(), msg.end());
	MessageBoxW(hwnd, wmsg.c_str(), title, MB_ICONERROR | MB_OK);
}

// Forward declaration so we can reference AppState* before full definition
struct AppState;

static void CenterWindowOnParent(HWND hwnd, HWND parent);

struct LoginCtx {
    AppState* state{};
    HFONT font{};
    HWND eUser{};
    HWND ePass{};
    HWND chkRemember{};
    HWND btnShow{};
    HWND linkForgot{};
    HWND btnSignIn{};
    bool authed{};
    // GDI+ visuals
    ULONG_PTR gdipToken{};
    Gdiplus::Image* bg{};
    Gdiplus::Image* logo{};
    int headerH{80};
    // Card geometry cached on size
    RECT cardRc{0,0,0,0};
};

// Build a rounded-rectangle path for GDI+
static void BuildRoundedRectPath(Gdiplus::GraphicsPath& path, const Gdiplus::Rect& rc, int radius) {
    int d = radius * 2;
    path.Reset();
    path.AddArc(rc.X, rc.Y, d, d, 180.0f, 90.0f);
    path.AddArc(rc.X + rc.Width - d, rc.Y, d, d, 270.0f, 90.0f);
    path.AddArc(rc.X + rc.Width - d, rc.Y + rc.Height - d, d, d, 0.0f, 90.0f);
    path.AddArc(rc.X, rc.Y + rc.Height - d, d, d, 90.0f, 90.0f);
    path.CloseFigure();
}

static LRESULT CALLBACK LoginProc(HWND hwnd, UINT msg, WPARAM wParam, LPARAM lParam) {
    LoginCtx* ctx = reinterpret_cast<LoginCtx*>(GetWindowLongPtrW(hwnd, GWLP_USERDATA));
    switch (msg) {
    case WM_CREATE: {
        auto cs = reinterpret_cast<LPCREATESTRUCTW>(lParam);
        ctx = reinterpret_cast<LoginCtx*>(cs->lpCreateParams);
        SetWindowLongPtrW(hwnd, GWLP_USERDATA, (LONG_PTR)ctx);
        // Init GDI+ for this dialog
        Gdiplus::GdiplusStartupInput si; Gdiplus::GdiplusStartup(&ctx->gdipToken, &si, nullptr);
        // Load images from exe dir
        wchar_t mod[MAX_PATH]; GetModuleFileNameW(nullptr, mod, MAX_PATH);
        fs::path exe(mod); fs::path dir = exe.parent_path();
        ctx->bg = new Gdiplus::Image((dir / L"login_bg2.png").c_str());
        ctx->logo = new Gdiplus::Image((dir / L"logo.png").c_str());
        // Create controls (layout positioned in WM_SIZE)
        ctx->eUser = CreateWindowExW(0, L"EDIT", L"", WS_CHILD | WS_VISIBLE | ES_AUTOHSCROLL, 0, 0, 0, 0, hwnd, (HMENU)6001, cs->hInstance, nullptr);
        ctx->ePass = CreateWindowExW(0, L"EDIT", L"", WS_CHILD | WS_VISIBLE | ES_PASSWORD | ES_AUTOHSCROLL, 0, 0, 0, 0, hwnd, (HMENU)6002, cs->hInstance, nullptr);
        ctx->btnShow = CreateWindowExW(0, L"BUTTON", L"\u25CF", WS_CHILD | WS_VISIBLE | BS_PUSHBUTTON, 0, 0, 0, 0, hwnd, (HMENU)6005, cs->hInstance, nullptr);
        ctx->chkRemember = CreateWindowExW(0, L"BUTTON", L"Remember me", WS_CHILD | WS_VISIBLE | BS_AUTOCHECKBOX, 0, 0, 0, 0, hwnd, (HMENU)6006, cs->hInstance, nullptr);
        ctx->linkForgot = CreateWindowExW(0, L"STATIC", L"Forgot password?", WS_CHILD | WS_VISIBLE | SS_NOTIFY, 0, 0, 0, 0, hwnd, (HMENU)6007, cs->hInstance, nullptr);
        ctx->btnSignIn = CreateWindowExW(0, L"BUTTON", L"Sign In", WS_CHILD | WS_VISIBLE | BS_DEFPUSHBUTTON, 0, 0, 0, 0, hwnd, (HMENU)6003, cs->hInstance, nullptr);
        CreateWindowExW(0, L"BUTTON", L"Cancel", WS_CHILD | WS_VISIBLE, 0, 0, 0, 0, hwnd, (HMENU)6004, cs->hInstance, nullptr);
        if (ctx->font) {
            SendMessageW(hwnd, WM_SETFONT, (WPARAM)ctx->font, TRUE);
            SendMessageW(ctx->eUser, WM_SETFONT, (WPARAM)ctx->font, TRUE);
            SendMessageW(ctx->ePass, WM_SETFONT, (WPARAM)ctx->font, TRUE);
            SendMessageW(ctx->btnShow, WM_SETFONT, (WPARAM)ctx->font, TRUE);
            SendMessageW(ctx->chkRemember, WM_SETFONT, (WPARAM)ctx->font, TRUE);
            SendMessageW(ctx->linkForgot, WM_SETFONT, (WPARAM)ctx->font, TRUE);
            SendMessageW(ctx->btnSignIn, WM_SETFONT, (WPARAM)ctx->font, TRUE);
        }
        // Placeholders and inner padding
        SendMessageW(ctx->eUser, EM_SETCUEBANNER, TRUE, (LPARAM)L"Email address");
        SendMessageW(ctx->ePass, EM_SETCUEBANNER, TRUE, (LPARAM)L"Password");
        SendMessageW(ctx->eUser, EM_SETMARGINS, EC_LEFTMARGIN | EC_RIGHTMARGIN, MAKELPARAM(10, 10));
        SendMessageW(ctx->ePass, EM_SETMARGINS, EC_LEFTMARGIN | EC_RIGHTMARGIN, MAKELPARAM(10, 10));
        // Make link look like a link (blue + underline)
        SetWindowTextW(ctx->linkForgot, L"Forgot password?");
        return 0;
    }
    case WM_SETCURSOR: {
        if (ctx && (HWND)wParam == ctx->linkForgot) {
            SetCursor(LoadCursor(nullptr, IDC_HAND));
            return TRUE;
        }
        return 0;
    }
    case WM_SIZE: {
        if (!ctx) return 0;
        RECT rc; GetClientRect(hwnd, &rc);
        // Card dimensions
        int cardW = 420;
        int cardH = 520;
        int xCard = (rc.right - rc.left - cardW) / 2;
        int yCard = (rc.bottom - rc.top - cardH) / 2;
        if (yCard < 20) yCard = 20;
        ctx->cardRc = { xCard, yCard, xCard + cardW, yCard + cardH };

        // Inner layout
        int fieldW = cardW - 80;
        int fieldH = 36;
        int xField = xCard + 40;
        int yField = yCard + 180;
        MoveWindow(ctx->eUser, xField, yField, fieldW, fieldH, TRUE);
        MoveWindow(ctx->ePass, xField, yField + 56, fieldW - 44, fieldH, TRUE);
        MoveWindow(ctx->btnShow, xField + fieldW - 40, yField + 56, 40, fieldH, TRUE);
        MoveWindow(ctx->chkRemember, xField, yField + 110, 140, 22, TRUE);
        MoveWindow(ctx->linkForgot, xField + fieldW - 150, yField + 110, 150, 22, TRUE);
        MoveWindow(ctx->btnSignIn, xField, yField + 150, fieldW, 40, TRUE);
        MoveWindow(GetDlgItem(hwnd, 6004), xField, yField + 196, fieldW, 28, TRUE);
        return 0;
    }
    case WM_PAINT: {
        PAINTSTRUCT ps; HDC hdc = BeginPaint(hwnd, &ps);
        RECT rc; GetClientRect(hwnd, &rc);
        Gdiplus::Graphics g(hdc);
        g.SetSmoothingMode(Gdiplus::SmoothingModeHighQuality);
        // Soft background
        Gdiplus::LinearGradientBrush bkg(
            Gdiplus::Point(0, 0), Gdiplus::Point(rc.right, rc.bottom),
            Gdiplus::Color(255, 242, 245, 250), Gdiplus::Color(255, 229, 235, 240));
        g.FillRectangle(&bkg, 0, 0, rc.right, rc.bottom);
        // Draw background image fully opaque (no transparency)
        if (ctx && ctx->bg && ctx->bg->GetLastStatus() == Gdiplus::Ok) {
            g.DrawImage(ctx->bg, 0, 0, rc.right, rc.bottom);
        }
        // Neumorphic card with soft shadows
        if (ctx) {
            int r = 24;
            Gdiplus::GraphicsPath path; Gdiplus::Rect cardRect(ctx->cardRc.left, ctx->cardRc.top, ctx->cardRc.right - ctx->cardRc.left, ctx->cardRc.bottom - ctx->cardRc.top);
            BuildRoundedRectPath(path, cardRect, r);
            // Drop shadow
            Gdiplus::SolidBrush shadow(Gdiplus::Color(40, 0, 0, 0));
            g.FillPath(&shadow, &path);
            Gdiplus::Matrix m; m.Translate(0.0f, -4.0f); g.SetTransform(&m);
            Gdiplus::SolidBrush card(Gdiplus::Color(255, 244, 247, 252));
            g.FillPath(&card, &path);
            g.ResetTransform();
            // Avatar circle
            int cx = (ctx->cardRc.left + ctx->cardRc.right) / 2;
            int top = ctx->cardRc.top + 40;
            Gdiplus::SolidBrush bubble(Gdiplus::Color(255, 235, 240, 247));
            g.FillEllipse(&bubble, cx - 36, top, 72, 72);
            // Title and subtitle
            Gdiplus::Font title(L"Segoe UI", 22, Gdiplus::FontStyleBold, Gdiplus::UnitPixel);
            Gdiplus::Font subtitle(L"Segoe UI", 12, Gdiplus::FontStyleRegular, Gdiplus::UnitPixel);
            Gdiplus::SolidBrush text(Gdiplus::Color(255, 55, 65, 80));
            Gdiplus::StringFormat fmt; fmt.SetAlignment(Gdiplus::StringAlignmentCenter);
            g.DrawString(L"Welcome back", -1, &title, Gdiplus::RectF((Gdiplus::REAL)ctx->cardRc.left, (Gdiplus::REAL)(top + 88), (Gdiplus::REAL)(cardRect.Width), 30.0f), &fmt, &text);
            g.DrawString(L"Please sign in to continue", -1, &subtitle, Gdiplus::RectF((Gdiplus::REAL)ctx->cardRc.left, (Gdiplus::REAL)(top + 118), (Gdiplus::REAL)(cardRect.Width), 20.0f), &fmt, &text);
        }
        EndPaint(hwnd, &ps);
        return 0;
    }
    case WM_COMMAND: {
        int id = LOWORD(wParam);
        if (id == 6005) { // show/hide password
            static bool showing = false; showing = !showing;
            SendMessageW(ctx->ePass, EM_SETPASSWORDCHAR, showing ? 0 : (WPARAM)L'\u2022', 0);
            InvalidateRect(ctx->ePass, nullptr, TRUE); SetFocus(ctx->ePass);
            return 0;
        }
        if (id == 6003) {
            wchar_t u[128], p[128]; GetWindowTextW(ctx->eUser, u, 128); GetWindowTextW(ctx->ePass, p, 128);
            std::string su(u, u + wcslen(u)); std::string sp(p, p + wcslen(p));
            ctx->authed = (su == "admin" && sp == "admin");
            if (!ctx->authed) MessageBoxW(hwnd, L"Invalid credentials", L"Login", MB_ICONWARNING);
            else DestroyWindow(hwnd);
            return 0;
        }
        if (id == 6004) { ctx->authed = false; DestroyWindow(hwnd); return 0; }
        break;
    }
    case WM_CLOSE: ctx->authed = false; DestroyWindow(hwnd); return 0;
    case WM_NCDESTROY: {
        if (ctx) {
            if (ctx->bg) { delete ctx->bg; ctx->bg = nullptr; }
            if (ctx->logo) { delete ctx->logo; ctx->logo = nullptr; }
            if (ctx->gdipToken) Gdiplus::GdiplusShutdown(ctx->gdipToken);
        }
        return 0;
    }
    }
    return DefWindowProcW(hwnd, msg, wParam, lParam);
}

static bool ShowLoginDialog(AppState* state, HINSTANCE hInst, HFONT hFont) {
    WNDCLASSW wc{}; wc.lpszClassName = L"VSRM_Login"; wc.hInstance = hInst; wc.lpfnWndProc = LoginProc; wc.hCursor = LoadCursor(nullptr, IDC_ARROW);
    static ATOM cls = RegisterClassW(&wc); (void)cls;
    LoginCtx ctx{}; ctx.state = state; ctx.font = hFont; ctx.authed = false;
    RECT wa{}; SystemParametersInfoW(SPI_GETWORKAREA, 0, &wa, 0);
    HWND dlg = CreateWindowExW(WS_EX_APPWINDOW, L"VSRM_Login", L"Login",
        WS_POPUP, wa.left, wa.top, wa.right - wa.left, wa.bottom - wa.top, nullptr, nullptr, hInst, &ctx);
    ShowWindow(dlg, SW_SHOW);
    MSG m; while (IsWindow(dlg) && GetMessageW(&m, nullptr, 0, 0)) { TranslateMessage(&m); DispatchMessageW(&m); }
    return ctx.authed;
}

struct AppState {
	vsrm::Database db;
	std::wstring dbPath;
    HFONT hFont{};
    HBRUSH hBg{};           // main background
    HBRUSH hHeaderBg{};     // banner background
    HBRUSH hButtonBg{};     // welcome button background
    HBRUSH hBtnPrimary{};   // primary buttons
    HBRUSH hBtnDanger{};    // danger buttons
    HBRUSH hBtnSuccess{};   // success buttons (Refresh)
    Gdiplus::Image* logo{};
    // Welcome panel
    bool showWelcome{true};
    HWND hBtnAdd{};
    HWND hBtnQuery{};
    HWND hBtnMechanics{};
    HWND hBtnExport{};
    HWND hBtnRefresh{};
    // New layout elements
    HWND hLeftNav{};
    HWND hNavDashboard{};
    HWND hNavVehicles{};
    HWND hNavMechanics{};
    HWND hNavReports{};
    HFONT hIconFont{}; // Segoe MDL2 Assets for icons
    // Search bar
    HWND hSearchVin{};
    HWND hSearchDateFrom{};
    HWND hSearchDateTo{};
    HWND hSearchMechanic{};
    HWND hChkDue{};
    // Data grid
    HWND hList{};
    // Views
    enum class View { Vehicles, Reports } currentView{ View::Vehicles };
    HWND hReportsPanel{};
    // Settings persistence
    std::wstring settingsPath;
};

static const wchar_t* kClassName = L"VSRMMainWindow";
static const int kHeaderHeight = 56;
static void SaveUiSettings(AppState* state) {
    if (!state) return;
    std::wofstream out(state->settingsPath);
    if (!out) return;
    // Filters
    wchar_t w[256];
    GetWindowTextW(state->hSearchVin, w, 256); out << L"vin=" << w << L"\n";
    GetWindowTextW(state->hSearchDateFrom, w, 256); out << L"from=" << w << L"\n";
    GetWindowTextW(state->hSearchDateTo, w, 256); out << L"to=" << w << L"\n";
    GetWindowTextW(state->hSearchMechanic, w, 256); out << L"mech=" << w << L"\n";
    out << L"due=" << (SendMessageW(state->hChkDue, BM_GETCHECK, 0, 0) == BST_CHECKED ? L"1" : L"0") << L"\n";
    // Column widths
    for (int i = 0; i < 7; ++i) {
        int wcol = ListView_GetColumnWidth(state->hList, i);
        out << L"col" << i << L'=' << wcol << L"\n";
    }
}

static void LoadUiSettings(AppState* state) {
    if (!state) return;
    std::wifstream in(state->settingsPath);
    if (!in) return;
    std::wstring line;
    while (std::getline(in, line)) {
        size_t eq = line.find(L'='); if (eq == std::wstring::npos) continue;
        std::wstring k = line.substr(0, eq), v = line.substr(eq + 1);
        if (k == L"vin") SetWindowTextW(state->hSearchVin, v.c_str());
        else if (k == L"from") SetWindowTextW(state->hSearchDateFrom, v.c_str());
        else if (k == L"to") SetWindowTextW(state->hSearchDateTo, v.c_str());
        else if (k == L"mech") SetWindowTextW(state->hSearchMechanic, v.c_str());
        else if (k == L"due") SendMessageW(state->hChkDue, BM_SETCHECK, (v == L"1"), 0);
        else if (k.rfind(L"col", 0) == 0) { int idx = _wtoi(k.c_str() + 3); int wcol = _wtoi(v.c_str()); if (idx >= 0 && idx < 7 && wcol > 20) ListView_SetColumnWidth(state->hList, idx, wcol); }
    }
}

static void SwitchView(HWND hwnd, AppState* state, AppState::View view) {
    state->currentView = view;
    BOOL veh = (view == AppState::View::Vehicles);
    ShowWindow(state->hSearchVin, veh ? SW_SHOW : SW_HIDE);
    ShowWindow(state->hSearchDateFrom, veh ? SW_SHOW : SW_HIDE);
    ShowWindow(state->hSearchDateTo, veh ? SW_SHOW : SW_HIDE);
    ShowWindow(state->hSearchMechanic, veh ? SW_SHOW : SW_HIDE);
    ShowWindow(state->hChkDue, veh ? SW_SHOW : SW_HIDE);
    ShowWindow(state->hList, veh ? SW_SHOW : SW_HIDE);
    ShowWindow(state->hReportsPanel, veh ? SW_HIDE : SW_SHOW);
    InvalidateRect(hwnd, nullptr, TRUE);
}

static void OpenRecordEditor(HWND parent, AppState* state, const std::string& vin) {
    // Prefill dialog with last record for VIN if exists
    auto rows = state->db.listServiceRecordsByVin(vin.empty() ? "JT123TESTVIN00001" : vin); // if empty, sample fallback
    HINSTANCE hi = GetModuleHandleW(nullptr);
    HWND dlg = CreateWindowExW(WS_EX_DLGMODALFRAME, L"STATIC", L"Edit Service Record",
        WS_POPUP | WS_CAPTION | WS_SYSMENU, CW_USEDEFAULT, CW_USEDEFAULT, 560, 380, parent, nullptr, hi, nullptr);
    CreateWindowExW(0, L"STATIC", L"VIN:", WS_CHILD | WS_VISIBLE, 20, 40, 120, 20, dlg, nullptr, hi, nullptr);
    HWND eVin = CreateWindowExW(WS_EX_CLIENTEDGE, L"EDIT", L"", WS_CHILD | WS_VISIBLE | ES_AUTOHSCROLL, 150, 38, 370, 26, dlg, (HMENU)5101, hi, nullptr);
    CreateWindowExW(0, L"STATIC", L"Customer:", WS_CHILD | WS_VISIBLE, 20, 74, 120, 20, dlg, nullptr, hi, nullptr);
    HWND eCust = CreateWindowExW(WS_EX_CLIENTEDGE, L"EDIT", L"", WS_CHILD | WS_VISIBLE | ES_AUTOHSCROLL, 150, 72, 370, 26, dlg, (HMENU)5102, hi, nullptr);
    CreateWindowExW(0, L"STATIC", L"Date:", WS_CHILD | WS_VISIBLE, 20, 108, 120, 20, dlg, nullptr, hi, nullptr);
    HWND eDate = CreateWindowExW(WS_EX_CLIENTEDGE, L"EDIT", L"", WS_CHILD | WS_VISIBLE | ES_AUTOHSCROLL, 150, 106, 170, 26, dlg, (HMENU)5103, hi, nullptr);
    CreateWindowExW(0, L"STATIC", L"Mechanic:", WS_CHILD | WS_VISIBLE, 20, 142, 120, 20, dlg, nullptr, hi, nullptr);
    HWND eMech = CreateWindowExW(WS_EX_CLIENTEDGE, L"EDIT", L"", WS_CHILD | WS_VISIBLE | ES_AUTOHSCROLL, 150, 140, 370, 26, dlg, (HMENU)5104, hi, nullptr);
    CreateWindowExW(0, L"STATIC", L"Description:", WS_CHILD | WS_VISIBLE, 20, 176, 120, 20, dlg, nullptr, hi, nullptr);
    HWND eDesc = CreateWindowExW(WS_EX_CLIENTEDGE, L"EDIT", L"", WS_CHILD | WS_VISIBLE | ES_AUTOVSCROLL | ES_MULTILINE, 150, 174, 370, 96, dlg, (HMENU)5105, hi, nullptr);
    HWND bSave = CreateWindowExW(0, L"BUTTON", L"Save", WS_CHILD | WS_VISIBLE | BS_DEFPUSHBUTTON, 330, 290, 90, 28, dlg, (HMENU)5190, hi, nullptr);
    HWND bCancel = CreateWindowExW(0, L"BUTTON", L"Cancel", WS_CHILD | WS_VISIBLE, 430, 290, 90, 28, dlg, (HMENU)5191, hi, nullptr);
    if (state) SendMessageW(dlg, WM_SETFONT, (WPARAM)state->hFont, TRUE);
    int editingId = 0;
    if (!rows.empty()) {
        SetWindowTextW(eVin, W(rows[0].vin).c_str());
        SetWindowTextW(eCust, W(rows[0].customerName).c_str());
        SetWindowTextW(eDate, W(rows[0].serviceDate).c_str());
        SetWindowTextW(eMech, W(rows[0].mechanic).c_str());
        SetWindowTextW(eDesc, W(rows[0].description).c_str());
        editingId = rows[0].id;
    } else if (!vin.empty()) {
        SetWindowTextW(eVin, W(vin).c_str());
    }
    CenterWindowOnParent(dlg, parent);
    ShowWindow(dlg, SW_SHOW);
    // Use IsDialogMessage for proper button handling
    MSG m; bool done = false;
    while (!done && GetMessageW(&m, nullptr, 0, 0)) {
        if (m.hwnd == dlg || IsChild(dlg, m.hwnd)) {
            if (IsDialogMessageW(dlg, &m)) continue;
        }
        if (m.message == WM_COMMAND) {
            int id = LOWORD(m.wParam);
            if (id == 5190) {
                wchar_t wb[512]; vsrm::ServiceRecord rec{};
                GetWindowTextW(eVin, wb, 512); rec.vin = std::string(wb, wb + wcslen(wb));
                GetWindowTextW(eCust, wb, 512); rec.customerName = std::string(wb, wb + wcslen(wb));
                GetWindowTextW(eDate, wb, 512); rec.serviceDate = std::string(wb, wb + wcslen(wb));
                GetWindowTextW(eDesc, wb, 512); rec.description = std::string(wb, wb + wcslen(wb));
                GetWindowTextW(eMech, wb, 512); rec.mechanic = std::string(wb, wb + wcslen(wb));
                if (rec.vin.empty() || rec.customerName.empty() || rec.serviceDate.empty()) {
                    MessageBoxW(dlg, L"Please fill VIN, Customer and Date.", L"Validation", MB_ICONWARNING);
                } else {
                    bool ok = false;
                    if (editingId > 0) { rec.id = editingId; ok = state->db.updateServiceRecord(rec); }
                    else { ok = (bool)state->db.addServiceRecord(rec); }
                    if (!ok) MessageBoxW(dlg, W(state->db.getLastError()).c_str(), L"DB Error", MB_ICONERROR); else done = true;
                }
            } else if (id == 5191) {
                done = true;
            }
        }
        TranslateMessage(&m); DispatchMessageW(&m);
    }
    DestroyWindow(dlg);
}
// Dialog control IDs
enum : int {
    IDC_REC_VIN = 5001,
    IDC_REC_CUST = 5002,
    IDC_REC_DATE = 5003,
    IDC_REC_DESC = 5004,
    IDC_REC_MECH = 5005,
    IDC_BTN_SAVE = 5090,
    IDC_BTN_CANCEL = 5091,

    IDC_MECH_LIST = 5201,
    IDC_MECH_ADD = 5202,
    IDC_MECH_DEL = 5203,
    IDC_MECH_CLOSE = 5204,
    IDC_MECH_NAME = 5211,
    IDC_MECH_SKILL = 5212,
    IDC_MECH_ACTIVE = 5213,
    IDC_MECH_OK = 5214,
    IDC_MECH_CANCEL2 = 5215
};

static void CenterWindowOnParent(HWND hwnd, HWND parent) {
    RECT rc{}, rp{}; GetWindowRect(hwnd, &rc); GetWindowRect(parent, &rp);
    int w = rc.right - rc.left, h = rc.bottom - rc.top;
    int x = rp.left + ((rp.right - rp.left) - w) / 2;
    int y = rp.top + ((rp.bottom - rp.top) - h) / 2;
    SetWindowPos(hwnd, nullptr, x, y, 0, 0, SWP_NOSIZE | SWP_NOZORDER);
}

static void ShowAddRecordDialog(HWND parent, AppState* state) {
    HINSTANCE hi = GetModuleHandleW(nullptr);
    HWND dlg = CreateWindowExW(WS_EX_DLGMODALFRAME, L"STATIC", L"Add Service Record",
        WS_POPUP | WS_CAPTION | WS_SYSMENU, CW_USEDEFAULT, CW_USEDEFAULT, 520, 360, parent, nullptr, hi, nullptr);
    CreateWindowExW(0, L"STATIC", L"VIN:", WS_CHILD | WS_VISIBLE, 16, 40, 120, 20, dlg, nullptr, hi, nullptr);
    HWND eVin = CreateWindowExW(WS_EX_CLIENTEDGE, L"EDIT", L"", WS_CHILD | WS_VISIBLE | ES_AUTOHSCROLL, 140, 38, 350, 24, dlg, (HMENU)IDC_REC_VIN, hi, nullptr);
    CreateWindowExW(0, L"STATIC", L"Customer:", WS_CHILD | WS_VISIBLE, 16, 70, 120, 20, dlg, nullptr, hi, nullptr);
    HWND eCust = CreateWindowExW(WS_EX_CLIENTEDGE, L"EDIT", L"", WS_CHILD | WS_VISIBLE | ES_AUTOHSCROLL, 140, 68, 350, 24, dlg, (HMENU)IDC_REC_CUST, hi, nullptr);
    CreateWindowExW(0, L"STATIC", L"Date (YYYY-MM-DD):", WS_CHILD | WS_VISIBLE, 16, 100, 120, 20, dlg, nullptr, hi, nullptr);
    HWND eDate = CreateWindowExW(WS_EX_CLIENTEDGE, L"EDIT", L"", WS_CHILD | WS_VISIBLE | ES_AUTOHSCROLL, 140, 98, 350, 24, dlg, (HMENU)IDC_REC_DATE, hi, nullptr);
    CreateWindowExW(0, L"STATIC", L"Description:", WS_CHILD | WS_VISIBLE, 16, 130, 120, 20, dlg, nullptr, hi, nullptr);
    HWND eDesc = CreateWindowExW(WS_EX_CLIENTEDGE, L"EDIT", L"", WS_CHILD | WS_VISIBLE | ES_AUTOVSCROLL | ES_MULTILINE, 140, 128, 350, 80, dlg, (HMENU)IDC_REC_DESC, hi, nullptr);
    CreateWindowExW(0, L"STATIC", L"Mechanic:", WS_CHILD | WS_VISIBLE, 16, 214, 120, 20, dlg, nullptr, hi, nullptr);
    HWND eMech = CreateWindowExW(WS_EX_CLIENTEDGE, L"EDIT", L"", WS_CHILD | WS_VISIBLE | ES_AUTOHSCROLL, 140, 212, 350, 24, dlg, (HMENU)IDC_REC_MECH, hi, nullptr);
    HWND bSave = CreateWindowExW(0, L"BUTTON", L"Save", WS_CHILD | WS_VISIBLE | BS_DEFPUSHBUTTON, 300, 270, 90, 28, dlg, (HMENU)IDC_BTN_SAVE, hi, nullptr);
    HWND bCancel = CreateWindowExW(0, L"BUTTON", L"Cancel", WS_CHILD | WS_VISIBLE, 400, 270, 90, 28, dlg, (HMENU)IDC_BTN_CANCEL, hi, nullptr);
    if (state) SendMessageW(dlg, WM_SETFONT, (WPARAM)state->hFont, TRUE);
    CenterWindowOnParent(dlg, parent);
    ShowWindow(dlg, SW_SHOW);
    // Modal loop with IsDialogMessage
    MSG m; bool done = false;
    while (!done && GetMessageW(&m, nullptr, 0, 0)) {
        if (m.hwnd == dlg || IsChild(dlg, m.hwnd)) {
            if (IsDialogMessageW(dlg, &m)) continue;
        }
        if (m.message == WM_COMMAND) {
            int id = LOWORD(m.wParam);
            if (id == IDC_BTN_SAVE) {
                wchar_t wbuf[512];
                vsrm::ServiceRecord rec{};
                GetWindowTextW(eVin, wbuf, 512); rec.vin = std::string(wbuf, wbuf + wcslen(wbuf));
                GetWindowTextW(eCust, wbuf, 512); rec.customerName = std::string(wbuf, wbuf + wcslen(wbuf));
                GetWindowTextW(eDate, wbuf, 512); rec.serviceDate = std::string(wbuf, wbuf + wcslen(wbuf));
                GetWindowTextW(eDesc, wbuf, 512); rec.description = std::string(wbuf, wbuf + wcslen(wbuf));
                GetWindowTextW(eMech, wbuf, 512); rec.mechanic = std::string(wbuf, wbuf + wcslen(wbuf));
                if (rec.vin.empty() || rec.customerName.empty() || rec.serviceDate.empty()) {
                    MessageBoxW(dlg, L"Please fill VIN, Customer and Date.", L"Validation", MB_ICONWARNING);
                } else {
                    auto idOpt = state->db.addServiceRecord(rec);
                    if (!idOpt) MessageBoxW(dlg, W(state->db.getLastError()).c_str(), L"DB Error", MB_ICONERROR);
                    else done = true;
                }
            } else if (id == IDC_BTN_CANCEL) {
                done = true;
            }
        }
        TranslateMessage(&m); DispatchMessageW(&m);
    }
    DestroyWindow(dlg);
}

static void RefreshMechanicList(HWND hList, AppState* state) {
    SendMessageW(hList, LB_RESETCONTENT, 0, 0);
    auto list = state->db.listMechanics(false);
    for (const auto& m : list) {
        std::wstring line = std::to_wstring(m.id) + L" - " + W(m.name) + L" - " + W(m.skill) + (m.active ? L" (active)" : L" (inactive)");
        SendMessageW(hList, LB_ADDSTRING, 0, (LPARAM)line.c_str());
    }
}

static void ShowManageMechanicsDialog(HWND parent, AppState* state) {
    HINSTANCE hi = GetModuleHandleW(nullptr);
    HWND dlg = CreateWindowExW(WS_EX_DLGMODALFRAME, L"STATIC", L"Manage Mechanics",
        WS_POPUP | WS_CAPTION | WS_SYSMENU, CW_USEDEFAULT, CW_USEDEFAULT, 520, 380, parent, nullptr, hi, nullptr);
    HWND hList = CreateWindowExW(WS_EX_CLIENTEDGE, L"LISTBOX", L"", WS_CHILD | WS_VISIBLE | LBS_NOTIFY | WS_VSCROLL, 16, 40, 480, 240, dlg, (HMENU)IDC_MECH_LIST, hi, nullptr);
    HWND bAdd = CreateWindowExW(0, L"BUTTON", L"Add", WS_CHILD | WS_VISIBLE | BS_PUSHBUTTON, 16, 300, 80, 26, dlg, (HMENU)IDC_MECH_ADD, hi, nullptr);
    HWND bDel = CreateWindowExW(0, L"BUTTON", L"Remove", WS_CHILD | WS_VISIBLE | BS_PUSHBUTTON, 102, 300, 80, 26, dlg, (HMENU)IDC_MECH_DEL, hi, nullptr);
    HWND bClose = CreateWindowExW(0, L"BUTTON", L"Close", WS_CHILD | WS_VISIBLE | BS_PUSHBUTTON, 416, 300, 80, 26, dlg, (HMENU)IDC_MECH_CLOSE, hi, nullptr);
    if (state) SendMessageW(dlg, WM_SETFONT, (WPARAM)state->hFont, TRUE);
    CenterWindowOnParent(dlg, parent);
    ShowWindow(dlg, SW_SHOW);
    RefreshMechanicList(hList, state);
    MSG m; bool done = false;
    while (!done && GetMessageW(&m, nullptr, 0, 0)) {
        if (m.message == WM_COMMAND) {
            int id = LOWORD(m.wParam);
            if (id == IDC_MECH_CLOSE) { done = true; }
            if (id == IDC_MECH_ADD) {
                // Tiny inline add dialog
                HWND addDlg = CreateWindowExW(WS_EX_DLGMODALFRAME, L"STATIC", L"Add Mechanic",
                    WS_POPUP | WS_CAPTION | WS_SYSMENU, CW_USEDEFAULT, CW_USEDEFAULT, 420, 200, dlg, nullptr, hi, nullptr);
                CreateWindowExW(0, L"STATIC", L"Name:", WS_CHILD | WS_VISIBLE, 16, 40, 80, 20, addDlg, nullptr, hi, nullptr);
                HWND eName = CreateWindowExW(WS_EX_CLIENTEDGE, L"EDIT", L"", WS_CHILD | WS_VISIBLE | ES_AUTOHSCROLL, 100, 38, 290, 24, addDlg, (HMENU)IDC_MECH_NAME, hi, nullptr);
                CreateWindowExW(0, L"STATIC", L"Skill:", WS_CHILD | WS_VISIBLE, 16, 70, 80, 20, addDlg, nullptr, hi, nullptr);
                HWND eSkill = CreateWindowExW(WS_EX_CLIENTEDGE, L"EDIT", L"", WS_CHILD | WS_VISIBLE | ES_AUTOHSCROLL, 100, 68, 290, 24, addDlg, (HMENU)IDC_MECH_SKILL, hi, nullptr);
                HWND chkActive = CreateWindowExW(0, L"BUTTON", L"Active", WS_CHILD | WS_VISIBLE | BS_AUTOCHECKBOX, 100, 100, 100, 20, addDlg, (HMENU)IDC_MECH_ACTIVE, hi, nullptr);
                HWND bOk = CreateWindowExW(0, L"BUTTON", L"OK", WS_CHILD | WS_VISIBLE | BS_DEFPUSHBUTTON, 210, 130, 80, 26, addDlg, (HMENU)IDC_MECH_OK, hi, nullptr);
                HWND bCancel = CreateWindowExW(0, L"BUTTON", L"Cancel", WS_CHILD | WS_VISIBLE, 310, 130, 80, 26, addDlg, (HMENU)IDC_MECH_CANCEL2, hi, nullptr);
                if (state) SendMessageW(addDlg, WM_SETFONT, (WPARAM)state->hFont, TRUE);
                CenterWindowOnParent(addDlg, dlg); ShowWindow(addDlg, SW_SHOW);
                bool addDone = false;
                while (!addDone && GetMessageW(&m, nullptr, 0, 0)) {
                    if (m.message == WM_COMMAND) {
                        int id2 = LOWORD(m.wParam);
                        if (id2 == IDC_MECH_OK) {
                            wchar_t wbuf[512]; vsrm::Mechanic mm{};
                            GetWindowTextW(eName, wbuf, 512); mm.name = std::string(wbuf, wbuf + wcslen(wbuf));
                            GetWindowTextW(eSkill, wbuf, 512); mm.skill = std::string(wbuf, wbuf + wcslen(wbuf));
                            mm.active = (SendMessageW(chkActive, BM_GETCHECK, 0, 0) == BST_CHECKED);
                            if (mm.name.empty() || mm.skill.empty()) {
                                MessageBoxW(addDlg, L"Please enter name and skill.", L"Validation", MB_ICONWARNING);
                            } else {
                                auto idNew = state->db.addMechanic(mm);
                                if (!idNew) MessageBoxW(addDlg, W(state->db.getLastError()).c_str(), L"DB Error", MB_ICONERROR);
                                else addDone = true;
                            }
                        } else if (id2 == IDC_MECH_CANCEL2) {
                            addDone = true;
                        }
                    }
                    TranslateMessage(&m); DispatchMessageW(&m);
                }
                DestroyWindow(addDlg);
                RefreshMechanicList(hList, state);
            }
            if (id == IDC_MECH_DEL) {
                int sel = (int)SendMessageW(hList, LB_GETCURSEL, 0, 0);
                if (sel != LB_ERR) {
                    wchar_t text[512]; SendMessageW(hList, LB_GETTEXT, sel, (LPARAM)text);
                    int mechId = 0; swscanf_s(text, L"%d", &mechId);
                    if (mechId > 0) {
                        if (!state->db.deleteMechanic(mechId)) MessageBoxW(dlg, W(state->db.getLastError()).c_str(), L"DB Error", MB_ICONERROR);
                        else RefreshMechanicList(hList, state);
                    }
                }
            }
        }
        TranslateMessage(&m); DispatchMessageW(&m);
    }
    DestroyWindow(dlg);
}

// (Removed duplicate AppState and W/ShowError definitions)

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
	static HWND hEdit{};
	static HWND hToolbar{};
	static HWND hStatus{};
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
        TBBUTTON tbb[4] = {};
        tbb[0] = { MAKELONG(STD_FILENEW, 0), 2001, TBSTATE_ENABLED, BTNS_BUTTON, {0}, 0, (INT_PTR)L"Add" };
        tbb[1] = { MAKELONG(STD_FILESAVE, 0), 2401, TBSTATE_ENABLED, BTNS_BUTTON, {0}, 0, (INT_PTR)L"Export" };
        tbb[2] = { MAKELONG(STD_REDOW, 0), 2501, TBSTATE_ENABLED, BTNS_BUTTON, {0}, 0, (INT_PTR)L"Refresh" };
        tbb[3] = { MAKELONG(STD_PROPERTIES, 0), 2601, TBSTATE_ENABLED, BTNS_BUTTON, {0}, 0, (INT_PTR)L"Settings" };
		SendMessageW(hToolbar, TB_SETIMAGELIST, 0, (LPARAM)ImageList_Create(16, 16, ILC_COLOR32 | ILC_MASK, 4, 4));
		SendMessageW(hToolbar, TB_LOADIMAGES, (WPARAM)IDB_STD_SMALL_COLOR, (LPARAM)HINST_COMMCTRL);
        SendMessageW(hToolbar, TB_ADDBUTTONS, (WPARAM)4, (LPARAM)&tbb);

		hStatus = CreateWindowExW(0, STATUSCLASSNAMEW, nullptr, WS_CHILD | WS_VISIBLE, 0, 0, 0, 0, hwnd, (HMENU)3002, GetModuleHandleW(nullptr), nullptr);

        LOGFONTW lf{}; SystemParametersInfoW(SPI_GETICONTITLELOGFONT, sizeof(lf), &lf, 0);
        lstrcpyW(lf.lfFaceName, L"Segoe UI"); lf.lfHeight = -16;
        if (state) {
            state->hFont = CreateFontIndirectW(&lf);
            state->hBg = CreateSolidBrush(RGB(248, 250, 252));
            state->hHeaderBg = CreateSolidBrush(RGB(226, 232, 240));
            state->hButtonBg = CreateSolidBrush(RGB(217, 239, 255));
            // Themed button brushes
            state->hBtnPrimary = CreateSolidBrush(RGB(59, 130, 246));  // blue
            state->hBtnDanger  = CreateSolidBrush(RGB(239, 68, 68));   // red
            state->hBtnSuccess = CreateSolidBrush(RGB(34, 197, 94));   // green
        }

        hEdit = CreateWindowExW(WS_EX_CLIENTEDGE, L"EDIT", L"",
			WS_CHILD | WS_VISIBLE | ES_MULTILINE | ES_AUTOVSCROLL | ES_READONLY | WS_VSCROLL,
			0, 0, 0, 0, hwnd, (HMENU)1001, GetModuleHandleW(nullptr), nullptr);
        // Tooltips for toolbar and nav
        HWND hTips = CreateWindowExW(0, TOOLTIPS_CLASS, nullptr, WS_POPUP | TTS_ALWAYSTIP, CW_USEDEFAULT, CW_USEDEFAULT, CW_USEDEFAULT, CW_USEDEFAULT, hwnd, nullptr, GetModuleHandleW(nullptr), nullptr);
        SendMessageW(hTips, TTM_SETMAXTIPWIDTH, 0, 400);
        auto addTip = [&](HWND target, const wchar_t* text){
            TOOLINFOW ti{}; ti.cbSize = sizeof(ti); ti.uFlags = TTF_SUBCLASS; ti.hwnd = hwnd; ti.hinst = GetModuleHandleW(nullptr); ti.uId = (UINT_PTR)target; ti.lpszText = const_cast<LPWSTR>(text);
            GetClientRect(target, &ti.rect);
            SendMessageW(hTips, TTM_ADDTOOL, 0, (LPARAM)&ti);
        };
        addTip(hToolbar, L"Global actions");
        addTip(state->hNavDashboard, L"Dashboard");
        addTip(state->hNavVehicles, L"Vehicles");
        addTip(state->hNavMechanics, L"Mechanics");
        addTip(state->hNavReports, L"Reports");

        // Tab order: left nav first, then search fields, then grid
        SetWindowPos(state->hNavDashboard, HWND_TOP, 0,0,0,0, SWP_NOMOVE | SWP_NOSIZE);
        SetWindowPos(state->hNavVehicles, state->hNavDashboard, 0,0,0,0, SWP_NOMOVE | SWP_NOSIZE);
        SetWindowPos(state->hNavMechanics, state->hNavVehicles, 0,0,0,0, SWP_NOMOVE | SWP_NOSIZE);
        SetWindowPos(state->hNavReports, state->hNavMechanics, 0,0,0,0, SWP_NOMOVE | SWP_NOSIZE);
        SetWindowPos(state->hSearchVin, state->hNavReports, 0,0,0,0, SWP_NOMOVE | SWP_NOSIZE);
        SetWindowPos(state->hSearchDateFrom, state->hSearchVin, 0,0,0,0, SWP_NOMOVE | SWP_NOSIZE);
        SetWindowPos(state->hSearchDateTo, state->hSearchDateFrom, 0,0,0,0, SWP_NOMOVE | SWP_NOSIZE);
        SetWindowPos(state->hSearchMechanic, state->hSearchDateTo, 0,0,0,0, SWP_NOMOVE | SWP_NOSIZE);
        SetWindowPos(state->hChkDue, state->hSearchMechanic, 0,0,0,0, SWP_NOMOVE | SWP_NOSIZE);
        SetWindowPos(state->hList, state->hChkDue, 0,0,0,0, SWP_NOMOVE | SWP_NOSIZE);
        if (state) {
            SendMessageW(hEdit, WM_SETFONT, (WPARAM)state->hFont, TRUE);
            SendMessageW(hStatus, WM_SETFONT, (WPARAM)state->hFont, TRUE);
        }
        // Main area replaces welcome buttons: left nav, search bar, and list view
        ShowWindow(hEdit, SW_HIDE);
        state->showWelcome = false;

        // Icon font for navigation (Segoe MDL2 Assets)
        LOGFONTW lfIcon{}; SystemParametersInfoW(SPI_GETICONTITLELOGFONT, sizeof(lfIcon), &lfIcon, 0);
        lstrcpyW(lfIcon.lfFaceName, L"Segoe MDL2 Assets"); lfIcon.lfHeight = -20; state->hIconFont = CreateFontIndirectW(&lfIcon);

        // Left navigation container
        state->hLeftNav = CreateWindowExW(0, L"STATIC", L"", WS_CHILD | WS_VISIBLE, 0, 0, 0, 0, hwnd, (HMENU)4100, GetModuleHandleW(nullptr), nullptr);
        // Icon + label buttons (using normal buttons with two-character text: icon glyph + space + label)
        state->hNavDashboard = CreateWindowExW(0, L"BUTTON", L"\uE80F  Dashboard", WS_CHILD | WS_VISIBLE | BS_PUSHBUTTON, 0,0,0,0, hwnd, (HMENU)4101, GetModuleHandleW(nullptr), nullptr);
        state->hNavVehicles  = CreateWindowExW(0, L"BUTTON", L"\uECAD  Vehicles",  WS_CHILD | WS_VISIBLE | BS_PUSHBUTTON, 0,0,0,0, hwnd, (HMENU)4102, GetModuleHandleW(nullptr), nullptr);
        state->hNavMechanics = CreateWindowExW(0, L"BUTTON", L"\uEADF  Mechanics", WS_CHILD | WS_VISIBLE | BS_PUSHBUTTON, 0,0,0,0, hwnd, (HMENU)4103, GetModuleHandleW(nullptr), nullptr);
        state->hNavReports   = CreateWindowExW(0, L"BUTTON", L"\uE9D2  Reports",   WS_CHILD | WS_VISIBLE | BS_PUSHBUTTON, 0,0,0,0, hwnd, (HMENU)4104, GetModuleHandleW(nullptr), nullptr);
        if (state) {
            SendMessageW(state->hNavDashboard, WM_SETFONT, (WPARAM)state->hFont, TRUE);
            SendMessageW(state->hNavVehicles, WM_SETFONT, (WPARAM)state->hFont, TRUE);
            SendMessageW(state->hNavMechanics, WM_SETFONT, (WPARAM)state->hFont, TRUE);
            SendMessageW(state->hNavReports, WM_SETFONT, (WPARAM)state->hFont, TRUE);
        }

        // Search/filter bar
        state->hSearchVin = CreateWindowExW(WS_EX_CLIENTEDGE, L"EDIT", L"", WS_CHILD | WS_VISIBLE | ES_AUTOHSCROLL, 0,0,0,0, hwnd, (HMENU)4201, GetModuleHandleW(nullptr), nullptr);
        SendMessageW(state->hSearchVin, EM_SETCUEBANNER, TRUE, (LPARAM)L"Search VIN");
        state->hSearchDateFrom = CreateWindowExW(WS_EX_CLIENTEDGE, L"EDIT", L"", WS_CHILD | WS_VISIBLE | ES_AUTOHSCROLL, 0,0,0,0, hwnd, (HMENU)4202, GetModuleHandleW(nullptr), nullptr);
        SendMessageW(state->hSearchDateFrom, EM_SETCUEBANNER, TRUE, (LPARAM)L"From (YYYY-MM-DD)");
        state->hSearchDateTo = CreateWindowExW(WS_EX_CLIENTEDGE, L"EDIT", L"", WS_CHILD | WS_VISIBLE | ES_AUTOHSCROLL, 0,0,0,0, hwnd, (HMENU)4203, GetModuleHandleW(nullptr), nullptr);
        SendMessageW(state->hSearchDateTo, EM_SETCUEBANNER, TRUE, (LPARAM)L"To (YYYY-MM-DD)");
        state->hSearchMechanic = CreateWindowExW(WS_EX_CLIENTEDGE, L"EDIT", L"", WS_CHILD | WS_VISIBLE | ES_AUTOHSCROLL, 0,0,0,0, hwnd, (HMENU)4204, GetModuleHandleW(nullptr), nullptr);
        SendMessageW(state->hSearchMechanic, EM_SETCUEBANNER, TRUE, (LPARAM)L"Mechanic");
        state->hChkDue = CreateWindowExW(0, L"BUTTON", L"Service due", WS_CHILD | WS_VISIBLE | BS_AUTOCHECKBOX, 0,0,0,0, hwnd, (HMENU)4205, GetModuleHandleW(nullptr), nullptr);
        if (state) {
            SendMessageW(state->hSearchVin, WM_SETFONT, (WPARAM)state->hFont, TRUE);
            SendMessageW(state->hSearchDateFrom, WM_SETFONT, (WPARAM)state->hFont, TRUE);
            SendMessageW(state->hSearchDateTo, WM_SETFONT, (WPARAM)state->hFont, TRUE);
            SendMessageW(state->hSearchMechanic, WM_SETFONT, (WPARAM)state->hFont, TRUE);
            SendMessageW(state->hChkDue, WM_SETFONT, (WPARAM)state->hFont, TRUE);
        }

        // Data grid (ListView)
        state->hList = CreateWindowExW(WS_EX_CLIENTEDGE, WC_LISTVIEWW, L"", WS_CHILD | WS_VISIBLE | LVS_REPORT | LVS_SHOWSELALWAYS | WS_TABSTOP,
            0,0,0,0, hwnd, (HMENU)4301, GetModuleHandleW(nullptr), nullptr);
        ListView_SetExtendedListViewStyle(state->hList, LVS_EX_FULLROWSELECT | LVS_EX_DOUBLEBUFFER | LVS_EX_GRIDLINES);
        // Columns
        LVCOLUMNW col{}; col.mask = LVCF_TEXT | LVCF_WIDTH | LVCF_FMT; col.fmt = LVCFMT_LEFT;
        struct { const wchar_t* name; int w; } cols[] = { {L"VIN", 160}, {L"Make", 100}, {L"Model", 100}, {L"Last Service", 120}, {L"Mechanic", 120}, {L"Next Service", 120}, {L"Status", 100} };
        for (int i = 0; i < 7; ++i) { col.pszText = const_cast<LPWSTR>(cols[i].name); col.cx = cols[i].w; ListView_InsertColumn(state->hList, i, &col); }
        if (state) SendMessageW(state->hList, WM_SETFONT, (WPARAM)state->hFont, TRUE);

        // Reports panel (metrics + basic bar chart + exporter)
        state->hReportsPanel = CreateWindowExW(0, L"STATIC", L"", WS_CHILD, 0,0,0,0, hwnd, (HMENU)4401, GetModuleHandleW(nullptr), nullptr);
        HWND rTitle = CreateWindowExW(0, L"STATIC", L"Reports", WS_CHILD | WS_VISIBLE, 0,0,0,0, state->hReportsPanel, (HMENU)4410, GetModuleHandleW(nullptr), nullptr);
        HWND rBtnExportAll = CreateWindowExW(0, L"BUTTON", L"Export All CSV", WS_CHILD | WS_VISIBLE, 0,0,0,0, state->hReportsPanel, (HMENU)4411, GetModuleHandleW(nullptr), nullptr);
        HWND rMetrics = CreateWindowExW(WS_EX_CLIENTEDGE, L"EDIT", L"", WS_CHILD | WS_VISIBLE | ES_MULTILINE | ES_READONLY | ES_AUTOVSCROLL | WS_VSCROLL, 0,0,0,0, state->hReportsPanel, (HMENU)4412, GetModuleHandleW(nullptr), nullptr);
        if (state) { SendMessageW(rTitle, WM_SETFONT, (WPARAM)state->hFont, TRUE); SendMessageW(rBtnExportAll, WM_SETFONT, (WPARAM)state->hFont, TRUE); SendMessageW(rMetrics, WM_SETFONT, (WPARAM)state->hFont, TRUE); }
        ShowWindow(state->hReportsPanel, SW_HIDE);

        // Persisted UI settings
        state->settingsPath = exeDir + L"/ui.settings";
        LoadUiSettings(state);
		return 0;
	}
	case WM_SIZE: {
		SendMessageW(hToolbar, TB_AUTOSIZE, 0, 0);
		SendMessageW(hStatus, WM_SIZE, 0, 0);
		RECT rc; GetClientRect(hwnd, &rc);
		RECT rcTb; GetWindowRect(hToolbar, &rcTb); int tbH = rcTb.bottom - rcTb.top;
		RECT rcSb; GetWindowRect(hStatus, &rcSb); int sbH = rcSb.bottom - rcSb.top;
        int startY = tbH + kHeaderHeight;
        int leftW = 200; // left navigation width
        int searchH = 40; // search bar height
        // Left navigation stack
        MoveWindow(state->hLeftNav, 0, startY, leftW, rc.bottom - rc.top - startY - sbH, TRUE);
        int navBtnH = 44; int navGap = 8; int yNav = startY + 8;
        MoveWindow(state->hNavDashboard, 8, yNav, leftW - 16, navBtnH, TRUE); yNav += navBtnH + navGap;
        MoveWindow(state->hNavVehicles, 8, yNav, leftW - 16, navBtnH, TRUE); yNav += navBtnH + navGap;
        MoveWindow(state->hNavMechanics, 8, yNav, leftW - 16, navBtnH, TRUE); yNav += navBtnH + navGap;
        MoveWindow(state->hNavReports, 8, yNav, leftW - 16, navBtnH, TRUE);
        // Search bar
        int contentX = leftW;
        int contentW = (rc.right - rc.left) - leftW;
        MoveWindow(state->hSearchVin, contentX + 8, startY + 4, 200, searchH - 8, TRUE);
        MoveWindow(state->hSearchDateFrom, contentX + 216, startY + 4, 140, searchH - 8, TRUE);
        MoveWindow(state->hSearchDateTo, contentX + 360, startY + 4, 140, searchH - 8, TRUE);
        MoveWindow(state->hSearchMechanic, contentX + 504, startY + 4, 160, searchH - 8, TRUE);
        MoveWindow(state->hChkDue, contentX + 672, startY + 8, 120, searchH - 16, TRUE);
        // Data grid area / reports panel
        MoveWindow(state->hList, contentX, startY + searchH, contentW, rc.bottom - rc.top - startY - searchH - sbH, TRUE);
        // Layout inside reports panel
        MoveWindow(state->hReportsPanel, contentX, startY + 4, contentW, rc.bottom - rc.top - startY - sbH - 8, TRUE);
        HWND rTitle = GetDlgItem(state->hReportsPanel, 4410);
        HWND rBtnExport = GetDlgItem(state->hReportsPanel, 4411);
        HWND rMetrics = GetDlgItem(state->hReportsPanel, 4412);
        if (rTitle && rBtnExport && rMetrics) {
            MoveWindow(rTitle, 8, 8, 200, 24, TRUE);
            MoveWindow(rBtnExport, contentW - 160, 8, 150, 28, TRUE);
            MoveWindow(rMetrics, 8, 44, contentW - 16, rc.bottom - rc.top - startY - sbH - 8 - 44 - 12, TRUE);
        }
		return 0;
	}
	case WM_NOTIFY: {
        AppState* state = reinterpret_cast<AppState*>(GetWindowLongPtrW(hwnd, GWLP_USERDATA));
        if (state) {
            LPNMHDR nm = (LPNMHDR)lParam;
            if (nm->hwndFrom == state->hList && nm->code == NM_DBLCLK) {
                int sel = ListView_GetNextItem(state->hList, -1, LVNI_SELECTED);
                if (sel >= 0) {
                    wchar_t wvin[256]; ListView_GetItemText(state->hList, sel, 0, wvin, 256);
                    OpenRecordEditor(hwnd, state, std::string(wvin, wvin + wcslen(wvin)));
                    SendMessageW(hwnd, WM_COMMAND, 2501, 0);
		}
		return 0;
            }
        }
        break;
	}
    case WM_PAINT: {
		PAINTSTRUCT ps; HDC hdc = BeginPaint(hwnd, &ps);
		RECT rc; GetClientRect(hwnd, &rc);
        // Draw header banner below the toolbar with branding and avatar
        RECT rcTb; GetWindowRect(hToolbar, &rcTb); int tbH = rcTb.bottom - rcTb.top;
        int headerH = 56;
        HBRUSH headerBrush = state && state->hHeaderBg ? state->hHeaderBg : CreateSolidBrush(RGB(233, 238, 243));
        RECT hdr{ rc.left, tbH, rc.right, tbH + headerH };
        FillRect(hdc, &hdr, headerBrush);
        if (!(state && state->hHeaderBg)) DeleteObject(headerBrush);
		if (state && state->logo && state->logo->GetLastStatus() == Gdiplus::Ok) {
			Gdiplus::Graphics g(hdc);
			g.SetInterpolationMode(Gdiplus::InterpolationModeHighQualityBicubic);
            int lh = headerH - 12; int lw = (int)((double)state->logo->GetWidth() * (double)lh / (double)state->logo->GetHeight());
            g.DrawImage(state->logo, 8, tbH + 6, lw, lh);
            // App name
            Gdiplus::Font title(L"Segoe UI", 18, Gdiplus::FontStyleBold, Gdiplus::UnitPixel);
            Gdiplus::SolidBrush tcol(Gdiplus::Color(255, 60, 60, 60));
            g.DrawString(L"Vehicle Service Records Manager", -1, &title, Gdiplus::PointF((Gdiplus::REAL)(lw + 16), (Gdiplus::REAL)(tbH + 14)), &tcol);
            // User avatar placeholder (circle) on top-right
            int avatarD = 32; int ax = rc.right - 16 - avatarD; int ay = tbH + (headerH - avatarD) / 2;
            Gdiplus::SolidBrush avatar(Gdiplus::Color(255, 220, 226, 236));
            g.FillEllipse(&avatar, ax, ay, avatarD, avatarD);
		}
		EndPaint(hwnd, &ps);
		return 0;
	}
    case WM_ERASEBKGND: {
        if (state && state->hBg) {
            RECT rc; GetClientRect(hwnd, &rc);
            FillRect((HDC)wParam, &rc, state->hBg);
            return 1;
        }
        break;
    }
	case WM_CTLCOLORSTATIC: {
		HDC hdc = (HDC)wParam;
		SetBkColor(hdc, RGB(248, 250, 252));
		return (LRESULT)state->hBg;
	}
    case WM_CTLCOLORBTN: {
        HDC hdc = (HDC)wParam;
        // Decide brush based on control ID: make Refresh (4005) green, others themed
        HWND hCtl = (HWND)lParam; int ctlId = (int)GetDlgCtrlID(hCtl);
        if (ctlId == 4005) { // Refresh
            SetBkColor(hdc, RGB(34, 197, 94));
            return (LRESULT)(state && state->hBtnSuccess ? state->hBtnSuccess : GetSysColorBrush(COLOR_BTNFACE));
        }
        if (ctlId == 4004) { // Export as example danger? keep primary
            SetBkColor(hdc, RGB(59, 130, 246));
            return (LRESULT)(state && state->hBtnPrimary ? state->hBtnPrimary : GetSysColorBrush(COLOR_BTNFACE));
        }
        // Default welcome color
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
                ShowWindow(hEdit, SW_SHOW);
			}
			if (LOWORD(wParam) == 4001) SendMessageW(hwnd, WM_COMMAND, 2001, 0);
			if (LOWORD(wParam) == 4002) SendMessageW(hwnd, WM_COMMAND, 2002, 0);
			if (LOWORD(wParam) == 4003) SendMessageW(hwnd, WM_COMMAND, 2102, 0);
			if (LOWORD(wParam) == 4004) SendMessageW(hwnd, WM_COMMAND, 2401, 0);
            if (LOWORD(wParam) == 4005) SendMessageW(hwnd, WM_COMMAND, 2501, 0);
			return 0;
		}
        if (LOWORD(wParam) == 2001) { // Add record
            OpenRecordEditor(hwnd, state, "");
            SendMessageW(hwnd, WM_COMMAND, 2501, 0); // Refresh
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
        if (LOWORD(wParam) == 2102 || LOWORD(wParam) == 4103) { // Manage mechanics
            ShowManageMechanicsDialog(hwnd, state);
            SendMessageW(hwnd, WM_COMMAND, 2501, 0);
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
        if (LOWORD(wParam) == 2501) { // Refresh current view
            // Populate vehicle grid from filters
            wchar_t wbuf[256];
            GetWindowTextW(state->hSearchVin, wbuf, 256); std::string vinLike(wbuf, wbuf + wcslen(wbuf));
            GetWindowTextW(state->hSearchDateFrom, wbuf, 256); std::optional<std::string> from = wcslen(wbuf) ? std::optional<std::string>(std::string(wbuf, wbuf + wcslen(wbuf))) : std::nullopt;
            GetWindowTextW(state->hSearchDateTo, wbuf, 256); std::optional<std::string> to = wcslen(wbuf) ? std::optional<std::string>(std::string(wbuf, wbuf + wcslen(wbuf))) : std::nullopt;
            GetWindowTextW(state->hSearchMechanic, wbuf, 256); std::optional<std::string> mech = wcslen(wbuf) ? std::optional<std::string>(std::string(wbuf, wbuf + wcslen(wbuf))) : std::nullopt;
            bool dueOnly = (SendMessageW(state->hChkDue, BM_GETCHECK, 0, 0) == BST_CHECKED);
            auto list = state->db.listVehicleSummaries(vinLike, from, to, mech, dueOnly);
            ListView_DeleteAllItems(state->hList);
            LVITEMW it{}; it.mask = LVIF_TEXT;
            for (size_t i = 0; i < list.size(); ++i) {
                std::wstring vin = W(list[i].vin);
                it.iItem = (int)i; it.iSubItem = 0; it.pszText = const_cast<LPWSTR>(vin.c_str()); ListView_InsertItem(state->hList, &it);
                ListView_SetItemText(state->hList, (int)i, 1, const_cast<LPWSTR>(W(list[i].make).c_str()));
                ListView_SetItemText(state->hList, (int)i, 2, const_cast<LPWSTR>(W(list[i].model).c_str()));
                ListView_SetItemText(state->hList, (int)i, 3, const_cast<LPWSTR>(W(list[i].lastServiceDate).c_str()));
                ListView_SetItemText(state->hList, (int)i, 4, const_cast<LPWSTR>(W(list[i].mechanic).c_str()));
                ListView_SetItemText(state->hList, (int)i, 5, const_cast<LPWSTR>(W(list[i].nextService.value_or("")).c_str()));
                ListView_SetItemText(state->hList, (int)i, 6, const_cast<LPWSTR>(W(list[i].status).c_str()));
            }
            SendMessageW(hStatus, SB_SETTEXT, 0, (LPARAM)L"Grid refreshed");
            return 0;
        }
        if (LOWORD(wParam) == 4411) { // Export all CSV
            std::wstring outPath;
            PWSTR pDesktop = nullptr;
            if (SUCCEEDED(SHGetKnownFolderPath(FOLDERID_Desktop, 0, nullptr, &pDesktop))) {
                std::wstring desktop(pDesktop); CoTaskMemFree(pDesktop);
                outPath = desktop + L"/vsrm_all_records.csv";
            } else {
                outPath = GetExecutableDir() + L"/vsrm_all_records.csv";
            }
            bool ok = state->db.exportAllServiceRecordsCsv(std::string(outPath.begin(), outPath.end()));
            if (!ok) ShowError(hwnd, L"Export Failed", state->db.getLastError()); else SendMessageW(hStatus, SB_SETTEXT, 0, (LPARAM)L"All records CSV exported");
            return 0;
        }
        // Left nav switching
        if (LOWORD(wParam) == 4101) { SwitchView(hwnd, state, AppState::View::Vehicles); return 0; }
        if (LOWORD(wParam) == 4102) { SwitchView(hwnd, state, AppState::View::Vehicles); return 0; }
        if (LOWORD(wParam) == 4104) { SwitchView(hwnd, state, AppState::View::Reports); return 0; }
		return 0;
	}
	case WM_DESTROY:
        SaveUiSettings(state);
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

    // Temporary font for login dialog before window exists
    LOGFONTW lf{}; SystemParametersInfoW(SPI_GETICONTITLELOGFONT, sizeof(lf), &lf, 0);
    lstrcpyW(lf.lfFaceName, L"Segoe UI"); lf.lfHeight = -16;
    HFONT tempFont = CreateFontIndirectW(&lf);
    if (!ShowLoginDialog(&state, hInstance, tempFont)) {
        DeleteObject(tempFont);
        return 0;
    }
    DeleteObject(tempFont);

	// Open DB
	if (!state.db.openOrCreate(std::string(state.dbPath.begin(), state.dbPath.end()))) {
		const std::string err = state.db.getLastError();
		MessageBoxW(nullptr, W(err).c_str(), L"Failed to open database", MB_ICONERROR);
		return -1;
	}
    // Initialize schema and ensure default admin user exists
	std::wstring schemaPath = exeDir + L"/schema.sql";
    state.db.initializeSchema(std::string(schemaPath.begin(), schemaPath.end()));
    state.db.ensureDefaultAdmin();

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

    // Create main window maximized
    HWND hwnd = CreateWindowExW(0, kClassName, L"Toyota Zambia - Vehicle Service Records Manager", WS_OVERLAPPEDWINDOW,
        CW_USEDEFAULT, CW_USEDEFAULT, 900, 600, nullptr, nullptr, hInstance, &state);

	if (!hwnd) {
		MessageBoxW(nullptr, L"Failed to create window", L"Error", MB_ICONERROR);
		return -1;
	}

	SetWindowLongPtrW(hwnd, GWLP_USERDATA, (LONG_PTR)&state);
	SetMenu(hwnd, CreateMainMenu());
    ShowWindow(hwnd, SW_SHOWMAXIMIZED);
	UpdateWindow(hwnd);

	MSG msg;
	while (GetMessageW(&msg, nullptr, 0, 0)) {
		TranslateMessage(&msg);
		DispatchMessageW(&msg);
	}
	return (int)msg.wParam;
}


