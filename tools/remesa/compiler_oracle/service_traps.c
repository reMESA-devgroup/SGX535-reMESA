/* SPDX-License-Identifier: MIT */
/* Every unexpected original device/EGL call terminates the CPU replay. */
#include <stdio.h>
#include <stdlib.h>
void PVRSRVGetBCBuffer(void){fputs("BLOCKED PVRSRVGetBCBuffer\n",stderr);_Exit(91);}
void PVRSRVMemSet(void){fputs("BLOCKED PVRSRVMemSet\n",stderr);_Exit(91);}
void PVRSRVPollForValue(void){fputs("BLOCKED PVRSRVPollForValue\n",stderr);_Exit(91);}
void PVRSRVFreeDeviceMem(void){fputs("BLOCKED PVRSRVFreeDeviceMem\n",stderr);_Exit(91);}
void PVRSRVDestroyMutex(void){fputs("BLOCKED PVRSRVDestroyMutex\n",stderr);_Exit(91);}
void PVRSRVGetBCBufferIdFromTag(void){fputs("BLOCKED PVRSRVGetBCBufferIdFromTag\n",stderr);_Exit(91);}
void PVRSRVCreateMutex(void){fputs("BLOCKED PVRSRVCreateMutex\n",stderr);_Exit(91);}
void PVRSRVUnloadLibrary(void){fputs("BLOCKED PVRSRVUnloadLibrary\n",stderr);_Exit(91);}
void PVRSRVOpenBCDevice(void){fputs("BLOCKED PVRSRVOpenBCDevice\n",stderr);_Exit(91);}
void SGXQueueTransfer(void){fputs("BLOCKED SGXQueueTransfer\n",stderr);_Exit(91);}
void PVRSRVLockMutex(void){fputs("BLOCKED PVRSRVLockMutex\n",stderr);_Exit(91);}
void PVRSRVGetAppHint(void){fputs("BLOCKED PVRSRVGetAppHint\n",stderr);_Exit(91);}
void PVRSRVAllocDeviceMem(void){fputs("BLOCKED PVRSRVAllocDeviceMem\n",stderr);_Exit(91);}
void PVRSRVCreateAppHintState(void){fputs("BLOCKED PVRSRVCreateAppHintState\n",stderr);_Exit(91);}
void PVRSRVMapDeviceClassMemory(void){fputs("BLOCKED PVRSRVMapDeviceClassMemory\n",stderr);_Exit(91);}
void PVRSRVUnmapDeviceClassMemory(void){fputs("BLOCKED PVRSRVUnmapDeviceClassMemory\n",stderr);_Exit(91);}
void PVRSRVGetLibFuncAddr(void){fputs("BLOCKED PVRSRVGetLibFuncAddr\n",stderr);_Exit(91);}
void PVRSRVUnlockMutex(void){fputs("BLOCKED PVRSRVUnlockMutex\n",stderr);_Exit(91);}
void PVRSRVClientEvent(void){fputs("BLOCKED PVRSRVClientEvent\n",stderr);_Exit(91);}
void PVRSRVLoadLibrary(void){fputs("BLOCKED PVRSRVLoadLibrary\n",stderr);_Exit(91);}
void PVRSRVGetBCBufferInfo(void){fputs("BLOCKED PVRSRVGetBCBufferInfo\n",stderr);_Exit(91);}
void PVRSRVEnumerateDeviceClass(void){fputs("BLOCKED PVRSRVEnumerateDeviceClass\n",stderr);_Exit(91);}
void PVRSRVFreeAppHintState(void){fputs("BLOCKED PVRSRVFreeAppHintState\n",stderr);_Exit(91);}
void SGXKickTA(void){fputs("BLOCKED SGXKickTA\n",stderr);_Exit(91);}
void PVRSRVEventObjectWait(void){fputs("BLOCKED PVRSRVEventObjectWait\n",stderr);_Exit(91);}
void PVRSRVCloseBCDevice(void){fputs("BLOCKED PVRSRVCloseBCDevice\n",stderr);_Exit(91);}
