gcc $1 -o main \
  -I$(brew --prefix raylib)/include -L$(brew --prefix raylib)/lib -lraylib \
  -I$(brew --prefix mosquitto)/include -L$(brew --prefix mosquitto)/lib -lmosquitto \
  -I$(brew --prefix cjson)/include \
  -framework OpenGL \
  -framework IOKit \
  -framework Cocoa \
  -framework CoreVideo