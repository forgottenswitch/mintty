#include <windows.h>
#include <commctrl.h>

#include <sys/cygwin.h>
#include <sys/stat.h>
#include <pwd.h>

#include "res.h"
#include "std.h"
#include "win.h"

#define INSIDE_LAUNCHER
#include "launcher.h"

#define DEFAULT_SHELL "/usr/bin/sh"
#define LINELEN 1024

static char ***ret_argv_addr;

int launcher_cancelled = 0;
int launcher_do_dedicated_window = 0;

const char* default_preferred_shells[] = {
  "bin/bash", "bash", "bin/sh", "sh", NULL
};

static char* get_preferred_shell_config_path(void) {
  char *xdg, *ret;

  xdg = get_xdg_dir();
  ret = asform("%s/preferred_shell", xdg);
  free(xdg);
  return ret;
}

static char* fgets_nonempty_noncomment_line(char *buf, size_t bufsiz, FILE *stream);

static char* get_preferred_shell(void) {
  char *s, *xdg_shell;
  char *ret = NULL;
  char line[LINELEN];
  FILE *f;
  xdg_shell = get_preferred_shell_config_path();
  f = fopen(xdg_shell, "r");
  if (f != NULL) {
    for (;;) {
      s = fgets_nonempty_noncomment_line(line, sizeof line, f);
      if (s == NULL) { break; }
      ret = strdup(s);
      break;
    }
    fclose(f);
  }
  free(xdg_shell);
  return ret;
}

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
    setenv("MSYSTEM", "MINGW32", true);
    break;
  case IDD_MINGW64_BTN:
    setenv("MSYSTEM", "MINGW64", true);
    break;
  default:
    setenv("MSYSTEM", "MSYS", true);
    break;
  }
  return;
}

static void setup_login_shell_argv(char *prog, char **argv) {
  const char *progname;

  /* Prepending "-" to shell's argv[0] should make it behave as a login one. */
  progname = strrchr(prog, '/');
  if (progname == NULL) {
    progname = prog;
  } else {
    progname += 1;
  }
  argv[0] = asform("-%s", progname);
  argv[1] = NULL;
}

void launcher_setup_argv(void) {
  /* Global variable from winmain.c, used to pass the filepath to execute to
   * child_create() from main() ("Work out what to execute."). */
  cmd = shells[selected_exe];

  setup_login_shell_argv(cmd, launcher_argv);
  *ret_argv_addr = launcher_argv;
}

void launcher_setup_argv_from_prefs(void) {
  char *preferred_shell;

  preferred_shell = get_preferred_shell();
  if (preferred_shell == NULL) {
    preferred_shell = strdup(DEFAULT_SHELL);
  }
  cmd = preferred_shell; /* See launcher_setup_argv */
  setup_login_shell_argv(cmd, launcher_argv);
  *ret_argv_addr = launcher_argv;
}

/* Should only be called after launcher_setup_argv. */
void launcher_save_prefs(void) {
  FILE *f;
  char *xdg, *xdg_shell;

  xdg = get_xdg_dir();
  if (xdg) {
    if (access(xdg, R_OK) != 0) {
      /* mkdir ~/.config */
      char* xdg_slash = strrchr(xdg, '/');
      if (xdg_slash != NULL) {
        *xdg_slash = 0;
        mkdir(xdg, S_IRWXU | S_IRWXG | S_IRWXO);
        *xdg_slash = '/';
      }
      /* mkdir ~/.config/mintty */
      mkdir(xdg, S_IRWXU | S_IRWXG | S_IRWXO);
    }
    free(xdg);
  }

  xdg_shell = get_preferred_shell_config_path();
  f = fopen(xdg_shell, "w");
  if (f != NULL) {
    fprintf(f, "%s", cmd);
    fclose(f);
  }
  free(xdg_shell);
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

static void launcher_add_tooltip_to_window_by_id(HWND dialog, int id, const char *text) {
  HWND hwnd;

  hwnd = GetDlgItem(dialog, id);
  launcher_add_tooltip_to_window(hwnd, strdup(text));
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
  launcher_add_tooltip_to_window_by_id(hwnd, IDD_DEDICATED,
      "Use a separate Taskbar item, and do not pop up this dialog in new windows.");
}

static void add_line_to_combo_box(HWND hwnd, const char *text) {
  SendMessage(hwnd, CB_ADDSTRING, 0, (LPARAM) text);
}

static void select_nth_line_in_combo_box(HWND hwnd, WPARAM n) {
  SendMessage(hwnd, CB_SETCURSEL, n, 0);
}

/* Return the index of the first shell that matches the text after or on a slash, or -1. */
static ssize_t find_shell_match(const char *text) {
  size_t i;
  while (*text == '/') { text++; }
  for (i = 0; i < shells_n; i++) {
    char *shell = shells[i];
    char *s = shell + strlen(shell);
    while (s > shell) {
      while ((s > shell) && (*s == '/')) { s--; }
      while ((s > shell) && (*s != '/')) { s--; }
      if (strcmp(s + 1, text) == 0) {
        return (ssize_t) i;
      }
      if (strcmp(s, text) == 0) {
        return (ssize_t) i;
      }
    }
  }
  return -1;
}

static HWND etc_shells;

static char* fgets_nonempty_noncomment_line(char *buf, size_t bufsiz, FILE *stream) {
  for (;;) {
    if ( fgets(buf, bufsiz, stream) == NULL) {
      return NULL;
    }
    char *s = buf;
    char *s1;
    while (*s == ' ' || *s == '\t') { s++; }
    if (*s == '#' || *s == '\r' || *s == '\n' || *s == '\0') { continue; }
    while ((s1 = strrchr(s, '\r')) || (s1 = strrchr(s, '\n'))) { *s1 = '\0'; }
    return s;
  }
  return NULL;
}

static void launcher_add_shells(HWND dialog) {
  size_t i;
  ssize_t j;
  FILE *f;
  char line[LINELEN];
  char *s;
  char *preferred_shell = NULL;

  f = fopen("/etc/shells", "r");
  if (!f) {
    shells[0] = strdup(DEFAULT_SHELL);
    shells_n = 1;
  } else {
    for (;;) {
      s = fgets_nonempty_noncomment_line(line, sizeof line, f);
      if (s == NULL) { break; }
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
    add_line_to_combo_box(etc_shells, shells[i]);
  }

  j = -1;

  preferred_shell = get_preferred_shell();
  if (preferred_shell) {
    j = find_shell_match(preferred_shell);
    if (j < 0) {
      char *preferred_shell_name =
          strrchr(preferred_shell, '/');
      if (preferred_shell_name != NULL) {
        preferred_shell_name++;
        j = find_shell_match(preferred_shell_name);
      }
    }
    free(preferred_shell);
  }

  const char **dflt_sh;
  for (dflt_sh = default_preferred_shells; (j < 0) && (*dflt_sh != NULL); dflt_sh++) {
    j = find_shell_match(*dflt_sh);
  }

  if (j < 0) {
    j = 0;
  }

  select_nth_line_in_combo_box(etc_shells, j);
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
      launcher_do_dedicated_window = IsDlgButtonChecked(hwnd, IDD_DEDICATED);
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

