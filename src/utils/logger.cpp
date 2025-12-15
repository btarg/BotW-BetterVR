
HANDLE Log::consoleHandle = NULL;
double Log::timeFrequency = 0.0f;

#define _DEBUG

Log::Log() {
#ifdef _DEBUG
    AllocConsole();
    SetConsoleTitleA("BetterVR Debugging Console");
    consoleHandle = GetStdHandle(STD_OUTPUT_HANDLE);
#endif
    Log::print<INFO>("Successfully started BetterVR!");
    LARGE_INTEGER timeLI;
    QueryPerformanceFrequency(&timeLI);
    timeFrequency = double(timeLI.QuadPart) / 1000.0;
}

Log::~Log() {
    Log::print<INFO>("Shutting down BetterVR debugging console...");
#ifdef _DEBUG
    FreeConsole();
#endif
}

void Log::printTimeElapsed(const char* message_prefix, LARGE_INTEGER time) {
    LARGE_INTEGER timeNow;
    QueryPerformanceCounter(&timeNow);
    Log::print<INFO>("{}: {} ms", message_prefix, double(time.QuadPart - timeNow.QuadPart) / timeFrequency);
}