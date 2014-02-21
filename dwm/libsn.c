#include <execinfo.h>
static void
print_backtrace (void)
{
	void *bt[500];
	int bt_size;
	int i;
	char **syms;

	bt_size = backtrace (bt, 500);

	syms = backtrace_symbols (bt, bt_size);

	i = 0;
	while (i < bt_size)
	{
		fprintf (stderr, "  %s\n", syms[i]);
		++i;
	}

	free (syms);
}
static int error_trap_depth = 0;

static void
error_trap_push (SnDisplay *display,
                 Display   *xdisplay)
{
	++error_trap_depth;
}

static void
error_trap_pop (SnDisplay *display,
                Display   *xdisplay)
{
	if (error_trap_depth == 0)
	{
		fprintf (stderr, "Error trap underflow!\n");
		exit (1);
	}

	XSync (xdisplay, False); /* get all errors out of the queue */
	--error_trap_depth;
}
