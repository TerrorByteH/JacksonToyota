#include <windows.h>
#include <string>
#include <vector>
#include <filesystem>
#include <commctrl.h>
#include <gdiplus.h>
#include <shlobj.h>

#include "../app/Database.h"

namespace fs = std::filesystem;

static std::wstring W(const std::string& s) {
	return std::wstring(s.begin(), s.end());
}

static void ShowError(HWND hwnd, const wchar_t* title, const std::string& msg) {
	std::wstring wmsg(msg.begin(), msg.end());
	MessageBoxW(hwnd, wmsg.c_str(), title, MB_ICONERROR | MB_OK);
}

// Forward declaration so we can reference AppState* before full definition
struct AppState;

struct LoginCtx {
    AppState* state;
    HFONT font;
    HWND eUser;
    HWND ePass;
    bool authed;
    // GDI+ visuals
    ULONG_PTR gdipToken{};
    Gdiplus::Image* bg{};
    Gdiplus::Image* logo{};
    int headerH{80};
};

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

        int y = ctx->headerH + 20;
        CreateWindowExW(0, L"STATIC", L"Username:", WS_CHILD | WS_VISIBLE, 20, y, 80, 20, hwnd, nullptr, cs->hInstance, nullptr);
        ctx->eUser = CreateWindowExW(WS_EX_CLIENTEDGE, L"EDIT", L"", WS_CHILD | WS_VISIBLE | ES_AUTOHSCROLL, 110, y-2, 220, 24, hwnd, (HMENU)6001, cs->hInstance, nullptr);
        CreateWindowExW(0, L"STATIC", L"Password:", WS_CHILD | WS_VISIBLE, 20, y+32, 80, 20, hwnd, nullptr, cs->hInstance, nullptr);
        ctx->ePass = CreateWindowExW(WS_EX_CLIENTEDGE, L"EDIT", L"", WS_CHILD | WS_VISIBLE | ES_PASSWORD | ES_AUTOHSCROLL, 110, y+30, 220, 24, hwnd, (HMENU)6002, cs->hInstance, nullptr);
        CreateWindowExW(0, L"BUTTON", L"Login", WS_CHILD | WS_VISIBLE | BS_DEFPUSHBUTTON, 170, y+70, 80, 28, hwnd, (HMENU)6003, cs->hInstance, nullptr);
        CreateWindowExW(0, L"BUTTON", L"Cancel", WS_CHILD | WS_VISIBLE, 260, y+70, 80, 28, hwnd, (HMENU)6004, cs->hInstance, nullptr);
        if (ctx->font) {
            SendMessageW(hwnd, WM_SETFONT, (WPARAM)ctx->font, TRUE);
            SendMessageW(ctx->eUser, WM_SETFONT, (WPARAM)ctx->font, TRUE);
            SendMessageW(ctx->ePass, WM_SETFONT, (WPARAM)ctx->font, TRUE);
        }
        return 0;
    }
    case WM_PAINT: {
        PAINTSTRUCT ps; HDC hdc = BeginPaint(hwnd, &ps);
        RECT rc; GetClientRect(hwnd, &rc);
        Gdiplus::Graphics g(hdc);
        g.SetSmoothingMode(Gdiplus::SmoothingModeHighQuality);
        // Full-window gradient background
        Gdiplus::LinearGradientBrush bkg(
            Gdiplus::Point(0, 0), Gdiplus::Point(rc.right, 0),
            Gdiplus::Color(255, 80, 120, 255), Gdiplus::Color(255, 150, 220, 255));
        g.FillRectangle(&bkg, 0, 0, rc.right, rc.bottom);
        // Overlay background image with low opacity
        if (ctx && ctx->bg && ctx->bg->GetLastStatus() == Gdiplus::Ok) {
            Gdiplus::ImageAttributes attrs; Gdiplus::ColorMatrix cm = {
                1,0,0,0,0,
                0,1,0,0,0,
                0,0,1,0,0,
                0,0,0,0.20f,0,
                0,0,0,0,1
            };
            attrs.SetColorMatrix(&cm);
            g.DrawImage(ctx->bg, Gdiplus::Rect(0,0, rc.right, rc.bottom), 0,0,(INT)ctx->bg->GetWidth(), (INT)ctx->bg->GetHeight(), Gdiplus::UnitPixel, &attrs);
        }
        // Header gradient bar
        Gdiplus::LinearGradientBrush hdr(
            Gdiplus::Point(0, 0), Gdiplus::Point(0, ctx->headerH),
            Gdiplus::Color(255, 20, 40, 120), Gdiplus::Color(255, 20, 20, 80));
        g.FillRectangle(&hdr, 0, 0, rc.right, ctx->headerH);
        // Logo
        if (ctx && ctx->logo && ctx->logo->GetLastStatus() == Gdiplus::Ok) {
            int lh = ctx->headerH - 20; int lw = (int)((double)ctx->logo->GetWidth() * (double)lh / (double)ctx->logo->GetHeight());
            g.DrawImage(ctx->logo, 10, 10, lw, lh);
        }
        // Title text
        Gdiplus::Font font(L"Segoe UI", 18, Gdiplus::FontStyleBold, Gdiplus::UnitPixel);
        Gdiplus::SolidBrush white(Gdiplus::Color(255,255,255));
        Gdiplus::StringFormat fmt; fmt.SetAlignment(Gdiplus::StringAlignmentCenter); fmt.SetLineAlignment(Gdiplus::StringAlignmentCenter);
        g.DrawString(L"Login", -1, &font, Gdiplus::RectF(0,0,(Gdiplus::REAL)rc.right,(Gdiplus::REAL)ctx->headerH), &fmt, &white);
        EndPaint(hwnd, &ps);
        return 0;
    }
    case WM_COMMAND: {
        int id = LOWORD(wParam);
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
    HWND dlg = CreateWindowExW(WS_EX_DLGMODALFRAME, L"VSRM_Login", L"Login",
        WS_POPUP | WS_CAPTION | WS_SYSMENU, CW_USEDEFAULT, CW_USEDEFAULT, 360, 200, nullptr, nullptr, hInst, &ctx);
    RECT rc{}; SystemParametersInfoW(SPI_GETWORKAREA, 0, &rc, 0);
    SetWindowPos(dlg, nullptr, rc.left + (rc.right - rc.left - 360) / 2, rc.top + (rc.bottom - rc.top - 200) / 2, 0, 0, SWP_NOSIZE | SWP_NOZORDER);
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
    Gdiplus::Image* logo{};
    // Welcome panel
    bool showWelcome{true};
    HWND hBtnAdd{};
    HWND hBtnQuery{};
    HWND hBtnMechanics{};
    HWND hBtnExport{};
    HWND hBtnRefresh{};
};

static const wchar_t* kClassName = L"VSRMMainWindow";
static const int kHeaderHeight = 56;
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
    // Modal loop
    MSG m; bool done = false;
    while (!done && GetMessageW(&m, nullptr, 0, 0)) {
        if (m.message == WM_COMMAND && (HWND)m.lParam) {
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
                    MessageBoxW(dlg, L"Please fill VIN, Customer, and Date.", L"Validation", MB_ICONWARNING);
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
        // Hide log area until user interacts so welcome buttons are primary
        ShowWindow(hEdit, state && state->showWelcome ? SW_HIDE : SW_SHOW);

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
        // Draw header banner below the toolbar
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
		if (LOWORD(wParam) == 2001) { // Add sample record
            ShowAddRecordDialog(hwnd, state);
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
        if (LOWORD(wParam) == 2102) { // Manage mechanics
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


