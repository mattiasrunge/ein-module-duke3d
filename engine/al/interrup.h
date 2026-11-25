/*
 * interrup.h -- for Apogee's sound code in the EiN build, where the
 * music runs on the page's thread between the game's turns, so there is
 * no interrupt to hold off.
 */
#ifndef EIN_INTERRUP_H
#define EIN_INTERRUP_H

#define DisableInterrupts() (0UL)
#define RestoreInterrupts(flags) ((void) (flags))

#endif
