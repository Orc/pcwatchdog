/*
 * watchdog for the berkshire products usb pc watchdog.
 */
#include <stdio.h>
#include <fcntl.h>
#include <unistd.h>
#include <string.h>



/*
 * send a packet to the watchdog, return a pointer to it if it
 * got there okay.
 */
unsigned char *
command(int f, unsigned char cmd, unsigned char *pkt, int size)
{
    int status;

    pkt[0] = cmd;

    if ( (status=write(f, pkt, size)) == -1 ) {
	perror("query (write)");
	exit(1);
    }
    if ( (status = read(f, pkt, size)) == -1 ) {
	perror("query (read)");
	exit(1);
    }
    return ( (status == size) && (pkt[0] == cmd) ) ? pkt : 0;
}


/*
 * send a query to the watchdog, return a pointer to the response
 * packet.
 */
unsigned char *
query(int f, unsigned char cmd)
{
    static unsigned char pkt[6];

    bzero(pkt, sizeof pkt);

    return command(f, cmd, pkt, sizeof pkt);
}


/*
 * get the watchdog status, display it in text
 */
void
status(int f)
{
    unsigned char *pkt = query(f, 0x04);

    if (pkt) {
	/* cmd[2] has status bits */
	printf("%s\n", (pkt[2] & 0x80) ? "Armed" : "booting");
	printf("CMOS is %s\n", (pkt[2] & 01) ? "good" : "bad");
	printf("temp sensor is %s\n", (pkt[2] & 02) ? "good" : "bad");
    }
}


/*
 * show the status of the hardware switches
 */
void
switches(int f)
{
    unsigned char *pkt = query(f, 0x0c);
    int secs, i;

    static char *what[] = { 0,
			    "passive", "active", "disable buzzer",
			    "temperature reset", "power on delay",
			    "delay time(0)", "delay time(1)", "delay time(2)" };
    static int delays[] = { 5, 10, 30, 60, 300, 600, 1800, 3600 };


    if (pkt) {
	/* pkt[2] has dip switch settings */
	for (i=1; i <= 5; i++)
	    printf("SW%d (%s): %s\n", i, what[i], (pkt[2] & (1<<(8-i))) ? "ON" : "OFF");

	printf("Delay Time: ");
	secs = delays[pkt[2] & 0x7];
	if (secs < 60)
	    printf("%d seconds", secs);
	else if (secs == 60)
	    printf("1 minute");
	else if (secs < 3600)
	    printf("%d minutes", (secs/60));
	else if (secs == 3600)
	    printf("1 hour");
	else
	    printf("%d hours", (secs/3600));
	printf(" (SW6=%s,SW7=%s,SW8=%s)\n", (pkt[2] & 0x04) ? "ON" : "OFF",
					    (pkt[2] & 0x02) ? "ON" : "OFF",
					    (pkt[2] & 0x01) ? "ON" : "OFF" );

    }
}


/*
 * ping the watchdog
 */
void
ping(int f)
{
    query(f, 0x02);
}


/*
 * ask the watchdog about the temperature (and reset the clock)
 */
void
temp(int f)
{
    signed char *pkt = query(f, 0x02);

    if (pkt)
	printf("Current temp: %dC\n", pkt[2]);
}


/*
 * display the BIOS version#
 */
void
firmware(int f)
{
    unsigned char *pkt = query(f, 0x08);

    if (pkt)
	printf("Version %d.%d\n", pkt[1], pkt[2]);
}


/*
 * display the delay before the watchdog arms itself
 */
void
getArmtime(int f)
{
    unsigned char *pkt = query(f, 0x10);
    int time;

    if (pkt) {
	time = (pkt[1] << 8) | pkt[2];
	printf("Current Arm Time: %d\n", time);
    }
}


/*
 * set the arming delay (see getArmtime)
 */
