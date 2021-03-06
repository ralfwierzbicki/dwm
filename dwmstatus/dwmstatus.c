#define _BSD_SOURCE
#include <unistd.h>
#include <stdio.h>
#include <stdlib.h>
#include <stdarg.h>
#include <string.h>
#include <strings.h>
#include <math.h>
#include <sys/time.h>
#include <time.h>
#include <sys/types.h>
#include <sys/wait.h>
#include <locale.h>
#include <libxml/xmlreader.h>
#include <X11/Xlib.h>

char *weather();
char *tz = "America/Toronto";

static Display *dpy;

/**
 * processNode:
 * @reader: the xmlReader
 *
 * Dump information about the current node
 */
static void
processNode(xmlTextReaderPtr reader)
{
	const xmlChar *name, *value;

	name = xmlTextReaderConstName(reader);
	if (name == NULL)
		name = BAD_CAST "--";

	value = xmlTextReaderConstValue(reader);

	printf("depth: %d type: %d name: %s empty: %d value: %d",
	       xmlTextReaderDepth(reader),
	       xmlTextReaderNodeType(reader),
	       name,
	       xmlTextReaderIsEmptyElement(reader),
	       xmlTextReaderHasValue(reader));
	if (value == NULL)
		printf("\n");
	else
	{
		if (xmlStrlen(value) > 40)
			printf(" %.40s...\n", value);
		else
			printf(" %s\n", value);
	}
}

/**
 * streamFile:
 * @filename: the file name to parse
 *
 * Parse and print information about an XML file.
 */
static void
streamFile(const char *filename)
{
	xmlTextReaderPtr reader;
	int ret;

	reader = xmlReaderForFile(filename, NULL, 0);
	if (reader != NULL)
	{
		ret = xmlTextReaderRead(reader);
		while (ret == 1)
		{
			processNode(reader);
			ret = xmlTextReaderRead(reader);
		}
		xmlFreeTextReader(reader);
		if (ret != 0)
		{
			fprintf(stderr, "%s : failed to parse\n", filename);
		}
	}
	else
	{
		fprintf(stderr, "Unable to open %s\n", filename);
	}
}

char *
rx_rate (const char *iface, int cap)
{
	FILE *fp = NULL;
	char buf[2048];
	char *priface;
	unsigned long int bytes = 0;
	static unsigned long int oldbytes = 0;
	float rate = 0;
	char *ret;
	const char block[] = "\342\226\210";
	const char arrdown[] = "\342\206\223";
	int i = 0;
	char *bar;
	const char markup[] = "kb/s %4.0f Û [<span font=\"DejaVu Sans Mono 4\" color=\"#ff0000\">%s</span>]Ú";

	bar = (char *) malloc (100);
	memset (bar, 0, 100);

	fp = fopen ("/proc/net/dev", "r");
	while (fgets (buf, 2047, fp))
	{
		if (!strstr (buf, iface))
			continue;
		priface = index (buf, ':') + 1;
		sscanf (priface, "%ul ", &bytes);
		break;
	}
	fclose (fp);
	rate = (bytes - oldbytes) * 8 / 10000;
	if (oldbytes == 0)
		rate = 0;
	rate = rate / 100;
	if (rate > cap)
	{
		rate = 10;
	}
	oldbytes = bytes;

	for (i = 0; i < (int) round (rate); i++)
	{
		strcat (bar, block);
	}
	for (i = 0; i < 10 - (int) round (rate); i ++)
	{
		strcat (bar, " ");
	}
	ret = (char *) malloc (strlen (markup) + strlen (bar) + 20);
	memset (ret, 0, strlen (markup) + strlen (bar) + 1);
	sprintf (ret, markup, rate * 100, bar);
	free (bar);
	return ret;
}

char *
cpu_usage ()
{
	FILE *fp = NULL;
	int user, system, nice, idle, wait, irq, srq, zero, total;
	static int prev_idle = 0;
	static int prev_total = 0;
	float usage = 0.00;
	char *ret;
	const char block[] = "\342\226\210";
	int i;

	ret = (char *) malloc (100);
	memset (ret, 0, 100);

	fp = fopen ("/proc/stat", "r");
	fscanf(fp, "cpu %d %d %d %d %d %d %d %d", &user, &system, &nice, &idle, &wait, &irq, &srq, &zero);

	total = user + system + nice + idle + wait + irq + srq + zero;
	usage = (1000 * (((float) total - (float) prev_total) - ((float) idle - (float) prev_idle)) / (((float)total - (float) prev_total) + 5) / 100);

	prev_total = total;
	prev_idle = idle;
	for (i = 0; i < (int) round (usage); i++)
	{
		strcat (ret, block);
	}
	for (i = 0; i < 10 - (int) round (usage); i ++)
	{
		strcat (ret, " ");
	}
	fclose (fp);
	return ret;
}

