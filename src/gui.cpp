#define NOMINMAX
#include <windows.h>
#include <commctrl.h>
#include <strsafe.h>
#include <cmath>
#include "gui.h"
#include "dangerous.h"
#include "resource.h"
#include "msgpump.h"
#include "util.h"

// ...
#undef small

static SmartHandle evGuiOpen(nullptr, CloseHandle);
static SmartHandle evGuiQuit(nullptr, CloseHandle);
static HWND hwndGui;

const int idInGameControls[] = {
    IDC_CHARGE, IDC_CHAIN, IDC_LIVES, IDC_RBITS,
    IDC_FORMATION1, IDC_FORMATION2, IDC_FORMATION3,
    IDC_ZERO_SCORE
};

enum : UINT_PTR {
    TIMER_FREELIB_SAFETY = 1,
};

// Logical values instead of physical positions
struct Trackbar {
    HWND handle{};
    int minValue, maxValue;
    int step = 1;
    bool inverted = false;
    void bind(HWND handle)
    {
        this->handle = handle;
        minValue = SendMessage(handle, TBM_GETRANGEMIN, 0, 0);
        maxValue = SendMessage(handle, TBM_GETRANGEMAX, 0, 0);
    }
    void bind(HWND dialog, int id)
    {
        bind(GetDlgItem(dialog, id));
    }
    void setRange(bool inverted, int minValue, int maxValue, int step = 1, bool redraw = true)
    {
        SendMessage(handle, TBM_SETRANGEMIN, FALSE, 0);
        SendMessage(handle, TBM_SETRANGEMAX, redraw ? TRUE : FALSE, (maxValue - minValue) / step);
        this->minValue = minValue;
        this->maxValue = maxValue;
        this->inverted = inverted;
        this->step = step;
    }
    void clearTicks(bool redraw = true)
    {
        SendMessage(handle, TBM_CLEARTICS, redraw ? TRUE : FALSE, 0);
    }
    void setTick(int value, bool redraw = true)
    {
        SendMessage(handle, TBM_SETTIC, redraw ? TRUE : FALSE, (inverted ? maxValue - value : value - minValue) / step);
    }
    void setLinePage(int line, int page)
    {
        SendMessage(handle, TBM_SETLINESIZE, 0, line / step);
        SendMessage(handle, TBM_SETPAGESIZE, 0, page / step);
    }
    void setValue(int value, bool redraw = true)
    {
        SendMessage(handle, TBM_SETPOS, redraw ? TRUE : FALSE, (inverted ? maxValue - value : value - minValue) / step);
    }
    int getValue()
    {
        int rawValue = SendMessage(handle, TBM_GETPOS, 0, 0) * step;
        return inverted ? maxValue - rawValue : rawValue + minValue;
    }
};

Trackbar tbCharge, tbChain, tbLives, tbRbits;

static void gui_close()
{
    EnableWindow(hwndGame, TRUE);
    ShowWindow(hwndGui, FALSE);
    freezeGame = false;
}

