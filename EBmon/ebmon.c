/**
 * @file ebmon.c
 * @brief Implementation of EBmonitor debug tool
 * @author Gerard Zagema
 * @company Sysdes
 * @date 2025-10-25
 */

/** \defgroup EmBitz EBmonitor debug tool
  @{
 */

#include "ebmon.h"


/** \brief Buffers for STDIO pipes */
static char _stdout_buffer[STDOUT_BUFFERSIZE] __attribute__((aligned(4)));
static char _stdin_buffer[STDIN_BUFFERSIZE]   __attribute__((aligned(4)));

/** \brief Pipe control block */
typedef struct _std_pipe
{
    uint16_t length __attribute__((aligned(4)));         /**< Length of the buffer */
    volatile uint16_t tail __attribute__((aligned(4)));  /**< Tail index (read) */
    volatile uint16_t head __attribute__((aligned(4)));  /**< Head index (write) */
    void *ptr __attribute__((aligned(4)));               /**< Pointer to the buffer */
} _std_pipe __attribute__((aligned(4)));

/** \brief STDIO pipes */
static _std_pipe _eb_monitor_stdout = {STDOUT_BUFFERSIZE, 0, 0, _stdout_buffer};
static _std_pipe _eb_monitor_stdin  = {STDIN_BUFFERSIZE,  0, 0, _stdin_buffer};

#ifndef NO_EBMON_INIT
static uint8_t _eb_initialized = 0;

/** \brief Initialize EBmonitor (called before first write) */
static void ebmonitor_init(void)
{
    _stdout_buffer[1] = '\f';  /**< Clear EBterm screen at startup */
    _eb_monitor_stdout.head = 1;
}
#endif

/** \brief Flush (clear) a pipe buffer
 *  \param file FILE* the pipe to flush (stdin or stdout)
 */
void EBmonitor_flush(FILE* file)
{
    if(file == stdin)
        _eb_monitor_stdin.tail = _eb_monitor_stdin.head;
    else if(file == stdout)
        _eb_monitor_stdout.head = _eb_monitor_stdout.tail;
}

/** \brief Check if there is data in the stdin pipe
 *  \return int 1 if data available, 0 otherwise
 */
int EBmonitor_kbhit(void)
{
    return (_eb_monitor_stdin.tail != _eb_monitor_stdin.head);
}

/** \brief Write data to the stdout pipe
 *  \param file int pipe handle (unused)
 *  \param ptr char* pointer to data buffer
 *  \param len int length of data
 *  \return int number of characters written
 */
int _write(int file, char *ptr, int len)
{
    int charWritten = 0;

#ifndef NO_EBMON_INIT
    if(!_eb_initialized)
    {
        _eb_initialized = 1;
        ebmonitor_init();
    }
#endif

    int head = _eb_monitor_stdout.head;
    for (; len > 0; --len)
    {
        if(++head >= _eb_monitor_stdout.length)
            head = 0;

#ifdef EBMON_WRITE_WAIT
        while(head == _eb_monitor_stdout.tail);
#else
        if(head == _eb_monitor_stdout.tail)
            return charWritten;
#endif

        ((char*)_eb_monitor_stdout.ptr)[head] = *ptr++;
        _eb_monitor_stdout.head = head;
        charWritten++;
    }

    return charWritten;
}

/** \brief Read data from the stdin pipe
 *  \param file int pipe handle (unused)
 *  \param ptr char* pointer to buffer to store data
 *  \param len int maximum number of characters to read
 *  \return int actual number of characters read, -1 if no data
 */
int _read(int file, char *ptr, int len)
{
    int charRead = 0;

#ifndef NO_EBMON_INIT
    if(!_eb_initialized)
    {
        _eb_initialized = 1;
        ebmonitor_init();
    }
#endif

    if(_eb_monitor_stdin.tail == _eb_monitor_stdin.head)
        return -1;

    for (; len > 0; --len)
    {
        if(++_eb_monitor_stdin.tail >= _eb_monitor_stdin.length)
            _eb_monitor_stdin.tail = 0;

        *ptr++ = ((char*)_eb_monitor_stdin.ptr)[_eb_monitor_stdin.tail];
        charRead++;

        if(_eb_monitor_stdin.tail == _eb_monitor_stdin.head)
            break;
    }

    return charRead;
}

/*@}*/ /* end of group EBmonitor */

