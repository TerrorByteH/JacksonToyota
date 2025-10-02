#pragma once
#include <windows.h>
#include <gdiplus.h>

// Modern UI Control Rendering
namespace ModernControls {

struct RoundedRect {
    static void Draw(HDC hdc, const RECT& rc, int radius, COLORREF fillColor, COLORREF borderColor) {
        Gdiplus::Graphics g(hdc);
        g.SetSmoothingMode(Gdiplus::SmoothingModeAntiAlias);
        
        Gdiplus::GraphicsPath path;
        int d = radius * 2;
        path.AddArc(rc.left, rc.top, d, d, 180.0f, 90.0f);
        path.AddArc(rc.right - d, rc.top, d, d, 270.0f, 90.0f);
        path.AddArc(rc.right - d, rc.bottom - d, d, d, 0.0f, 90.0f);
        path.AddArc(rc.left, rc.bottom - d, d, d, 90.0f, 90.0f);
        path.CloseFigure();

        Gdiplus::SolidBrush fill(Gdiplus::Color(
            GetRValue(fillColor),
            GetGValue(fillColor),
            GetBValue(fillColor)
        ));
        Gdiplus::Pen border(Gdiplus::Color(
            GetRValue(borderColor),
            GetGValue(borderColor),
            GetBValue(borderColor)
        ), 1.0f);

        g.FillPath(&fill, &path);
        g.DrawPath(&border, &path);
    }
};

struct ModernButton {
    static void Draw(HDC hdc, const RECT& rc, const wchar_t* text, bool isHovered, bool isPressed, 
                    COLORREF normalColor, COLORREF hoverColor, COLORREF textColor, HFONT hFont) {
        COLORREF fillColor = isHovered ? hoverColor : normalColor;
        
        // Draw rounded rectangle background
        RoundedRect::Draw(hdc, rc, 4, fillColor, RGB(229, 231, 235));

        // Draw text
        SetBkMode(hdc, TRANSPARENT);
        SetTextColor(hdc, textColor);
        HFONT hOldFont = (HFONT)SelectObject(hdc, hFont);
        
        // Center text
        RECT textRc = rc;
        DrawTextW(hdc, text, -1, &textRc, DT_CENTER | DT_VCENTER | DT_SINGLELINE);
        
        SelectObject(hdc, hOldFont);
    }
};

struct ModernInput {
    static void Draw(HDC hdc, const RECT& rc, bool hasFocus) {
        RoundedRect::Draw(hdc, rc, 4, RGB(255, 255, 255),
            hasFocus ? RGB(37, 99, 235) : RGB(229, 231, 235));
    }
};

} // namespace ModernControls