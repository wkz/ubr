#ifndef UBR_H_
#define UBR_H_

#define PACKAGE_VERSION "v0.1"	/* XXX: Remove when autotools is in place */

#define MIN(a,b) (((a) <= (b))? (a) : (b))
#define MAX(a,b) (((a) >= (b))? (a) : (b))

#define DBG(fmt, args...) if (debug)  printf("%s(): " fmt, __func__, ##args)

struct cmd {
	struct cmd *ctx;

	char *name;
	int   args;

	char *usage;
	char *help;

	int (*cb)(char *);
};

/* Global variables */
extern char *prognm;
extern int   debug;

/* Helper functions */
size_t strlcat(char *dst, const char *src, size_t siz);

#endif /* UBR_H_ */
