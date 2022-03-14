/*
Part 1 - 

1.
USER         PID    PPID NLWP     LWP S CMD
rljess    727344  719215    1  727344 S echoserveri

2. 

Processes: 1
Threads: 1

3. The nc process in the second window received the message back, while the third pane is still waiting

4. 
USER         PID    PPID NLWP     LWP S CMD
rljess    731944  724355    1  731944 S echoserverp
rljess    732029  731944    1  732029 S echoserverp
rljess    732045  731944    1  732045 S echoserverp
rljess    732063  731944    1  732063 S echoserverp

5.

Processes: 4
Threads: Each process has a thread id, which totals to 4 total threads running, one thread per process

6. 
USER         PID    PPID NLWP     LWP S CMD
rljess    733392  724355    4  733392 S echoservert
rljess    733392  724355    4  733429 S echoservert
rljess    733392  724355    4  733458 S echoservert
rljess    733392  724355    4  733495 S echoservert

7. 

Processes: 1
Threads: 4 in total

8.
USER         PID    PPID NLWP     LWP S CMD
rljess    736037  724355    9  736037 S echoservert_pre
rljess    736037  724355    9  736038 S echoservert_pre
rljess    736037  724355    9  736039 S echoservert_pre
rljess    736037  724355    9  736040 S echoservert_pre
rljess    736037  724355    9  736041 S echoservert_pre
rljess    736037  724355    9  736042 S echoservert_pre
rljess    736037  724355    9  736043 S echoservert_pre
rljess    736037  724355    9  736044 S echoservert_pre
rljess    736037  724355    9  736045 S echoservert_pre

9. 
Processes: 1
Threads: 9 total

10. There is 1 producer thread running

11. There are 8 consumer threads running

12. The producer thread is waiting on a client connection

13. The consumer thread is waiting for available items to fill the slots

14. It was waiting for the connection from a client to take place

15. Once the sem_post(&mutext) and sem_post(&items) is called, because it's announcing that there are items available in the queue

16. Just a single consumer changes state

(Repeat) 17. Once the sem_post(&mutext) and sem_post(&items) is called, because it's announcing that there are items available in the queue

18. Once the slot is available, it is waiting on the slot to be filled with items

? 19. 

Most Expensive -> Least Expensive

echoserverp -> echoservert -> echoservert_pre

20. echoservert_pre

21. echoservert and echoservert_pre use threads

22. echoserverp looks the easiest

*/

#include <stdio.h>
#include <string.h>
#include <stdlib.h>

#define MAXLINE 1000

int main(int argc, char *argv[])
{

    char *buf, *p;
    char arg1[MAXLINE], arg2[MAXLINE], content[MAXLINE];
    int n1 = 0, n2 = 0;

    if ((buf = getenv("QUERY_STRING")) != NULL)
    {
        p = strchr(buf, '&');
        *p = '\0';
        strcpy(arg1, buf);
        strcpy(arg2, p + 1);
        n1 = atoi(arg1);
        n2 = atoi(arg2);
    }

    sprintf(content, "The query string is: %s\r\n", buf);

    printf("Content-type: text/plain\r\n");
    printf("Content-length: %d\r\n\r\n", (int)strlen(content));
    printf("%s\r\n", content);
    fflush(stdout);

    return 0;
}
