/*
 * dpmi.h -- for Apogee's sound code in the EiN build: WebAssembly's
 * memory needs no locking against being paged out.
 */
#ifndef EIN_DPMI_H
#define EIN_DPMI_H

enum DPMI_Errors
{
	DPMI_Warning = -2,
	DPMI_Error = -1,
	DPMI_Ok = 0
};

#define DPMI_Lock(variable) (DPMI_Ok)
#define DPMI_Unlock(variable) ((void) 0)
#define DPMI_LockMemory(address, length) (DPMI_Ok)
#define DPMI_UnlockMemory(address, length) ((void) 0)
#define DPMI_LockMemoryRegion(start, end) (DPMI_Ok)
#define DPMI_UnlockMemoryRegion(start, end) ((void) 0)

#endif
