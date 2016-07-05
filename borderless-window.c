/* borderless-window - a minimal borderless window with the Windows API
 *
 * Written in 2016 by James Ross-Gowan
 *
 * To the extent possible under law, the author(s) have dedicated all copyright
 * and related and neighboring rights to this software to the public domain
 * worldwide. This software is distributed without any warranty.
 *
 * See <https://creativecommons.org/publicdomain/zero/1.0/> for a copy of the
 * CC0 Public Domain Dedication, which applies to this software.
 */

#include <windows.h>
#include <windowsx.h>
#include <shellapi.h>
#include <uxtheme.h>
#include <dwmapi.h>
#include <versionhelpers.h>
#include <stdlib.h>
#include <stdbool.h>

extern IMAGE_DOS_HEADER __ImageBase;
#define HINST_THISCOMPONENT ((HINSTANCE)&__ImageBase)

#ifndef WM_NCUAHDRAWCAPTION
#define WM_NCUAHDRAWCAPTION (0x00AE)
#endif
#ifndef WM_NCUAHDRAWFRAME
#define WM_NCUAHDRAWFRAME (0x00AF)
#endif

struct window {
	HWND window;

	unsigned width;
	unsigned height;

	RECT rgn;

	bool theme_enabled;
	bool composition_enabled;
};

static void update_region(struct window *data)
{
	RECT old_rgn = data->rgn;

	if (IsMaximized(data->window)) {
		WINDOWINFO wi = { .cbSize = sizeof wi };
		GetWindowInfo(data->window, &wi);

		/* For maximized windows, a region is needed to cut off the non-client
		   borders that hang over the edge of the screen */
		data->rgn = (RECT) {
			.left = wi.rcClient.left - wi.rcWindow.left,
			.top = wi.rcClient.top - wi.rcWindow.top,
			.right = wi.rcClient.right - wi.rcWindow.left,
			.bottom = wi.rcClient.bottom - wi.rcWindow.top,
		};
	} else if (!data->composition_enabled) {
		/* For ordinary themed windows when composition is disabled, a region
		   is needed to remove the rounded top corners. Make it as large as
		   possible to avoid having to change it when the window is resized. */
		data->rgn = (RECT) {
			.left = 0,
			.top = 0,
			.right = 32767,
			.bottom = 32767,
		};
	} else {
		/* Don't mess with the region when composition is enabled and the
		   window is not maximized, otherwise it will lose its shadow */
		data->rgn = (RECT) { 0 };
	}

	/* Avoid unnecessarily updating the region to avoid unnecessary redraws */
	if (EqualRect(&data->rgn, &old_rgn))
		return;
	/* Treat empty regions as NULL regions */
	if (EqualRect(&data->rgn, &(RECT) { 0 }))
		SetWindowRgn(data->window, NULL, TRUE);
	else
		SetWindowRgn(data->window, CreateRectRgnIndirect(&data->rgn), TRUE);
}

static void handle_nccreate(HWND window, CREATESTRUCTW *cs)
{
	struct window *data = cs->lpCreateParams;
	SetWindowLongPtrW(window, GWLP_USERDATA, (LONG_PTR)data);
}

static void handle_compositionchanged(struct window *data)
{
	BOOL enabled = FALSE;
	DwmIsCompositionEnabled(&enabled);
	data->composition_enabled = enabled;

	if (enabled) {
		/* The window needs a frame to show a shadow, so give it the smallest
		   amount of frame possible */
		DwmExtendFrameIntoClientArea(data->window, &(MARGINS) { 0, 0, 1, 0 });
		DwmSetWindowAttribute(data->window, DWMWA_NCRENDERING_POLICY,
		                      &(DWORD) { DWMNCRP_ENABLED }, sizeof(DWORD));
	}

	update_region(data);
}

