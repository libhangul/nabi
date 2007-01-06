#ifndef nabi_debug_h
#define nabi_debug_h

void nabi_log_set_level(int level);
void nabi_log_set_device(const char* device);
void nabi_log(int level, const char* format, ...);

#endif /* nabi_debug_h */
