#include <unistd.h>
#include <stdlib.h>
#include <fcntl.h>
#include <stdarg.h>
#include <stdio.h>			// pour les msg d'erreur uniquement
#include <stdnoreturn.h>		// C norme 2011

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
    fprintf (stderr, "usage: %s [-n][-E] [fichier1 ... fichiern]\n", prog) ;
    exit (1) ;
}

/******************************************************************************
 * Lecture bufferisée des fichiers
 *
 * On utilise un seul buffer car on n'ouvre qu'un seul fichier à la fois.
 */

#define	TAILLE_BUFFER	1024

#define	MON_EOF	-34			// et pourquoi pas ?

struct buffer
{
    unsigned char stock [TAILLE_BUFFER] ;
    int pos ;				// position dans le buffer
    int reste ;				// reste à lire dans le buffer
} ;

struct buffer buffer ;

void init_buffer (void)
{
    buffer.reste = 0 ;			// vider le buffer
}

int mon_getchar (int fd)
{
    if (buffer.reste <= 0)		// peut être -1 si erreur précédente
    {
	buffer.reste = read (fd, buffer.stock, sizeof buffer.stock) ;
	if (buffer.reste == -1)
	    raler (1, "cannot read file") ;
	buffer.pos = 0 ;
    }
    return buffer.reste-- <= 0 ? MON_EOF : buffer.stock [buffer.pos++] ;
}

/******************************************************************************
 * moncat
 *
 * Le vrai "cat -n" numérote les lignes des différents fichiers de manière
 * continue (i.e. pas de remise à 0 du numéro de ligne à chaque fichier).
 */

void moncat (int fd, int optn, int optE)
{
    static int numligne = 0 ;
    int debutligne ;
    int c ;

    init_buffer () ;
    debutligne = 1 ;
    while ((c = mon_getchar (fd)) != MON_EOF)
    {
	if (debutligne && optn)
	{
	    printf ("%6d\t", ++numligne) ;
	    debutligne = 0 ;
	}

	if (c == '\n')
	{
	    debutligne = 1 ;
	    if (optE)
		putchar ('$') ;
	}
	putchar (c) ;
    }
}

/******************************************************************************
 * Main
 */

int main (int argc, char *argv [])
{
    int optn, optE ;
    int opt ;
    int i ;

    prog = argv [0] ;

    optn = optE = 0 ;
    while ((opt = getopt (argc, argv, "nE")) != -1)
    {
	switch (opt)
	{
	    case 'n' :
		optn = 1 ;
		break ;
	    case 'E' :
		optE = 1 ;
		break ;
	    default :
		usage () ;
	}
    }

    if (optind >= argc)			// pas d'argument
    {
	moncat (0, optn, optE) ;
    }
    else				// fichier1 ... fichiern
    {
	for (i = optind ; i < argc ; i++)
	{
	    int fd ;

	    fd = open (argv [i], O_RDONLY) ;
	    if (fd == -1)
		raler (1, "cannot open %s", argv [i]) ;
	    moncat (fd, optn, optE) ;
	    if (close (fd) == -1)
		raler (1, "cannot close %s", argv [i]) ;
	}
    }

    exit (0) ;				// si on arrive là, tout est ok
}