static bool handle_keydown(struct window *data, DWORD key)
{
	/* Handle various commands that are useful for testing */
	switch (key) {
	case 'H':;
		HDC dc = GetDC(data->window);

		WINDOWINFO wi = { .cbSize = sizeof wi };
		GetWindowInfo(data->window, &wi);

		int width = wi.rcWindow.right - wi.rcWindow.left;
		int height = wi.rcWindow.bottom - wi.rcWindow.top;
		int cwidth = wi.rcClient.right - wi.rcClient.left;
		int cheight = wi.rcClient.bottom - wi.rcClient.top;
		int diffx = width - cwidth;
		int diffy = height - cheight;

		/* Visualize the NCHITTEST values in the client area */
		for (int y = 0, posy = 0; y < height; y++, posy++) {
			/* Compress the window rectangle into the client rectangle by
			   skipping pixels in the middle */
			if (y == cheight / 2)
				y += diffy;
			for (int x = 0, posx = 0; x < width; x++, posx++) {
				if (x == cwidth / 2)
					x += diffx;

				LRESULT ht = SendMessageW(data->window, WM_NCHITTEST, 0,
				                          MAKELPARAM(x + wi.rcWindow.left,
				                                     y + wi.rcWindow.top));
				switch (ht) {
				case HTLEFT:
				case HTTOP:
				case HTRIGHT:
				case HTBOTTOM:
					SetPixel(dc, posx, posy, RGB(255, 0, 0));
					break;
				case HTTOPLEFT:
				case HTTOPRIGHT:
				case HTBOTTOMLEFT:
				case HTBOTTOMRIGHT:
					SetPixel(dc, posx, posy, RGB(0, 255, 0));
					break;
				default:
					SetPixel(dc, posx, posy, RGB(0, 0, 255));
					break;
				}
			}
		}

		ReleaseDC(data->window, dc);
		return true;
	case 'I':;
		static bool icon_toggle;
		HICON icon;

		if (icon_toggle)
			icon = LoadIcon(NULL, IDI_ERROR);
		else
			icon = LoadIcon(NULL, IDI_EXCLAMATION);
		icon_toggle = !icon_toggle;

		/* This should make DefWindowProc try to redraw the icon on the window
		   border. The redraw can be blocked by blocking WM_NCUAHDRAWCAPTION
		   when themes are enabled or unsetting WS_VISIBLE while WM_SETICON is
		   processed. */
		SendMessageW(data->window, WM_SETICON, ICON_BIG, (LPARAM)icon);
		return true;
	case 'T':;
		static bool text_toggle;

		/* This should make DefWindowProc try to redraw the title on the window
		   border. As above, the redraw can be blocked by blocking
		   WM_NCUAHDRAWCAPTION or unsetting WS_VISIBLE while WM_SETTEXT is
		   processed. */
		if (text_toggle)
			SetWindowTextW(data->window, L"window text");
		else
			SetWindowTextW(data->window, L"txet wodniw");
		text_toggle = !text_toggle;

		return true;
	case 'M':;
		static bool menu_toggle;
		HMENU menu = GetSystemMenu(data->window, FALSE);

		/* This should make DefWindowProc try to redraw the window controls.
		   This redraw can be blocked by blocking WM_NCUAHDRAWCAPTION when
		   themes are enabled or unsetting WS_VISIBLE during the EnableMenuItem
		   call (not done here for testing purposes.) */
		if (menu_toggle)
			EnableMenuItem(menu, SC_CLOSE, MF_BYCOMMAND | MF_ENABLED);
		else
			EnableMenuItem(menu, SC_CLOSE, MF_BYCOMMAND | MF_GRAYED);
		menu_toggle = !menu_toggle;

		return true;
	default:
		return false;
	}
}

static bool has_autohide_appbar(UINT edge, RECT mon)
{
	if (IsWindows8Point1OrGreater()) {
		return SHAppBarMessage(ABM_GETAUTOHIDEBAREX, &(APPBARDATA) {
			.cbSize = sizeof(APPBARDATA),
			.uEdge = edge,
			.rc = mon,
		});
	}

	/* Before Windows 8.1, it was not possible to specify a monitor when
	   checking for hidden appbars, so check only on the primary monitor */
	if (mon.left != 0 || mon.top != 0)
		return false;
	return SHAppBarMessage(ABM_GETAUTOHIDEBAR, &(APPBARDATA) {
		.cbSize = sizeof(APPBARDATA),
		.uEdge = edge,
	});
}

