/* Win32 display mode and focus lifetime. No process-wide display mutation for
 * windowed devices; fullscreen owns only the selected monitor's mode. */
static LRESULT CALLBACK device_wndproc(HWND hwnd, UINT msg, WPARAM wp, LPARAM lp)
{
    /* The fullscreen device owns this procedure until it restores the window. */
    struct vkd_device *d = device_window_owner;
    if (!d || d->hooked != hwnd)
        return 0;
    if (d->fullscreen_mode && ((msg == WM_ACTIVATEAPP && !wp) || (msg == WM_ACTIVATE && LOWORD(wp) == WA_INACTIVE))) {
        d->lost = 1;
        d->not_reset = 0;
        w32.ChangeDisplaySettingsExA(d->fullscreen_device, 0, 0, 0, 0);
    }
    if (d->fullscreen_mode && d->lost &&
        ((msg == WM_ACTIVATEAPP && wp) || (msg == WM_ACTIVATE && LOWORD(wp) != WA_INACTIVE)))
        d->not_reset = 1;
    return w32.CallWindowProcA(d->old_wndproc, hwnd, msg, wp, lp);
}
static int device_window_hook(struct vkd_device *d, HWND hwnd)
{
    if (d->hooked == hwnd)
        return 1;
    if (device_window_owner && device_window_owner != d)
        return 0;
    if (d->hooked) {
        if (VKD_GETWNDPROC(d->hooked) == device_wndproc)
            (void)VKD_SETWNDPROC(d->hooked, d->old_wndproc);
        d->hooked = 0;
    }
    d->old_wndproc = VKD_GETWNDPROC(hwnd);
    if (!d->old_wndproc)
        return 0;
    d->hooked = hwnd;
    device_window_owner = d;
    (void)VKD_SETWNDPROC(hwnd, device_wndproc);
    return 1;
}
static void device_window_restore(struct vkd_device *d)
{
    if (d->fullscreen_mode) {
        w32.ChangeDisplaySettingsExA(d->fullscreen_device, 0, 0, 0, 0);
        if (d->hooked && w32.IsWindow(d->hooked)) {
            w32.SetWindowLongA(d->hooked, GWL_STYLE, d->win_style);
            w32.SetWindowLongA(d->hooked, GWL_EXSTYLE, d->win_exstyle);
            w32.SetWindowPos(d->hooked, HWND_NOTOPMOST, d->win_rect.left, d->win_rect.top,
                             d->win_rect.right - d->win_rect.left, d->win_rect.bottom - d->win_rect.top,
                             SWP_FRAMECHANGED | SWP_NOACTIVATE);
        }
        d->fullscreen_mode = 0;
    }
}
static HRESULT device_window_set(struct vkd_device *d, const D3DPRESENT_PARAMETERS *pp)
{
    HWND hwnd = pp->hDeviceWindow ? pp->hDeviceWindow : d->cp.hFocusWindow;
    if (!hwnd || !w32.IsWindow(hwnd))
        return D3DERR_INVALIDCALL;
    if (pp->Windowed) {
        device_window_restore(d);
        /* Windowed devices need no hook; a later fullscreen Reset installs it. */
        return D3D_OK;
    }
    if (!pp->BackBufferWidth || !pp->BackBufferHeight || pp->BackBufferFormat == D3DFMT_UNKNOWN)
        return D3DERR_INVALIDCALL;
    if (d->fullscreen_mode && d->hooked != hwnd)
        device_window_restore(d);
    MONITORINFOEXA monitor;
    memset(&monitor, 0, sizeof monitor);
    monitor.cbSize = sizeof monitor;
    if (!w32.GetMonitorInfoA(w32.MonitorFromWindow(hwnd, MONITOR_DEFAULTTOPRIMARY), (MONITORINFO *)&monitor))
        return D3DERR_NOTAVAILABLE;
    DEVMODEA mode;
    memset(&mode, 0, sizeof mode);
    mode.dmSize = sizeof mode;
    if (!w32.EnumDisplaySettingsA(monitor.szDevice, ENUM_CURRENT_SETTINGS, &mode))
        return D3DERR_NOTAVAILABLE;
    mode.dmPelsWidth = pp->BackBufferWidth;
    mode.dmPelsHeight = pp->BackBufferHeight;
    mode.dmBitsPerPel = pp->BackBufferFormat == D3DFMT_R5G6B5 || pp->BackBufferFormat == D3DFMT_X1R5G5B5 ||
                                pp->BackBufferFormat == D3DFMT_A1R5G5B5
                            ? 16
                            : 32;
    mode.dmFields = DM_PELSWIDTH | DM_PELSHEIGHT | DM_BITSPERPEL;
    if (pp->FullScreen_RefreshRateInHz) {
        mode.dmDisplayFrequency = pp->FullScreen_RefreshRateInHz;
        mode.dmFields |= DM_DISPLAYFREQUENCY;
    }
    if (w32.ChangeDisplaySettingsExA(monitor.szDevice, &mode, 0, CDS_TEST, 0) != DISP_CHANGE_SUCCESSFUL)
        return D3DERR_NOTAVAILABLE;
    if (!d->fullscreen_mode) {
        d->win_style = w32.GetWindowLongA(hwnd, GWL_STYLE);
        d->win_exstyle = w32.GetWindowLongA(hwnd, GWL_EXSTYLE);
        w32.GetWindowRect(hwnd, &d->win_rect);
    }
    if (!device_window_hook(d, hwnd))
        return D3DERR_NOTAVAILABLE;
    if (w32.ChangeDisplaySettingsExA(monitor.szDevice, &mode, 0, CDS_FULLSCREEN, 0) != DISP_CHANGE_SUCCESSFUL)
        return D3DERR_NOTAVAILABLE;
    memcpy(d->fullscreen_device, monitor.szDevice, sizeof d->fullscreen_device);
    d->fullscreen_mode = 1;
    w32.SetWindowLongA(hwnd, GWL_STYLE,
                       (d->win_style & ~(WS_CAPTION | WS_THICKFRAME | WS_MINIMIZEBOX | WS_MAXIMIZEBOX)) | WS_POPUP |
                           WS_VISIBLE);
    w32.SetWindowPos(hwnd, HWND_TOPMOST, monitor.rcMonitor.left, monitor.rcMonitor.top, pp->BackBufferWidth,
                     pp->BackBufferHeight, SWP_FRAMECHANGED | SWP_SHOWWINDOW);
    return D3D_OK;
}
static void device_window_free(struct vkd_device *d)
{
    device_window_restore(d);
    if (d->hooked && w32.IsWindow(d->hooked) && VKD_GETWNDPROC(d->hooked) == device_wndproc)
        (void)VKD_SETWNDPROC(d->hooked, d->old_wndproc);
    if (device_window_owner == d)
        device_window_owner = 0;
    d->hooked = 0;
}
