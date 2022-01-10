#include <stdio.h>
#include <stdlib.h>
#include <sys/types.h>
#include <unistd.h>
#include <string.h>

/********** PART 1 ************/ 

// 1. What are the numbers associated with the manual sections for executable programs, system calls, and library calls, respectively?
 
//      The numbers associated are 1, 2 and 3 respectively.
//
// 2. Which section number(s) contain an entry for open?
 
//      The section that contains an entry for open is 2. 

// 3. What three #include lines should be included to use the open() system call?
 
//      #include <sys/types.h>
//      #include <sys/stat.h>
//      #include <fcntl.h>

// 4. Which section number(s) contain an entry for socket?
 
//      The sections that contains an entry for socket are 2 and 7

// 5. Which socket option "Returns a value indicating whether or not this socket has been marked to accept connections with listen(2)"?
 
//      SO_ACCEPTCONN
 
// 6. How many sections contain keyword references to getaddrinfo?
 
//      Just one, section 3. 
 
// 7. According to the "DESCRIPTION" section of the man page for string, the functions described in that man page are used to perform operations on strings that are ________________. (fill in the blank)
 
//      Null-terminated strings
 
// 8. What is the return value of strcmp() if the value of its first argument (i.e., s1) is greater than the value of its second argument (i.e., s2)?

//      It returns an integer greater than 0.

/********** PART 2 ************/ 

// I completed the TMUX exercise from Part 2

/********** PART 3 ************/ 

int main(int argc, char *argv[]) {
    // Print the process ID to stderr followed by two new lines

    // int pid_t = getpid();

    fprintf(stderr, "%d\n\n", getpid());
    
    // Read the entire line into a buffer (i.e., "C string"; you will want to declare this as an array of char);

    char *fn = argv[1];
    FILE *fp = fopen(fn, "w");
    char *line = NULL;
    size_t len = 0; // base unsigned int
    ssize_t read;

    if (fp != NULL) {

        const char *pattern = getenv("CATMATCH_PATTERN");

        while ((read = getline(&line, &len, fp)) != -1) {
            // If CATMATCH_PATTERN exists as an environment variable, check the line for the pattern specified in the environment;
            char *p;
            
            p = strstr(line, pattern);

            if (p) {
                printf("%d ", 1);
            } else {
                printf("%d ", 0);
            }

            printf("%s", line);
        }
    
        fclose(fp);
        return 0;
    }

    return 0;

}
