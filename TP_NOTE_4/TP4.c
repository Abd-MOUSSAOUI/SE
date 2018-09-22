#include <unistd.h>
#include <stdlib.h>
#include <string.h>
#include <fcntl.h>
#include <stdarg.h>
#include <stdio.h>			// pour les msg d'erreur uniquement
#include <stdnoreturn.h>		// C norme 2011
#include <sys/types.h>
#include <sys/wait.h>
#include <sys/stat.h>
#include <errno.h>
#include <ctype.h>

#define	MAX_CHEMIN	512		// taille max des chemins (cf PIPE_BUF)

#define	TAILLE_BUFFER	4096		// pour la lecture de l'entrée std

/******************************************************************************
 * Gestion des erreurs
 */

char *prog ;				// nom du programme pour les erreurs

noreturn void raler (int syserr, const char *fmt, ...)
{
    va_list ap ;

    va_start (ap, fmt) ;
    fprintf (stderr, "%s: ", prog) ;
    vfprintf (stderr, fmt, ap) ;
    fprintf (stderr, "\n") ;
    va_end (ap) ;

    if (syserr)
	perror ("") ;

    exit (1) ;
}

noreturn void usage (void)
{
    fprintf (stderr, "usage: %s n (avec n > 0)\n", prog) ;
    exit (1) ;
}


/******************************************************************************
 * Code des fils
 */

int fils (int numero, int fd)
{
    int err ;
    char chemin [MAX_CHEMIN + 1] ;	// +1 pour le \0 non transmis
    ssize_t nlus ;

    err = 0 ;
    while ((nlus = read (fd, chemin, MAX_CHEMIN)) > 0)
    {
	struct stat stbuf ;

	chemin [nlus] = '\0' ;
	if (stat (chemin, &stbuf) == -1)
	{
	    fprintf (stderr, "child %d: cannot stat %s (%s)",
				numero, chemin, strerror (errno)) ;
	    err = 1 ;
	    continue ;
	}
	if (! S_ISREG (stbuf.st_mode))
	{
	    fprintf (stderr, "child %d: %s is not a regular file\n",
	    			numero, chemin) ;
	    err = 1 ;
	    continue ;
	}

	/*
	 * C'est bon, on peut faire l'action...
	 */

	usleep (stbuf.st_size * 1000) ;

	printf ("%d\t%s\t%zd\n", numero, chemin, stbuf.st_size) ;
    }

    return err ;
}

/******************************************************************************
 * Code du père
 */

int pere (int n, int fd)
{
    int r, i ;
    char buf [TAILLE_BUFFER] ;
    char morceau [MAX_CHEMIN + 1] ;	// \0 pour les msg d'erreur éventuels
    int fin ;				// fin du morceau en cours
    ssize_t nlus ;

    /*
     * Lire ce qui est sur l'entrée standard et envoyer dans le tube
     *
     * C'est là que réside la principale difficulté de l'exercice :
     * on ne peut pas se contenter de déverser directement ce qu'on
     * lit sur l'entrée standard dans le tube. À la place, il faut
     * écrire des chemins, débarrassés du \n final, dans des morceaux
     * de 512 octets exactement, tout en tenant compte du fait que
     * l'entrée standard peut elle même être un tube et provoquer
     * des lectures partielles... Cf script de test.
     */

    fin = 0 ;
    while ((nlus = read (0, buf, sizeof buf)) > 0)
    {
	morceau [fin] = '\0' ;		// si on doit afficher un msg d'erreur

	/*
	 * Accumuler les octets dans morceau []
	 */

	for (i = 0 ; i < nlus ; i++)
	{
	    if (buf [i] == '\n')	// trouvé un chemin !
	    {
		ssize_t necr ;

		necr = write (fd, morceau, MAX_CHEMIN) ;
		if (necr == -1)
		    raler (1, "cannot write %s", morceau) ;
		if (necr < MAX_CHEMIN)
		    raler (0, "wrote %d (< %d) for %s",
			    	necr, MAX_CHEMIN, morceau) ;

		fin = 0 ;
		morceau [fin] = '\0' ;	// si on doit afficher un msg d'erreur
	    }
	    else if (fin >= MAX_CHEMIN)
		raler (0, "path too long (%s)", morceau) ;
	    else
	    {
		morceau [fin++] = buf [i] ;
		morceau [fin] = '\0' ;	// si on doit afficher un msg d'erreur
	    }
	}
    }
    if (fin != 0)
	raler (0, "path %s not terminated by \\n", morceau) ;

    close (fd) ;

    /*
     * Attendre la fin des fils
     */

    r = 0 ;
    for (i = 1 ; i <= n ; i++)
    {
	int raison ;

	if (wait (&raison) == -1)
	    raler (1, "cannot wait child") ;

	if (! (WIFEXITED (raison) && WEXITSTATUS (raison) == 0))
	    r = 1 ;
    }

    return r ;
}

/******************************************************************************
 * Main
 */

int main (int argc, char *argv [])
{
    int tube [2] ;
    int r, i, n ;

    prog = argv [0] ;
    if (argc != 2)
	usage () ;

    n = atoi (argv [1]) ;
    if (n <= 0)
	usage () ;

    if (pipe (tube) == -1)
	raler (1, "cannot create pipe") ;

    for (i = 1 ; i <= n ; i++)
    {
	switch (fork ())
	{
	    case -1 :
		raler (1, "cannot fork child %d", i) ;

	    case 0 :
		if (close (tube [1]) == -1)
		    raler (1, "child %d: cannot close tube[1]", i) ;
		r = fils (i, tube [0]) ;
		exit (r) ;

	    default :
		/* rien */
		break ;
	}
    }

    if (close (tube [0]) == -1)
	raler (1, "father: cannot close tube[0]", i) ;

    r = pere (n, tube [1]) ;
    exit (r) ;
}