void
setArmtime(int f, char *arg)
{
    unsigned int time;
    unsigned char pkt[6];
    unsigned char *ret;

    bzero(pkt, sizeof pkt);

    if (strcmp(arg, "?") == 0) {
	printf("usage: pcwatchdog armtime=NN (0 to 65535 seconds)\n");
	return;
    }

    time = atoi(arg);

    if (time > 0xFFFF)
	fprintf(stderr, "cannot set Arm Time to %s\n", arg);
    else {
	pkt[1] = (time >> 8) & 0xff;
	pkt[2] = time & 0xff;

	if (ret = command(f, 0x11, pkt, sizeof pkt))
	    printf("Set Arm Time: status=%d\n", pkt[2]);
    }
}


/*
 * display the stored arming delay
 */
void
getStoredArmtime(int f)
{
    unsigned char *pkt = query(f, 0x14);
    int time;

    if (pkt) {
	time = (pkt[1] << 8) | pkt[2];
	printf("Stored Arm Time: %d\n", time);
    }
}


/*
 * set the stored arming delay
 */
void
setStoredArmtime(int f, char *arg)
{
    unsigned int time;
    unsigned char pkt[6];
    unsigned char *ret;

    bzero(pkt, sizeof pkt);

    if (strcmp(arg, "?") == 0) {
	printf("usage: pcwatchdog stored-armtime=NN (1 to 65535 seconds)\n"
	       "       pcwatchdog stored-armtime=0 (clear the stored arm time)\n");
	return;
    }

    time = atoi(arg);

    if (time > 0xFFFF)
	fprintf(stderr, "cannot store %s as the Arm Time\n", arg);
    else {
	pkt[1] = (time >> 8) & 0xff;
	pkt[2] = time & 0xff;

	if (ret = command(f, 0x15, pkt, sizeof pkt))
	    printf("Stored Arm Time: status=%d\n", pkt[2]);
    }
}


/*
 * get the time left on the watchdog timer
 */
void
getAlarm(int f)
{
    unsigned char *pkt = query(f, 0x18);
    unsigned int time;

    if (pkt) {
	time = (pkt[1] << 8) | pkt[2];
	printf("Current Watchdog Time: %d\n", time);
    }
}


/*
 * set the initial watchdog alarm
 */
void
setAlarm(int f, char *arg)
{
    unsigned int time;
    unsigned char pkt[6];

    bzero(pkt, sizeof pkt);

    if (strcmp(arg, "?") == 0) {
	printf("usage: pcwatchdog alarm=NN (1-65535 seconds)\n"
	       "       pcwatchdog alarm=0  (reset to default value)\n");
	exit(1);
    }
    time = atoi(arg);

    if (time > 0xFFFF)
	fprintf(stderr, "cannot set Watchdog Time to %s\n", arg);
    else {
	pkt[1] = (time >> 8) & 0xff;
	pkt[2] = time & 0xff;

	if ( command(f, 0x19, pkt, sizeof pkt) == 0 ) {
	    fprintf(stderr, "Cannot set Watchdog Time\n");
	    exit(1);
	}
    }
}


/*
 * get the stored alarm time
 */
void
getStoredAlarm(int f)
{
    unsigned char *pkt = query(f,0x1C);
    unsigned int time;

    if (pkt) {
	time = (pkt[1]<<8) | pkt[2];
	printf("Stored Watchdog Time = %d\n", time);
    }
}


/*
 * set the CMOS watchdog timer
 */
void
setStoredAlarm(int f, char *arg)
{
    unsigned int time;
    unsigned char pkt[6];

    bzero(pkt, sizeof pkt);

    if (strcmp(arg, "?") == 0) {
	printf("usage: pcwatchdog stored-alarm=NN (1-65535 seconds)\n"
	       "       pcwatchdog stored-alarm=0  (reset to default value)\n");
	exit(1);
    }
    time = atoi(arg);

    if (time > 0xFFFF)
	fprintf(stderr, "cannot store %s as the Watchdog Time\n", arg);
    else {
	pkt[1] = (time >> 8) & 0xff;
	pkt[2] = time & 0xff;

	if ( command(f, 0x1D, pkt, sizeof pkt) == 0 ) {
	    fprintf(stderr, "Cannot store Watchdog Time\n");
	    exit(1);
	}
    }
}


/*
 * zero the re-trigger counter
 */