char *
core_temp () {
	FILE *fp = NULL;
	int temp2;
	int temp3;
	int temp4;
	int temp5;
	char *ret;
	const char red[] = "red";
	const char yellow[] = "yellow";
	const char green[] = "green";
	const char markup[] = "<span color=\"%s\">%dC</span> <span color=\"%s\">%dC</span> <span color=\"%s\">%dC</span> <span color=\"%s\">%dC</span>";
	char *temp2_color;
        char *temp3_color;
        char *temp4_color;
        char *temp5_color;

	
	ret = (char *) malloc (21 * sizeof (char) + strlen (markup) * sizeof (char));
	fp = fopen ("/sys/bus/platform/drivers/coretemp/coretemp.0/hwmon/hwmon2/temp2_input", "r");
	fscanf (fp, "%d", &temp2);
	temp2 = temp2 / 1000;
	if (temp2 < 40) {
		temp2_color = green;
	} else if (temp2 >= 40 && temp2 < 50) {
		temp2_color = yellow;
	} else {
		temp2_color = red;
	}
	fclose (fp);

        fp = fopen ("/sys/bus/platform/drivers/coretemp/coretemp.0/hwmon/hwmon2/temp3_input", "r");
        fscanf (fp, "%d", &temp3);
        temp3 = temp3 / 1000;
        if (temp3 < 40) {
                temp3_color = green;
        } else if (temp3 >= 40 && temp3 < 50) {
                temp3_color = yellow;
        } else {
                temp3_color = red;
        }
	fclose (fp);

        fp = fopen ("/sys/bus/platform/drivers/coretemp/coretemp.0/hwmon/hwmon2/temp4_input", "r");
        fscanf (fp, "%d", &temp4);
        temp4 = temp4 / 1000;
        if (temp4 < 40) {
                temp4_color = green;
        } else if (temp4 >= 40 && temp4 < 50) {
                temp4_color = yellow;
        } else {
                temp4_color = red;
        }
	fclose (fp);

        fp = fopen ("/sys/bus/platform/drivers/coretemp/coretemp.0/hwmon/hwmon2/temp5_input", "r");
        fscanf (fp, "%d", &temp5);
        temp5 = temp5 / 1000;
        if (temp5 < 40) {
                temp5_color = green;
        } else if (temp5 >= 40 && temp5 < 50) {
                temp5_color = yellow;
        } else {
                temp5_color = red;
        }
	fclose (fp);
	sprintf (ret, markup, temp2_color,  temp2, temp3_color, temp3, temp4_color, temp4, temp5_color, temp5);
	return ret;
}

char *
smprintf(char *fmt, ...)
{
	va_list fmtargs;
	char *ret;
	int len;

	va_start(fmtargs, fmt);
	len = vsnprintf(NULL, 0, fmt, fmtargs);
	va_end(fmtargs);

	ret = (char *) malloc(++len);
	if (ret == NULL)
	{
		perror("malloc");
		exit(1);
	}

	va_start(fmtargs, fmt);
	vsnprintf(ret, len, fmt, fmtargs);
	va_end(fmtargs);

	return ret;
}

void
settz(char *tzname)
{
	setenv("TZ", tzname, 1);
}

char *
mktimes(char *fmt, char *tzname)
{
	char buf[129];
	time_t tim;
	struct tm *timtm;

	memset(buf, 0, sizeof(buf));
	settz(tzname);
	tim = time(NULL);
	timtm = localtime(&tim);
	if (timtm == NULL)
	{
		perror("localtime");
		exit(1);
	}

	if (!strftime(buf, sizeof(buf) - 1, fmt, timtm))
	{
		fprintf(stderr, "strftime == 0\n");
		exit(1);
	}

	return smprintf("%s", buf);
}

void
setstatus(char *str)
{
	XStoreName(dpy, DefaultRootWindow(dpy), str);
	XSync(dpy, False);
}

char *
loadavg(void)
{
	double avgs[3];

	if (getloadavg(avgs, 3) < 0)
	{
		perror("getloadavg");
		exit(1);
	}

	return smprintf("%.2f %.2f %.2f", avgs[0], avgs[1], avgs[2]);
}

int
main(void)
{
	char *status;
	char *avgs;
	char *tm;
	char *cpu;
	char *rx;
	char *temp2;

	setlocale(LC_ALL, "");

	if (!(dpy = XOpenDisplay(NULL)))
	{
		fprintf(stderr, "dwmstatus: cannot open display.\n");
		return 1;
	}

	/*
	     * this initialize the library and check potential ABI mismatches
	     * between the version it was compiled for and the actual shared
	     * library used.
	     */
	/* LIBXML_TEST_VERSION*/

	/*streamFile("http://weather.yahooapis.com/forecastrss?w=4125&u=c");*/

	/*
	 * Cleanup function for the XML library.
	 */
	/*xmlCleanupParser();*/
	/*
	 * this is to debug memory for regression tests
	 */
	/*xmlMemoryDump();*/

	for (;; sleep(1))
	{
		//avgs = loadavg();
		tm = mktimes("%a %d %b %H:%M %Y", tz);
		cpu = cpu_usage ();
		rx = rx_rate ("wlp7s2", 100);
		temp2 = core_temp ();


		status = smprintf("[%s] %s [<span font=\"DejaVu Sans Mono 4\" color=\"#8ebe27\">%s</span>] %s", temp2, rx, cpu, tm);
		setstatus(status);
		//free(avgs);
		free(cpu);
		free(rx);
		free(status);
		free(temp2);
	}

	XCloseDisplay(dpy);

	return 0;
}

