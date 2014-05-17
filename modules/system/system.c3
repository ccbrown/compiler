namespace system {
	extern int64 write(int32 fd, const void* buf, uint64 len) : "write";

	extern void* calloc(uint64 count, uint64 size) : "calloc";
	extern void* malloc(uint64 size) : "malloc";
	extern void* realloc(void* ptr, uint64 size) : "realloc";
	extern void free(void* ptr) : "free";

	const int32 stdin  = 0;
	const int32 stdout = 1;
	const int32 stderr = 2;

	void print(const uint8* string) {
		extern uint64 _strlen(const uint8* string) : "strlen";
		write(stdout, string, _strlen(string));
		write(stdout, "\n", 1);
	}
}