void
settrigger(int f, char *arg)
{
    unsigned char pkt[6];
    int zero, count;
    unsigned char *resp;


    bzero(pkt, sizeof pkt);
    if (arg) {
	if (strcmp(arg, "?") == 0 || atoi(arg) != 0) {
	    printf("usage: pcwatchdog trigger=0 (clear the re-trigger count)\n");
	    return;
	}
	pkt[2] = 1;
    }
    if ( resp = command(f, 0x20, pkt, sizeof pkt) ) {
	count = (resp[1] << 8) | resp[2];

	printf("%sRe-trigger Count: %d\n", arg ? "Old ":"", count);
    }

}


/*
 * get the current re-trigger count
 */
void
triggercount(int f)
{
    settrigger(f, 0);
}


/*
 * turn the watchdog on
 */
void
enablewatchdog(int f)
{
    char pkt[6];
    char *resp;

    bzero(pkt, sizeof pkt);

    pkt[1] = 0x5A;
    pkt[2] = 0x3C;

    if ( resp = command(f, 0x30, pkt, sizeof pkt) ) {
	if (resp[2])
	    printf("Watchdog is enabled\n");
    }
}


/*
 * turn the watchdog off
 */
void
disablewatchdog(int f)
{
    char pkt[6];
    char *resp;

    bzero(pkt, sizeof pkt);
    pkt[1] = 0xA5;
    pkt[2] = 0xC3;

    if ( resp = command(f, 0x30, pkt, sizeof pkt) ) {
	if (resp[2] == 0)
	    printf("Watchdog is disabled\n");
    }
}


/*
 * core function that displays the external relay settings
 */
void
describerelay(unsigned char *pkt)
{
    unsigned int flags = pkt[2];

    printf("Auxiliary relay is %s\n", (flags & 0x01) ? "ON" : "OFF");
    if (flags & 0x04)
	printf("Auxiliary relay will latch on at watchdog reset\n");
    else if (flags & 0x02)
	printf("Auxiliary relay will pulse at watchdog reset\n");
    if (flags & 0x80)
	printf("Non-volatile inversion setting is active\n");
}


/*
 * display the current state of the external relay
 */
void
getrelay(int f)
{
    unsigned char *pkt = query(f, 0x50);

    if (pkt)
	describerelay(pkt);
}


/*
 * turn the external relay on/off, set additional attributes
 */
void
setrelay(int f, char *flags)
{
    unsigned char pkt[6];
    char *s;
    unsigned char *resp;

    if ( strcmp(flags, "?") == 0 ) {
	printf("usage: pcwatchdog relay=flag{,flag(s)}\n"
	       "  flags are:  ON        -- close the relay\n"
	       "              OFF       -- open the relay\n"
	       "              PULSE     -- pulse the relay on reset\n"
	       "              LATCH     -- close the relay after reset\n"
	       "              INVERSION -- close the relay at power-up\n");
	return;
    }

    bzero(pkt, sizeof pkt);

    for (s = strtok(flags, ","); s; s = strtok(0, ","))
	if (strcasecmp(s, "off") == 0)
	    pkt[2] &= ~0x01;
	else if (strcasecmp(s, "on") == 0)
	    pkt[2] |= 0x01;
	else if (strcasecmp(s, "pulse") == 0)
	    pkt[2] |= 0x02;
	else if (strcasecmp(s, "latch") == 0)
	    pkt[2] |= 0x04;
	else if (strcasecmp(s, "inversion") == 0)
	    pkt[2] |= 0x80;

    if ( resp = command(f, 0x51, pkt, sizeof pkt) )
	describerelay(resp);
}


void
describebuzzer(unsigned char *pkt)
{
    int in_cmos = (pkt[2] & 0x80);

    if (pkt[2] & 0x01)
	printf("2.5 second beep after reset%s\n",
		in_cmos ? " (stored)" : "");
    if (pkt[2] & 0x02)
	printf("super-annoying continual beep after reset%s\n",
		in_cmos ? " (stored)" : "");
    if (pkt[2] & 0x04)
	puts("the buzzer is turned on");
    if (pkt[2] & 0x08)
	puts("the buzzer is turned off");
}


