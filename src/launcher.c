#include <windows.h>
#include <commctrl.h>
#include "res.h"

#define INSIDE_LAUNCHER
#include "launcher.h"

static char ***ret_argv_addr;

int launcher_cancelled = 0;

void launcher_init(char ***argv_addr) {
  ret_argv_addr = argv_addr;
}

void launcher_free(void) {
  return;
}

static int selected_btn = IDD_MSYS2_BTN;

void launcher_setup_env(void) {
  switch (selected_btn) {
  case IDD_MINGW32_BTN:
    putenv("MSYSTEM=MINGW32");
    break;
  case IDD_MINGW64_BTN:
    putenv("MSYSTEM=MINGW64");
    break;
  default:
    putenv("MSYSTEM=MSYS");
    break;
  }
  return;
}

static char *bash_cmd[] = {
  "/usr/bin/bash", "--login", NULL
};

void launcher_setup_argv(void) {
  *ret_argv_addr = bash_cmd;
}

static void launcher_add_tooltip_to_window(HWND hwnd, char *text) {
  TOOLINFO ti;
  RECT rect;
  HWND hwnd_tt;

  hwnd_tt = CreateWindowEx(WS_EX_TOPMOST, TOOLTIPS_CLASS, NULL,
      WS_POPUP | TTS_NOPREFIX | TTS_ALWAYSTIP,
      0, 0, 0, 0, hwnd, NULL, NULL, NULL );

  SetWindowPos(hwnd_tt, HWND_TOPMOST, 0, 0, 0, 0,
      SWP_NOMOVE | SWP_NOSIZE | SWP_NOACTIVATE);

  GetClientRect(hwnd, &rect);

  ti.cbSize = sizeof(TOOLINFO);
  ti.uFlags = TTF_SUBCLASS;
  ti.hwnd = hwnd;
  ti.hinst = NULL;
  ti.uId = 0;
  ti.lpszText = text;
  ti.rect.left = rect.left;
  ti.rect.top = rect.top;
  ti.rect.right = rect.right;
  ti.rect.bottom = rect.bottom;

  SendMessage(hwnd_tt, TTM_ADDTOOL, 0, (LPARAM) (LPTOOLINFO) &ti);
}

static void launcher_add_tooltip_to_window_by_id(HWND dialog, int id, char *text) {
  HWND hwnd;

  hwnd = GetDlgItem(dialog, id);
  launcher_add_tooltip_to_window(hwnd, text);
}

static void launcher_add_tooltips(HWND hwnd) {
  launcher_add_tooltip_to_window(hwnd,
      "MSYS2 shell launcher");
  launcher_add_tooltip_to_window_by_id(hwnd, IDD_MSYS2_BTN,
      "The emulated shell, for running and building Msys2-specific apps.");
  launcher_add_tooltip_to_window_by_id(hwnd, IDD_MINGW32_BTN,
      "32-bit shell, for running and building native apps.");
  launcher_add_tooltip_to_window_by_id(hwnd, IDD_MINGW64_BTN,
      "64-bit shell, for running and building native apps.");
}

HICON launcher_icon;

INT_PTR CALLBACK launcher_dlgproc(HWND hwnd, UINT uMsg, WPARAM wParam, LPARAM lParam) {
  (void) lParam;

  switch (uMsg) { /* handle the messages */
  case WM_COMMAND:
    switch (LOWORD(wParam)) {
    case IDCANCEL:
      SendMessage(hwnd, WM_CLOSE, 0, 0);
      launcher_cancelled = 1;
      return TRUE;
    case IDD_MSYS2_BTN:
    case IDD_MINGW32_BTN:
    case IDD_MINGW64_BTN:
      DestroyWindow(hwnd);
      selected_btn = LOWORD(wParam);
      return TRUE;
    default:
      return FALSE;
    }
    break;
  case WM_INITDIALOG:
    SendMessage(hwnd, WM_SETICON, 0, (LPARAM)launcher_icon);
    launcher_add_tooltips(hwnd);
    return TRUE;
  case WM_DESTROY:
    break;
  case WM_CLOSE:
    DestroyWindow(hwnd);
    launcher_cancelled = 1;
    return TRUE;
  default:
    return FALSE;
  }

  return FALSE;
}

