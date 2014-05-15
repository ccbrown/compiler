namespace system {
	extern int64 write(int64 fd, void* buf, int64 len) : "write";

	extern void* calloc(int64 count, int64 size) : "calloc";
	extern void* malloc(int64 size) : "malloc";
	extern void* realloc(void* ptr, int64 size) : "realloc";
	extern void* free(void* ptr) : "free";
}