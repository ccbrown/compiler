import system;

namespace string {	
	const uint8* make(int64 n) {
		if (!n) { return "0"; }
		
		static uint8* ret = nullptr;
		if (!ret) { ret = static_cast<uint8*>(system::malloc(25)); }

		auto ptr = ret + 24;
		*ptr = '\0';
		ptr = ptr - 1;

		auto is_negative = n < 0;
		if (is_negative) { n = -n; }

		while (n > 0) {
			*ptr = '0' + n % 10;
			ptr = ptr - 1;
			n = n / 10;
		}
		
		if (is_negative) {
			*ptr = '-';
			ptr = ptr - 1;
		}

		return ptr + 1;
	}
}