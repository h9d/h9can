#pragma once
/* Stub errno.h: does NOT define 'errno' as a macro.
   The system <errno.h> on macOS expands 'errno' to (*__error()), which would
   break the 'uint8_t errno' parameter in send_command_error().
   can.c includes <errno.h> but never reads the global errno value. */
#define EPERM   1
#define ENOENT  2
#define ENOMEM  12
