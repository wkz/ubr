
Based on the tipc tool from iproute2, by Richard Alpe

Build and run-time requirement:

    libmnl

Cross compile using:

	CPPFLAGS=-I/home/joachim/Troglobit/TroglOS/staging/include/   \
	LDFLAGS=-L/home/joachim/Troglobit/TroglOS/staging/lib         \
	CC=x86_64-unknown-linux-gnu-gcc                               \
	make