void
getbuzzer(int f)
{
    char *pkt = query(f, 0x5C);

    if (pkt)
	describebuzzer(pkt);
}


void
setbuzzer(int f, char *arg)
{
    char *opt;
    int flags;
    unsigned char pkt[6];
    unsigned char *resp;

    flags = 0;

    if (strcmp(arg, "?") == 0) {
	printf("usage: pcwatchdog buzzer=flag{,flag(s)}\n"
	       "   flags are: ON       -- start the buzzer\n"
	       "              OFF      -- stop the buzzer\n"
	       "              SHORT    -- sound the buzzer for 2.5 seconds on reset\n"
	       "              ANNOYING -- turn the buzzer on forever on reset\n"
	       "              STORE    -- store buzzer state in CMOS\n");
	return;
    }
    for (opt = strtok(arg, ","); opt; opt = strtok(0, ","))
	if (strcasecmp(opt, "on") == 0)
	    flags |= 0x04;
	else if (strcasecmp(opt, "off") == 0)
	    flags |= 0x08;
	else if (strcasecmp(opt, "store") == 0)
	    flags |= 0x80;
	else if (strcasecmp(opt, "short") == 0)
	    flags |= 0x01;
	else if (strcasecmp(opt, "annoying") == 0)
	    flags |= 0x02;

    bzero(pkt, sizeof pkt);

    pkt[2] = flags;


    if ( resp = command(f, 0x5D, pkt, sizeof pkt) )
	describebuzzer(pkt);
}


void
getresetcount(int f)
{
    unsigned char *pkt = query(f, 0x84);

    if (pkt)
	printf("Reset Count: %d\n", pkt[2]);
}


void
clearresetcount(int f, char *opt)
{
    unsigned char pkt[6];
    unsigned char *resp;

    bzero(pkt, sizeof pkt);

    if (strcmp(opt, "?") == 0 || atoi(opt) != 0) {
	printf("usage: pcwatchdog trigger=0 (clear the re-trigger count)\n");
	return;
    }

    pkt[2] = 1;

    if ( resp = command(f, 0x84, pkt, sizeof pkt) )
	printf("Old Reset Count: %d\n", pkt[2]);
}


void
getpulse(int f)
{
    unsigned char *pkt = query(f, 0x88);

    if (pkt == 0)
	return;

    if (pkt[2] == 0)
	printf("Pulse Width: 3 seconds (default)\n");
    else if (pkt[2] < 20)
	printf("Pulse Width: %d (%d ms)\n", pkt[2], 50*pkt[2]);
    else if (pkt[2] == 20)
	printf("Pulse Width: %d (1 second)\n", pkt[2]);
    else
	printf("Pulse Width: %d (%2.2f seconds)\n",
		pkt[2], ((float)pkt[2])/20);
}


void
setpulse(int f, char *opt)
{
    unsigned char pkt[6];
    unsigned char *resp;
    char *rest;
    int time;

    if (strcmp(opt, "?") == 0) {
	printf("usage: pcwatchdog pulse=NN (set reset time to NN 50ms ticks)\n"
	       "usage: pcwatchdog pulse=NNms (set reset time to NN ms)\n"
	       "usage: pcwatchdog pulse=NNseconds (set reset time to NN secs)\n");
	return;
    }
    time = strtol(opt, &rest, 10);

    if (rest == opt || time < 0)
	return;

    if (strcasecmp(rest, "ms") == 0)
	time /= 50;
    else if (strcasecmp(rest, "second") == 0
	  || strcasecmp(rest, "seconds") == 0)
	time *= 20;
    else if (*rest)
	return;

    if (time > 255)
	return;

    pkt[2] = (unsigned char) time;
    command(f, 0x89, pkt, sizeof pkt);
}


