extern char* preset_msystem_to_set;
extern int mintty_main(int, char**);

int main(int argc, char **argv) {
  int ret;

#ifdef MINTTY_AS_MSYS2
  preset_msystem_to_set = "MSYS";
#elif defined(MINTTY_AS_MINGW32)
  preset_msystem_to_set = "MINGW32";
#elif defined(MINTTY_AS_MINGW64)
  preset_msystem_to_set = "MINGW64";
#endif

  ret = mintty_main(argc, argv);
  return ret;
}
