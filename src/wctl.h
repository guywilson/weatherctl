#ifndef _INCL_WCTL
#define _INCL_WCTL

void    cleanup(void);
void    handleSignal(int sigNum);
void    daemonise();
void    resetAVR();
float   getCPUTemp();

#endif