#include "Manager.h"

int WINAPI WinMain(HINSTANCE hInstance, HINSTANCE, LPSTR, int) {
    Manager manager;
    if (!manager.Initialize()) {
        return 1;
    }
    manager.Run();
    manager.Shutdown();
    return 0;
}