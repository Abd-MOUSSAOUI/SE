#include <unistd.h>
#include <stdlib.h>
#include <stdio.h>
#include <string.h>
#include <stdnoreturn.h>
#include <sys/wait.h>
#include <errno.h>
#include <signal.h>



#define CODE_ERREUR 1
char *prog;
typedef int Pipe[2];

noreturn void usage (void)
{
    fprintf (stderr, "usage: %s [n] (pair et non div par 4)\n", prog);
    exit (CODE_ERREUR);
}
noreturn void erreur(char *c)
{
    perror(c);
    exit(CODE_ERREUR);
}


int signall=0;
void fct_handler(int sig)
{
    (void)sig;
    signall+=1;
}


// Print the magic square
void print_MagicS(int a, int *matrix)
{
    int i=0,j;
    while(i<(a*a))
    {
        for (j=0; j<a; j++)
        {
            printf("%d ", matrix[i]);
            i++;
        }
        printf("\n");
    }
}

// A function to generate odd sized magic squares
void generateSquare(int a)
{
    int n= a/2;     //Divide the matrix by 4 matrix
    int x= n*n;     //Number of values for each matrix

    int ind, num;
    //Create a single pipe
    Pipe input;
    if (pipe(input) != 0) erreur("pipe a echoue");

    pid_t *pid = malloc(4*sizeof(size_t));
    int *matrix = malloc((a*a)*(sizeof(int)));

    for (ind=1; ind<=4; ind++)
    {
        // Initialize position for 1
        int i = 0;      int j = (n/2);
        int *child_buf=malloc(x*sizeof(int));
        // Set all slots as 0
        for (i=0; i<x; i++)
            {
                child_buf[i]=0;
            }
        i=0;
        pid_t p;

        // calcul a and b to do "from a to b"
        int a= ind*x-(x-1);     int b= ind*x;
        switch (p = fork())
        {
            case -1:
                erreur("fork a echoue");
            break;

            case 0:
                if(close(input[0]) == -1) erreur("close a echoue");
                // build the  small matrix
                for (num = a ; num <= b;)
                {
                    if (i==-1 && j==n)
                    {
                        j = 0; i = n-1;
                    }
                    else
                    {
                        if (i < 0)  i = n-1; //1st condition
                        if (j == n) j = 0; //2nd condition
                    }

                    if (child_buf[(i*n)+j]) //3rd condition
                    {
                        if(j==0) j=(n-1);
                        else j--;
                        if (i==(n-2))   i=0;
                        if (i==(n-1))   i=1;
                        else i+=2;
                        continue;
                    }
                    else
                    {
                        //set number at position
                        child_buf[(i*n)+j]=num++;
                    }
                    j++; i--;
                }
                //Suspend the child and wait for signal
                sigset_t myset, oldset;
                sigemptyset(&myset);
                sigemptyset(&oldset);
                sigaddset(&myset, SIGCONT);
                if (sigprocmask(SIG_BLOCK , &myset , &oldset) == -1)
                    erreur("masquage a echoue");
                
                while(signall == 0)     sigsuspend(&oldset);
                if (sigprocmask(SIG_UNBLOCK, &myset ,NULL) == -1)
                    erreur( "unblock a echoue");
                //Write on pipe
                write(input[1], child_buf, (x*4));
                exit(0);
            default:
                pid[ind-1]=p;
            break;
        }
    free(child_buf);
    }
    int code = 0;
    if (close(input[1]) == -1) erreur("close a echoue");
    for (ind = 1; ind <= 4; ind++)
    {
        //Read from childs one by one
        int *father_buf = malloc(x*sizeof(int));
        //Send signal to child
        if (kill(pid[ind-1], SIGCONT) == -1) erreur("kill a echoue");
        read(input[0], father_buf, (x*4));
        //wait child for finish
        int raison;
        if(wait(&raison) == -1) erreur("wait a echoue");
        if(WIFEXITED(raison)) code += WEXITSTATUS(raison);

        //build the big matrix with changes
        int i,j,count=0;
        switch(ind)
        {
            case 1:
                //bild the top left with changes
                while(count<x)
                {
                    for (i=0; i<n; i++)
                    {
                        for(j=0; j<n; j++)
                        {
                            if (((i != n/2) && (j < n/2)) ||
                            ((i == n/2) && (j>0) && (j <= n/2)))
                            {
                                matrix[(i*a)+(2*x)+j]=father_buf[count];
                                count++;
                            }
                            else
                            {
                                matrix[(i*a)+j]=father_buf[count];
                                count++;
                            }
                        }
                    }
                }
                free(father_buf);
            break;

            case 2:
                //bild the bottom right with changes
                while(count<x)
                {
                    for (i=n; i<a; i++)
                    {
                        for(j=n; j<a; j++)
                        {
                            if( ((n/2)-1 > 0) && (j > (a-(n/2))) )
                            {
                                matrix[(i*a)+j-(2*x)]=father_buf[count];
                                count++;
                            }
                            else
                            {
                                matrix[(i*a)+j]=father_buf[count];
                                count++;
                            }
                        }
                    }
                }
                free(father_buf);
            break;

            case 3:
                //bild the top right with changes
                while(count<x)
                {
                    for (i=0; i<n; i++)
                    {
                        for(j=n; j<a; j++)
                        {
                            if( ((n/2)-1 > 0) && (j > (a-(n/2))) )
                            {
                                matrix[(i*a)+j+(2*x)]=father_buf[count];
                                count++;
                            }
                            else
                            {
                                matrix[(i*a)+j]=father_buf[count];
                                count++;
                            }
                        }
                    }
                }
                free(father_buf);
            break;

            case 4:
                //bild the bottom left with changes
                while(count<x)
                {
                    for (i=n; i<a; i++)
                    {
                        for(j=0; j<n; j++)
                        {
                            if( ((i != n+(n/2)) && (j < n/2)) ||
                            ((i == n+(n/2)) && (j > 0) && (j <= n/2)) )
                            {
                                matrix[(i*a)+j-(2*x)]=father_buf[count];
                                count++;
                            }
                            else
                            {
                                matrix[(i*a)+j]=father_buf[count];
                                count++;
                            }
                        }
                    }
                }
                free(father_buf);
            break;

            default:
            break;
        }
    }
    if (close(input[0]) == -1 ) erreur("close a echoue");
    print_MagicS(a, matrix);
    free(matrix);
    free(pid);
    exit(code);
}


int main (int argc, char *argv[])
{
    prog = argv[0];
    if (argc != 2) exit(CODE_ERREUR);

    int n = atoi(argv[1]);
    if ((n<0) || (n%4 == 0) || (n%2 != 0)) usage();
    
    //Create a sig struct and set the signal
    struct sigaction s;
    s.sa_handler = &fct_handler;
    s.sa_flags=0;
    sigemptyset(&s.sa_mask);
    sigaction(SIGCONT, &s, NULL);

    generateSquare (n);
    return 0;
}
