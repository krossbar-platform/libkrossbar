#pragma once

#include <log4c.h>

#ifdef NDEBUG
    #define log_trace ((void) (0))
#else
    #define log_trace(a_category, a_format, ...) log4c_category_log(a_category, LOG4C_PRIORITY_TRACE, a_format, ##__VA_ARGS__);
#endif
