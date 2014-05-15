namespace system {
	extern int64 write(int64 fd, const void* buf, uint64 len) : "write";

	extern void* calloc(uint64 count, uint64 size) : "calloc";
	extern void* malloc(uint64 size) : "malloc";
	extern void* realloc(void* ptr, uint64 size) : "realloc";
	extern void free(void* ptr) : "free";
}