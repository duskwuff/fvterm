#import <signal.h>

int main(int argc, char *argv[])
{
    // We don't care about our subprocesses exiting.
    signal(SIGCHLD, SIG_IGN);

    return NSApplicationMain(argc, (const char **)argv);
}
