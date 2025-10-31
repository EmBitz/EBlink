/**
 * @file ebmon.h
 * @brief EBmonitor debug tool for redirecting STDIO in embedded systems
 * @author Gerard Zagema
 * @company Sysdes
 * @date 2025-10-25
 *
 * This module provides an in-memory pipe system to redirect STDOUT and STDIN
 * for debugging purposes in embedded systems using EmBitz IDE.
 * It supports circular buffers for both input and output streams and
 * optional blocking behavior for writes.
 *
 * @defgroup EmBitz EBmonitor debug tool
 * @{
 */

#ifndef EBMON_H
#define EBMON_H

#include <stdio.h>
#include <stdint.h>

/** Uncomment to skip EBmonitor initialization */
//#define NO_EBMON_INIT

/** Uncomment if write must wait until buffer has space */
//#define EBMON_WRITE_WAIT

/** Initial buffer sizes */
#ifndef STDOUT_BUFFERSIZE
#define STDOUT_BUFFERSIZE 256
#endif

#ifndef STDIN_BUFFERSIZE
#define STDIN_BUFFERSIZE  16
#endif

/** Flush a pipe buffer */
void EBmonitor_flush(FILE* file);

/** Check if stdin has data available */
int EBmonitor_kbhit(void);


#endif /* EBMON_H */

/*@}*/ /* end of group EmBitz */
