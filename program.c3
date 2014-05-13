import system;

struct thing {
	int64 a;
	int64 b;
}

namespace foo {
	struct thing {
		int64 x;
	}
	
	int64 bar() {
		return 0;
	}
	
	thing lt;
	lt.x = 7;
}

foo::bar();

foo::thing fb;

fb.x = 3;

int64 sum(thing* this) {
	return (*this).a + this->b;
}

thing t;

t.a = 2;
t.b = 3;

int64 a = 50 / t.a;

if (a == 25) {
	system::write(1, "pass\n", 5);
}

char c = '0' + sum(&t);

system::write(1, &c, 1);
system::write(1, "\n", 1);

int64 written = system::write(1, "hello world\n", 12);

if (written == 1 + 3 * 2 + 5)
	system::write(1, "12\n", 3);

if (1 == 2) {
	system::write(1, "??\n", 3);
} else if (3 == 4 - 1) {
	system::write(1, "!!\n", 3);
} else {
	system::write(1, "*1*\n", 4);
	system::write(1, "*2*\n", 4);
}

int64 i = 0;
while (i < 5) {
	char x = '0' + i;
	system::write(1, &x, 1);
	system::write(1, "\n", 1);
	i = i + 1;
}
