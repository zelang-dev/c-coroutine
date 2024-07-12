#include <windows.h>
#include <stdio.h>
#include <process.h>

int hThreadHandle = 0;

VOID CALLBACK TimerProc(HWND hWnd, UINT nMsg, UINT nIDEvent, DWORD dwTime) {
    printf("TimerId this is a test\n");
    fflush(stdout);
}


int begin_timer() {
    MSG Msg;
    UINT TimerId = SetTimer(NULL, 0, 1000, (TIMERPROC)&TimerProc);
    printf("TimerId: %d\n", TimerId);
    if (!TimerId)
        return 16;
    while (GetMessage(&Msg, NULL, 0, 0))
        DispatchMessage(&Msg);
}

int main(int argc, char *argv[], char *envp[]) {
    int Counter = 0;
    _beginthread((_beginthread_proc_type)begin_timer, 0, NULL);
    Sleep(5000);
    return 0;
}
