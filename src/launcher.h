#ifndef INSIDE_LAUNCHER
extern int launcher_cancelled;
extern int launcher_do_dedicated_window;
extern HICON launcher_icon;
#endif

void launcher_init(char ***argv_addr);
void launcher_free(void);

void launcher_setup_env(void);
void launcher_setup_argv(void);
void launcher_save_prefs(void);
void launcher_setup_argv_from_prefs(void);

INT_PTR CALLBACK launcher_dlgproc(HWND hwnd, UINT uMsg, WPARAM wParam, LPARAM lParam);
