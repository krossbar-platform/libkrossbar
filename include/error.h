#pragma once

#include <mpack-common.h>

enum kb_message_error_e
{
    /**< No error. */
    KB_MESSAGE_OK = mpack_ok,
    /**< The reader or writer failed to fill or flush, or some other file or socket error occurred. */
    KB_MESSAGE_ERROR_IO = mpack_error_io,
    /**< The data read is not valid MessagePack. */
    KB_MESSAGE_ERROR_INVALID = mpack_error_invalid,
    /**< The data read is not supported by this configuration of MPack. (See @ref MPACK_EXTENSIONS.) */
    KB_MESSAGE_ERROR_UNSUPPORTED = mpack_error_unsupported,
    /**< The type or value range did not match what was expected by the caller. */
    KB_MESSAGE_ERROR_TYPE = mpack_error_type,
    /**< A read or write was bigger than the maximum size allowed for that operation. */
    KB_MESSAGE_ERROR_TOO_BIG = mpack_error_too_big,
    /**< An allocation failure occurred. */
    KB_MESSAGE_ERROR_MEMORY = mpack_error_memory,
    /**< The MPack API was used incorrectly. (This will always assert in debug mode.) */
    KB_MESSAGE_ERROR_BUG = mpack_error_bug,
    /**< The contained data is not valid. */
    KB_MESSAGE_ERROR_DATA = mpack_error_data,
    /**< The reader failed to read because of file or socket EOF */
    KB_MESSAGE_ERROR_EOF = mpack_error_eof,
};

typedef enum kb_message_error_e kb_message_error_t;
