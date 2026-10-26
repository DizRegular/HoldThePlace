gcc $1 -o airmouse.exe \
  -I$(brew --prefix raylib)/include -L$(brew --prefix raylib)/lib -lraylib \
  -I$(brew --prefix cjson)/include \
  -framework OpenGL \
  -framework IOKit \
  -framework Cocoa \
  -framework CoreVideo