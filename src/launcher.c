#include <windows.h>
#include <commctrl.h>
#include "res.h"
#include "std.h"
#include "win.h"

#define INSIDE_LAUNCHER
#include "launcher.h"

static char ***ret_argv_addr;

int launcher_cancelled = 0;

static char **shells;
size_t shells_sz = 4;
size_t shells_n = 0;

static char *launcher_argv[2] = {
  NULL, NULL
};

void launcher_init(char ***argv_addr) {
  ret_argv_addr = argv_addr;
  shells = newn(char*, shells_sz);
}

void launcher_free(void) {
  size_t i;
  for (i = 0; i < shells_n; i++) {
    free(shells[i]);
  }
  free(shells);
  if (launcher_argv[0]) {
    free(launcher_argv[0]);
  }
}

static int selected_btn = IDD_MSYS2_BTN;
static int selected_exe = 0;

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

void launcher_setup_argv(void) {
  const char *cmdname;

  /* Global variable from winmain.c, used to pass the filepath to execute to
   * child_create() from main() ("Work out what to execute."). */
  cmd = shells[selected_exe];

  /* Prepending "-" to shell's argv[0] should make it behave as a login one. */
  cmdname = strrchr(cmd, '/');
  if (cmdname == NULL) {
    cmdname = cmd;
  } else {
    cmdname += 1;
  }
  if (launcher_argv[0]) {
    free(launcher_argv[0]);
  }
  launcher_argv[0] = asform("-%s", cmdname);
  launcher_argv[1] = NULL;

  *ret_argv_addr = launcher_argv;
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
  launcher_add_tooltip_to_window_by_id(hwnd, IDD_ETC_SHELLS,
      "Shell executable to run");
  launcher_add_tooltip_to_window_by_id(hwnd, IDD_MSYS2_BTN,
      "The emulated shell, for running and building Msys2-specific apps.");
  launcher_add_tooltip_to_window_by_id(hwnd, IDD_MINGW32_BTN,
      "32-bit shell, for running and building native apps.");
  launcher_add_tooltip_to_window_by_id(hwnd, IDD_MINGW64_BTN,
      "64-bit shell, for running and building native apps.");
}

static void launcher_add_line_to_combo_box(HWND hwnd, const char *text) {
  SendMessage(hwnd, CB_ADDSTRING, 0, (LPARAM) text);
}

static void launcher_select_line_in_combo_box(HWND hwnd, const char *text) {
  (void) text;
  WPARAM n = shells_n - 1;
  SendMessage(hwnd, CB_SETCURSEL, n, 0);
}

static HWND etc_shells;

static void launcher_add_shells(HWND dialog) {
  size_t i;
  FILE *f;
  char line[1024];

  f = fopen("/etc/shells", "r");
  if (!f) {
    shells[0] = strdup("/usr/bin/sh");
    shells_n = 1;
  } else {
    while ( fgets(line, sizeof line, f) != NULL ) {
      char *s = line;
      char *s1;
      while (*s == ' ' || *s == '\t') { s++; }
      if (*s == '#' || *s == '\r' || *s == '\n' || *s == '\0') { continue; }
      while ((s1 = strrchr(s, '\r')) || (s1 = strrchr(s, '\n'))) { *s1 = '\0'; }
      shells[shells_n] = strdup(s);
      shells_n++;
      if (shells_n >= shells_sz) {
        shells_sz *= 2;
        shells = renewn(shells, sizeof(char*) * shells_sz);
      }
    }
    fclose(f);
  }

  etc_shells = GetDlgItem(dialog, IDD_ETC_SHELLS);
  for (i = 0; i < shells_n; i++) {
    launcher_add_line_to_combo_box(etc_shells, shells[i]);
  }
  launcher_select_line_in_combo_box(etc_shells, shells[0]);
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
      selected_exe = SendMessage(etc_shells, CB_GETCURSEL, 0, 0);
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
    launcher_add_shells(hwnd);
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