static INT_PTR CALLBACK gui_dlgproc(HWND hWnd, UINT uMsg, WPARAM wParam, LPARAM lParam)
{
    bool handled = false;
    LRESULT wpRes = 0;

    switch (uMsg) {
    case WM_VSCROLL:
    {
        if (!lParam)
            return FALSE;
        switch (GetWindowLong((HWND)lParam, GWL_ID)) {
        case IDC_CHARGE:
            qp->degHyperCharge = 360.f * tbCharge.getValue() / 150.f;
            break;
        case IDC_CHAIN:
            qp->chain = tbChain.getValue();
            qp->tfChainTimer = 180;
            break;
        case IDC_LIVES:
            qp->lives = tbLives.getValue();
            break;
        case IDC_RBITS:
        {
            int old_cRbits = qp->cRbits;
            qp->cRbits = tbRbits.getValue();
            qp->rbitProgress = 0;
            for (int i = old_cRbits; i < qp->cRbits; i++) {
                qp->rbits[i].fExists = 1;
                qp->rbits[i].state = QPSD::Player::Rbit::RBIT_MOVING;
#if 0
                // This is what happens for natural spawns
                qp->rbits[i].pos = {i % 2 ? -200.f : 200.f, 320.f};
#else
                // More convenient
                qp->rbits[i].scrPos = qp->scrPos;
#endif
            }
            for (int i = qp->cRbits; i < old_cRbits; i++)
                qp->rbits[i].fExists = 0;
            break;
        }
        default:
            return FALSE;
        }
        break;
    }
    case WM_COMMAND:
    {
        HWND hCtrl = (HWND)lParam;
        switch (HIWORD(wParam)) {
        case CBN_SELCHANGE:
            switch (LOWORD(wParam)) {
            case IDC_FORMATION1:
            case IDC_FORMATION2:
            case IDC_FORMATION3:
            {
                int sel = SendMessage(hCtrl, CB_GETCURSEL, 0, 0);
                int formation = SendMessage(hCtrl, CB_GETITEMDATA, sel, 0);
                if (1 <= formation && 29 >= formation || 49 == formation) {
                    int idx = LOWORD(wParam) - IDC_FORMATION1;
                    qp->formations[idx] = formation;
                    if (idx == qp->iFormationActive && formation != qp->formation) {
                        qp->formation = formation;
                        qp->scrRbitAnchor = qp->scrPos;
                        qp->radRbitAngle = -1.5707933f;
                        qp->pxRbitRadius = 0;
                        for (auto &rbit : qp->rbits)
                            if (rbit.state != QPSD::Player::Rbit::RBIT_CAUGHT)
                                rbit.state = QPSD::Player::Rbit::RBIT_MOVING;
                    }
                }
                handled = true;
                break;
            }
            default:
                break;
            }
            break;
        case BN_CLICKED:
            switch (LOWORD(wParam)) {
            case IDC_RESUME:
                gui_close();
                handled = true;
                break;
            case IDC_ZERO_SCORE:
                // Total score is constantly recalculated,
                // but a negative stage score persists
                qp->stageScore -= qp->tempScore;
                qp->tempScore = 0; // Just for instant feedback
                handled = true;
                break;
            case IDC_REMOVE_TOOL:
                for (auto id : idInGameControls)
                    EnableWindow(GetDlgItem(hwndGui, id), FALSE);
                for (auto id : {IDC_RESUME, IDC_REMOVE_TOOL})
                    EnableWindow(GetDlgItem(hwndGui, id), FALSE);
                EnableWindow(hwndGui, FALSE);
                unhook();
                freezeGame = false;
                EnableWindow(hwndGame, TRUE);
                SetFocus(hwndGame);
                // There's no easy reliable way to wait for all hooks to
                // definitely have returned in C++.  Theoretically without
                // tail calls, which you can't force, there is a chance
                // that it's still in your code after you release a lock.
                // So just wait a while, fine for normal use.
                SetTimer(hwndGui, TIMER_FREELIB_SAFETY, 200, nullptr);
                handled = true;
                break;
            case IDC_FIX_BACKGROUNDS:
            {
                HWND fixBG = (HWND)lParam;
                SetWindowText(fixBG, fix_backgrounds() ? L"Fixed" : L"Failed");
                EnableWindow(fixBG, FALSE);
                SetFocus(GetDlgItem(hwndGui, IDC_RESUME));
                handled = true;
                break;
            }
            default:
                break;
            }
            break;
        }
        break;
    }
    case WM_NOTIFY:
    {
        NMHDR *nmhdr = (NMHDR*)lParam;
        switch (nmhdr->code) {
            case TTN_NEEDTEXT:
            {
                NMTTDISPINFO *pttDispInfo = (NMTTDISPINFO*)nmhdr;
                int id;
                if (pttDispInfo->uFlags & TTF_IDISHWND)
                    id = GetDlgCtrlID((HWND)nmhdr->idFrom);
                else
                    id = nmhdr->idFrom;
                auto format = L"%d";
                int value;
                switch (id) {
                    case IDC_CHARGE:
                        value = tbCharge.getValue();
                        format = L"%d%%";
                        break;
                    case IDC_CHAIN:
                        value = tbChain.getValue();
                        break;
                    case IDC_LIVES:
                        value = tbLives.getValue();
                        break;
                    case IDC_RBITS:
                        value = tbRbits.getValue();
                        break;
                    default:
                        return FALSE;
                }
                StringCbPrintf(pttDispInfo->szText, 80, format, value);
                handled = true;
                break;
            }
            default:
                return FALSE;
        }
        break;
    }
    case WM_CLOSE:
        gui_close();
        handled = true;
        break;
    case WM_TIMER:
        if (TIMER_FREELIB_SAFETY != wParam)
            return FALSE;
        PostQuitMessage(0);
        handled = true;
        break;
    default:
        return FALSE;
    }
    if (handled) {
        SetWindowLong(hWnd, DWL_MSGRESULT, wpRes);
        return TRUE;
    }
    return FALSE;
}

const struct { int small, large; } styleChainCaps[] = {
    {  400,  750 },
    {  200, 1250 },
    { 1800,  500 },
    {  400,  750 },
};

const wchar_t *formationNames[] = {
    L"Front", L"Rear", L"Orbit", L"Fire", L"Support", L"Shell",
    L"Twin Cannon", L"3-Way", L"Back", L"Wall", L"Umbrella", L"Twin Tail",
    L"Niagara", L"Trap", L"Sword", L"Catapult", L"Bringer", L"Halo", L"Wing",
    L"Wiper", L"Natural", L"Parasol", L"Shield", L"Cross", L"Wave", nullptr,
    L"Child", L"Chainsaw", L"Scissors", L"Final"
};