static void handle_nccalcsize(struct window *data, WPARAM wparam,
                              LPARAM lparam)
{
	union {
		LPARAM lparam;
		RECT* rect;
	} params = { .lparam = lparam };

	/* DefWindowProc must be called in both the maximized and non-maximized
	   cases, otherwise tile/cascade windows won't work */
	RECT nonclient = *params.rect;
	DefWindowProcW(data->window, WM_NCCALCSIZE, wparam, params.lparam);
	RECT client = *params.rect;

	if (IsMaximized(data->window)) {
		WINDOWINFO wi = { .cbSize = sizeof wi };
		GetWindowInfo(data->window, &wi);

		/* Maximized windows always have a non-client border that hangs over
		   the edge of the screen, so the size proposed by WM_NCCALCSIZE is
		   fine. Just adjust the top border to remove the window title. */
		*params.rect = (RECT) {
			.left = client.left,
			.top = nonclient.top + wi.cyWindowBorders,
			.right = client.right,
			.bottom = client.bottom,
		};

		HMONITOR mon = MonitorFromWindow(data->window, MONITOR_DEFAULTTOPRIMARY);
		MONITORINFO mi = { .cbSize = sizeof mi };
		GetMonitorInfoW(mon, &mi);

		/* If the client rectangle is the same as the monitor's rectangle,
		   the shell assumes that the window has gone fullscreen, so it removes
		   the topmost attribute from any auto-hide appbars, making them
		   inaccessible. To avoid this, reduce the size of the client area by
		   one pixel on a certain edge. The edge is chosen based on which side
		   of the monitor is likely to contain an auto-hide appbar, so the
		   missing client area is covered by it. */
		if (EqualRect(params.rect, &mi.rcMonitor)) {
			if (has_autohide_appbar(ABE_BOTTOM, mi.rcMonitor))
				params.rect->bottom--;
			else if (has_autohide_appbar(ABE_LEFT, mi.rcMonitor))
				params.rect->left++;
			else if (has_autohide_appbar(ABE_TOP, mi.rcMonitor))
				params.rect->top++;
			else if (has_autohide_appbar(ABE_RIGHT, mi.rcMonitor))
				params.rect->right--;
		}
	} else {
		/* For the non-maximized case, set the output RECT to what it was
		   before WM_NCCALCSIZE modified it. This will make the client size the
		   same as the non-client size. */
		*params.rect = nonclient;
	}
}

static LRESULT handle_nchittest(struct window *data, int x, int y)
{
	if (IsMaximized(data->window))
		return HTCLIENT;

	POINT mouse = { x, y };
	ScreenToClient(data->window, &mouse);

	/* The horizontal frame should be the same size as the vertical frame,
	   since the NONCLIENTMETRICS structure does not distinguish between them */
	int frame_size = GetSystemMetrics(SM_CXFRAME) +
	                 GetSystemMetrics(SM_CXPADDEDBORDER);
	/* The diagonal size handles are wider than the frame */
	int diagonal_width = frame_size * 2 + GetSystemMetrics(SM_CXBORDER);

	if (mouse.y < frame_size) {
		if (mouse.x < diagonal_width)
			return HTTOPLEFT;
		if (mouse.x >= data->width - diagonal_width)
			return HTTOPRIGHT;
		return HTTOP;
	}

	if (mouse.y >= data->height - frame_size) {
		if (mouse.x < diagonal_width)
			return HTBOTTOMLEFT;
		if (mouse.x >= data->width - diagonal_width)
			return HTBOTTOMRIGHT;
		return HTBOTTOM;
	}

	if (mouse.x < frame_size)
		return HTLEFT;
	if (mouse.x >= data->width - frame_size)
		return HTRIGHT;
	return HTCLIENT;
}

