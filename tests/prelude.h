#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include <switch.h>

static int s_nxlinkSock = -1;

extern "C" void userAppInit()
{
    if (R_FAILED(socketInitializeDefault()))
        return;

    s_nxlinkSock = nxlinkStdio();
    if (s_nxlinkSock >= 0)
        printf("printf output now goes to nxlink server\n");
    else
        socketExit();
}

extern "C" void userAppExit()
{
    if (s_nxlinkSock >= 0)
    {
        socketExit();
        s_nxlinkSock = -1;
    }
}
