extern char* preset_msystem_to_set;
extern int mintty_main(int, char**);

int main(int argc, char **argv) {
  int ret;

#ifdef MINTTY_AS_MSYS2
  preset_msystem_to_set = "MSYS";
#endif

  ret = mintty_main(argc, argv);
  return ret;
}
