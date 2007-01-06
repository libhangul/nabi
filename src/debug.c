#include <stdio.h>
#include <stdarg.h>
#include <string.h>

static FILE* output_device = NULL;
static int log_level = 0;

void
nabi_log_set_level(int level)
{
    log_level = level;
}

void
nabi_log_set_device(const char* device)
{
    if (strcmp(device, "stdout") == 0) {
	output_device = stdout;
    } else if (strcmp(device, "stderr") == 0) {
	output_device = stderr;
    }
}

void
nabi_log(int level, const char* format, ...)
{
    if (output_device == NULL)
	return;

    if (level <= log_level) {
	va_list ap;

	va_start(ap, format);
	vfprintf(output_device, format, ap);
	va_end(ap);
    }
}