void
bigredswitch(int f)
{
    unsigned char pkt[6];
    unsigned char *resp;
    int i;

    if (!isatty(0)) {
	fprintf(stderr, "Can't reset from a batch file.  Sorry\n");
	exit(1);
    }
    for (i=30; i > 0; --i) {
	printf("Hi! I'm a %d second bomb! \r", i);
	fflush(stdout);
	sleep(1);
    }


    bzero(pkt, sizeof pkt);
    pkt[1] = 0xA5;
    pkt[2] = 1;


    if ( (resp = command(f, 0x80, pkt, sizeof pkt)) && (pkt[2] == 0) )
	puts("hey!   I'm not dead?");
}



struct cmdtab {
    char *action;
    void (*f)(int);
    void (*set)(int,char*);
    char *describe;
} cmds[] = {
    { "ping",     ping,            0,
	    "ping the watchdog card, resetting the alarm clock" },
    { "temp",     temp,            0,
	    "read the pcwatchdog thermometer" },
    { "status",   status,          0,
	    "read the current status of the pcwatchdog" },
    { "switches", switches,        0,
	    "show the current settings on the dip switch bank" },
    { "version",  firmware,        0,
	    "show the firmware version#" },
    { "arm-time",  getArmtime,      setArmtime,
	    "show (=set) the startup arming delay" },
    { "stored-arm-time",  getStoredArmtime,      setStoredArmtime,
	    "show (=set) the stored startup arming delay" },
    { "alarm",    getAlarm,        setAlarm,
	    "show the remaining alarm time (=set the default alarm time)" },
    { "stored-alarm",    getStoredAlarm,        setStoredAlarm,
	    "show (=set) the stored alarm time" },
    { "trigger",  triggercount,    settrigger, 
	    "show (=0; clear) how many times the watchdog was been pinged" },
    { "enable",   enablewatchdog, 0,
	    "turn the watchdog on" },
    { "disable",  disablewatchdog, 0,
	    "turn the watchdog off" },
    { "relay",    getrelay,        setrelay,
	    "show (=set) external relay parameters" },
    { "buzzer",   getbuzzer,       setbuzzer,
	    "show (=set) buzzer parameters" },
    { "count",    getresetcount,   clearresetcount,
	    "show (=0; clear) how many times the computer has been reset" },
    { "pulse",    getpulse,        setpulse,
	    "show (=configure) how long a reset lasts" },
    { "reset",    bigredswitch,    0,
	    "press the Big Red Switch" },
    { 0 },
};


printhelp()
{
    int i;

    printf("usage: pcwatchdog [-f device] [-h]\n"
	   "       pcwatchdog [-f device] command {...}\n"
	   "\n"
	   "If not specified, pcwatchdog uses the device /dev/uhid0\n"
	   "\n"
	   " the commands are:\n");

    for (i=0; cmds[i].action; i++)
	printf("%-15s-- %s.\n", cmds[i].action, cmds[i].describe);

    printf("\n"
	   "the command ``pcwatchdog command=?'' returns a description of\n"
	   "the valid arguments for the command\n");
    exit(0);
}

main(int argc, char **argv)
{
    char *device = "/dev/uhid0";
    int f;
    int opt;

    while ( (opt=getopt(argc, argv, "f:?h")) != EOF ) {
	switch (opt) {
	case 'f':   device = optarg;
		    break;
	case '?':
	case 'h':   printhelp();
	default :   fprintf(stderr, "usage: watch [-f port] [command {...}]\n");
		    exit(1);
	}
    }

    if ( (f = open(device, O_RDWR)) == -1 ) {
	perror(device);
	exit(1);
    }

    if (optind == argc)
	status(f);
    else if (strcmp(argv[optind], "help") == 0 || strcmp(argv[optind], "?") == 0)
	printhelp();
    else while (optind < argc) {
	int i;
	int len;
	char *arg;

	if ( arg = strchr(argv[optind], '=') )
	    *arg++ = 0;

	for (i=0; cmds[i].action; i++)
	    if (strcasecmp(argv[optind], cmds[i].action) == 0) {
		if (arg) {
		    if (cmds[i].set)
			(*cmds[i].set)(f, arg);
		    else
			fprintf(stderr, "cannot set %s\n", cmds[i].action);
		} 
		else
		    (*cmds[i].f)(f);
		break;
	    }
	++optind;
    }
    close(f);
}