static void handle_paint(struct window *data)
{
	PAINTSTRUCT ps;
	HDC dc = BeginPaint(data->window, &ps);
	HBRUSH bb = CreateSolidBrush(RGB(0, 255, 0));

	/* Draw a rectangle on the border of the client area for testing */
	FillRect(dc, &(RECT) { 0, 0, 1, data->height }, bb);
	FillRect(dc, &(RECT) { 0, 0, data->width, 1 }, bb);
	FillRect(dc, &(RECT) { data->width - 1, 0, data->width, data->height }, bb);
	FillRect(dc, &(RECT) { 0, data->height - 1, data->width, data->height }, bb);

	DeleteObject(bb);
	EndPaint(data->window, &ps);
}

static void handle_themechanged(struct window *data)
{
	data->theme_enabled = IsThemeActive();
}

static void handle_windowposchanged(struct window *data, const WINDOWPOS *pos)
{
	RECT client;
	GetClientRect(data->window, &client);
	unsigned old_width = data->width;
	unsigned old_height = data->height;
	data->width = client.right;
	data->height = client.bottom;
	bool client_changed = data->width != old_width || data->height != old_height;

	if (client_changed || (pos->flags & SWP_FRAMECHANGED))
		update_region(data);

	if (client_changed) {
		/* Invalidate the changed parts of the rectangle drawn in WM_PAINT */
		if (data->width > old_width) {
			InvalidateRect(data->window, &(RECT) {
				old_width - 1, 0, old_width, old_height
			}, TRUE);
		} else {
			InvalidateRect(data->window, &(RECT) {
				data->width - 1, 0, data->width, data->height
			}, TRUE);
		}
		if (data->height > old_height) {
			InvalidateRect(data->window, &(RECT) {
				0, old_height - 1, old_width, old_height
			}, TRUE);
		} else {
			InvalidateRect(data->window, &(RECT) {
				0, data->height - 1, data->width, data->height
			}, TRUE);
		}
	}
}

static LRESULT handle_message_invisible(HWND window, UINT msg, WPARAM wparam,
	LPARAM lparam)
{
	LONG_PTR old_style = GetWindowLongPtrW(window, GWL_STYLE);

	/* Prevent Windows from drawing the default title bar by temporarily
	   toggling the WS_VISIBLE style. This is recommended in:
	   https://blogs.msdn.microsoft.com/wpfsdk/2008/09/08/custom-window-chrome-in-wpf/ */
	SetWindowLongPtrW(window, GWL_STYLE, old_style & ~WS_VISIBLE);
	LRESULT result = DefWindowProcW(window, msg, wparam, lparam);
	SetWindowLongPtrW(window, GWL_STYLE, old_style);

	return result;
}

