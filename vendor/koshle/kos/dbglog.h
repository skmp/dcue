#pragma once

/** \defgroup logging   Logging
    \brief              KOS's Logging API 
    \ingroup            debugging
*/

/** \brief   Kernel debugging printf.
    \ingroup logging    

    This function is similar to printf(), but filters its output through a log
    level check before being printed. This way, you can set the level of debug
    info you want to see (or want your users to see).

    \param  level           The level of importance of this message.
    \param  fmt             Message format string.
    \param  ...             Format arguments
    \see    dbglog_levels
*/
#define dbglog(level, fmt, ...) fprintf(stdout, fmt, ## __VA_ARGS__)

/** \defgroup   dbglog_levels   Log Levels
    \brief                      dbglog severity levels
    \ingroup                    logging

    This is the list of levels that are allowed to be passed into the dbglog()
    function, representing different levels of importance.

    @{
*/
#define DBG_DEAD        0       /**< \brief The system is dead */
#define DBG_CRITICAL    1       /**< \brief A critical error message */
#define DBG_ERROR       2       /**< \brief A normal error message */
#define DBG_WARNING     3       /**< \brief Potential problem */
#define DBG_NOTICE      4       /**< \brief Normal but significant */
#define DBG_INFO        5       /**< \brief Informational messages */
#define DBG_DEBUG       6       /**< \brief User debug messages */
#define DBG_KDEBUG      7       /**< \brief Kernel debug messages */
/** @} */