static void on_gui_open(HANDLE syncObject, void *param)
{
    if (4 == gameMode) {
        for (auto id : idInGameControls)
            EnableWindow(GetDlgItem(hwndGui, id), TRUE);

        tbCharge.setValue((int)(qp->degHyperCharge * 150. / 360. + 0.5));

        auto chainCaps = styleChainCaps[qp->style >= 1 && qp->style <= 4 ? qp->style - 1 : 0];
        tbChain.setRange(true, 0, std::max(chainCaps.small, chainCaps.large));
        tbChain.clearTicks();
        tbChain.setTick(std::min(chainCaps.small, chainCaps.large));
        tbChain.setValue(qp->chain);

        tbLives.setValue(qp->lives);

        tbRbits.setRange(true, 2, qp->style == 4 ? 12 : 10, 2);
        tbRbits.setValue(qp->cRbits);

        for (int i = 0; i < 3; i++) {
            int formation = qp->formations[i];
            auto name = formationNames[49 == formation ? 29 : formation - 1];
            SendDlgItemMessage(hwndGui, IDC_FORMATION1 + i, CB_SELECTSTRING, -1, (LPARAM)name);
        }
        SetFocus(GetDlgItem(hwndGui, IDC_CHARGE));
    } else {
        for (auto id : idInGameControls)
            EnableWindow(GetDlgItem(hwndGui, id), FALSE);
        SetFocus(GetDlgItem(hwndGui, IDC_RESUME));
    }
    if (brokenBackgrounds)
        EnableWindow(GetDlgItem(hwndGui, IDC_FIX_BACKGROUNDS), TRUE);

    ShowWindow(hwndGui, SW_SHOW);
    EnableWindow(hwndGame, FALSE);
}

static void gui_create()
{
    if (hwndGui)
        return;

    hwndGui = CreateDialog(hPracticeDLL, MAKEINTRESOURCE(IDD_POKE_GAME), hwndGame, gui_dlgproc);

    tbCharge.bind(hwndGui, IDC_CHARGE);
    tbCharge.setRange(true, 0, 150);
    tbCharge.setTick(100);
    tbCharge.setLinePage(1, 10);

    tbChain.bind(hwndGui, IDC_CHAIN);
    tbChain.setLinePage(10, 100);

    tbLives.bind(hwndGui, IDC_LIVES);
    tbLives.setRange(true, 0, 9);
    tbLives.setLinePage(1, 9);

    tbRbits.bind(hwndGui, IDC_RBITS);
    tbRbits.setLinePage(1, 10);

    for (auto id : {IDC_FORMATION1, IDC_FORMATION2, IDC_FORMATION3}) {
        int i = 1;
        for (auto name : formationNames) {
            // Skip the unimplemented Familiar.
            // There's also Comet and Satellite but they're after the rest.
            if (!name) { i++; continue; }
            int idx = SendDlgItemMessage(hwndGui, id, CB_ADDSTRING, 0, (LPARAM)name);
            SendDlgItemMessage(hwndGui, id, CB_SETITEMDATA, idx, 30 == i ? 49 : i++);
        }
    }
}

static void gui_free()
{
    DestroyWindow(hwndGui);
}

static void on_gui_quit(HANDLE syncObject, void *param)
{
    PostQuitMessage(0);
}

static DWORD WINAPI gui_main(LPVOID pvParam)
{
    evGuiOpen.reset(CreateEvent(nullptr, FALSE, FALSE, nullptr));
    evGuiQuit.reset(CreateEvent(nullptr, FALSE, FALSE, nullptr));

    SetThreadDpiAwarenessContext(DPI_AWARENESS_CONTEXT_SYSTEM_AWARE);

    INITCOMMONCONTROLSEX iccx;
    iccx.dwSize = sizeof(iccx);
    iccx.dwICC = ICC_WIN95_CLASSES;
    InitCommonControlsEx(&iccx);

    gui_create();

    MessagePump msgPump;
    msgPump.add_syncObject(evGuiOpen.get(), on_gui_open);
    msgPump.add_syncObject(evGuiQuit.get(), on_gui_quit);
    msgPump.add_modelessDialogBox(hwndGui);
    int ret = msgPump.run();

    if (-1 == ret) {
        EnableWindow(hwndGame, TRUE);
        return ret;
    }
    gui_free();
    FreeLibraryAndExitThread(hPracticeDLL, ret);
}

void gui_open()
{
    if (!evGuiOpen)
        return;
    freezeGame = true;
    SetEvent(evGuiOpen.get());
}

void gui_quit()
{
    if (evGuiQuit)
        SetEvent(evGuiQuit.get());
}

void gui_run()
{
    CreateThread(nullptr, 0, gui_main, nullptr, 0, nullptr);
}
