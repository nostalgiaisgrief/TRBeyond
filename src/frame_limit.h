#ifndef FRAME_LIMIT_H
#define FRAME_LIMIT_H
/* Shared Win32 render pacing. Simulation and movie clocks are independent.
   Settings: 0 = uncapped, 1 = 60 Hz, 2 = 30 Hz. */
static void tomb_frame_limit_wait(LARGE_INTEGER previous,int setting,int synced){
    if(!setting || !previous.QuadPart)return;
    LARGE_INTEGER frequency,now;QueryPerformanceFrequency(&frequency);QueryPerformanceCounter(&now);
    double period=1.0/(setting==1?60:30),lead=synced?.001:0;
    while((double)(now.QuadPart-previous.QuadPart)/frequency.QuadPart<period-lead){
        double remaining=period-lead-(double)(now.QuadPart-previous.QuadPart)/frequency.QuadPart;
        Sleep(remaining>.002?1:0);QueryPerformanceCounter(&now);
    }
}
#endif
