namespace system {

int64 write(int64 fd, char* buf, int64 len) {
	int64 ret;
	asm (
		"mov $$0x2000004, %rax\n"
		"mov  $1, %rdi    \n"
		"mov  $2, %rsi    \n"
		"mov  $3, %rdx    \n"
		"syscall\n"
		"mov %rax, $0\n"
		: "=m" (ret)
		: "m" (fd), "m" (buf), "m" (len)
		: "rax", "rdi", "rsi", "rdx"
	);
	return ret;
}

}