static LRESULT CALLBACK borderless_window_proc(HWND window, UINT msg,
	WPARAM wparam, LPARAM lparam)
{
	struct window *data = (void*)GetWindowLongPtrW(window, GWLP_USERDATA);
	if (!data) {
		/* Due to a longstanding Windows bug, overlapped windows will receive a
		   WM_GETMINMAXINFO message before WM_NCCREATE. This is safe to ignore.
		   It doesn't need any special handling anyway. */
		if (msg == WM_NCCREATE)
			handle_nccreate(window, (CREATESTRUCTW*)lparam);
		return DefWindowProcW(window, msg, wparam, lparam);
	}

	switch (msg) {
	case WM_CLOSE:
		DestroyWindow(window);
		return 0;
	case WM_DESTROY:
		PostQuitMessage(0);
		return 0;
	case WM_DWMCOMPOSITIONCHANGED:
		handle_compositionchanged(data);
		return 0;
	case WM_KEYDOWN:
		if (handle_keydown(data, wparam))
			return 0;
		break;
	case WM_LBUTTONDOWN:
		/* Allow window dragging from any point */
		ReleaseCapture();
		SendMessageW(window, WM_NCLBUTTONDOWN, HTCAPTION, 0);
		return 0;
	case WM_NCACTIVATE:
		/* DefWindowProc won't repaint the window border if lParam (normally a
		   HRGN) is -1. This is recommended in:
		   https://blogs.msdn.microsoft.com/wpfsdk/2008/09/08/custom-window-chrome-in-wpf/ */
		return DefWindowProcW(window, msg, wparam, -1);
	case WM_NCCALCSIZE:
		handle_nccalcsize(data, wparam, lparam);
		return 0;
	case WM_NCHITTEST:
		return handle_nchittest(data, GET_X_LPARAM(lparam),
		                        GET_Y_LPARAM(lparam));
	case WM_NCPAINT:
		/* Only block WM_NCPAINT when composition is disabled. If it's blocked
		   when composition is enabled, the window shadow won't be drawn. */
		if (!data->composition_enabled)
			return 0;
		break;
	case WM_NCUAHDRAWCAPTION:
	case WM_NCUAHDRAWFRAME:
		/* These undocumented messages are sent to draw themed window borders.
		   Block them to prevent drawing borders over the client area. */
		return 0;
	case WM_PAINT:
		handle_paint(data);
		return 0;
	case WM_SETICON:
	case WM_SETTEXT:
		/* Disable painting while these messages are handled to prevent them
		   from drawing a window caption over the client area, but only when
		   composition and theming are disabled. These messages don't paint
		   when composition is enabled and blocking WM_NCUAHDRAWCAPTION should
		   be enough to prevent painting when theming is enabled. */
		if (!data->composition_enabled && !data->theme_enabled)
			return handle_message_invisible(window, msg, wparam, lparam);
		break;
	case WM_THEMECHANGED:
		handle_themechanged(data);
		break;
	case WM_WINDOWPOSCHANGED:
		handle_windowposchanged(data, (WINDOWPOS*)lparam);
		return 0;
	}

	return DefWindowProcW(window, msg, wparam, lparam);
}

int CALLBACK wWinMain(HINSTANCE inst, HINSTANCE prev, LPWSTR cmd, int show)
{
	ATOM cls = RegisterClassExW(&(WNDCLASSEXW) {
		.cbSize = sizeof(WNDCLASSEXW),
		.lpfnWndProc = borderless_window_proc,
		.hInstance = HINST_THISCOMPONENT,
		.hCursor = LoadCursor(NULL, IDC_ARROW),
		.hbrBackground = (HBRUSH)(COLOR_WINDOW + 1),
		.lpszClassName = L"borderless-window",
	});

	struct window *data = calloc(1, sizeof(struct window));
	data->window = CreateWindowExW(
		WS_EX_APPWINDOW | WS_EX_LAYERED,
		(LPWSTR)MAKEINTATOM(cls),
		L"Borderless Window",
		WS_OVERLAPPEDWINDOW | WS_SIZEBOX,
		CW_USEDEFAULT, CW_USEDEFAULT, 200, 200,
		NULL, NULL, HINST_THISCOMPONENT, data);

	/* Make the window a layered window so the legacy GDI API can be used to
	   draw to it without messing up the area on top of the DWM frame. Note:
	   This is not necessary if other drawing APIs are used, eg. GDI+, OpenGL,
	   Direct2D, Direct3D, DirectComposition, etc. */
	SetLayeredWindowAttributes(data->window, RGB(255, 0, 255), 0, LWA_COLORKEY);

	handle_compositionchanged(data);
	handle_themechanged(data);
	ShowWindow(data->window, SW_SHOWDEFAULT);
	UpdateWindow(data->window);

	MSG message;
	while (GetMessageW(&message, NULL, 0, 0)) {
		TranslateMessage(&message);
		DispatchMessageW(&message);
	}

	free(data);
	UnregisterClassW((LPWSTR)MAKEINTATOM(cls), HINST_THISCOMPONENT);
	return message.wParam;
}
