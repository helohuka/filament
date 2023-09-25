
#include "gamedriver/GameDriver.h"

int SDL_main(int argc, char** argv) {
    GameDriver::get().mainLoop();
    return 0;
}
