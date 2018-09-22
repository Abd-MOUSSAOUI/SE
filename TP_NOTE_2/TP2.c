#include <unistd.h>
#include <stdlib.h>
#include <string.h>
#include <fcntl.h>
#include <stdarg.h>
#include <stdio.h>			// pour les msg d'erreur uniquement
#include <stdnoreturn.h>		// C norme 2011
#include <sys/types.h>
#include <sys/stat.h>
#include <dirent.h>
#include <limits.h>			// pour PATH_MAX
#include <inttypes.h>			// pour intmax_t


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
    fprintf (stderr, "usage: %s répertoire\n", prog) ;
    exit (1) ;
}


/******************************************************************************
 * Gestion de la structure de données pour mémoriser l'arborescence
 */

#define	INC_ARBO	100		// realloc par paquets de 100

struct fichier
{
    char *chemin ;
    off_t taille ;			// st_size retournée par stat => off_t
} ;

struct arbo				// tous les fichiers d'une arborescence
{
    struct fichier *tab ;
    int taille ;			// taille du tableau
    int pos ;				// position dans le tableau
} ;

/*
 * Initialisation de la structure de données
 */
void init_arborescence (struct arbo *a)
{
    a->tab = NULL ;
    a->taille = 0 ;
    a->pos = 0 ;
}

/*
 * Ajoute un fichier à la structure de données. S'il n'y a plus de place,
 * alloue un nouveau paquet de INC_ARBO entrées dont on consommera la
 * première. Le chemin du fichier est recopié dans une nouvelle zone
 * allouée pour l'occasion.
 */
void ajouter_arborescence (struct arbo *a, const char *chemin, off_t taille)
{
    if (a->pos >= a->taille)		// plus de place disponible
    {
	a->taille += INC_ARBO ;
	a->tab = realloc (a->tab, a->taille * sizeof (struct fichier)) ;
	if (a->tab == NULL)
	    raler (1, "cannot alloc memory for %d files", a->taille) ;
    }

    a->tab [a->pos].taille = taille ;
    a->tab [a->pos].chemin = malloc (strlen (chemin) + 1) ;
    if (a->tab [a->pos].chemin == NULL)
	raler (1, "cannot alloc memory for '%s'", chemin) ;
    strcpy (a->tab [a->pos].chemin, chemin) ;
    a->pos++ ;
}

/*
 * Libérer la mémoire, pour faire propre et net à la fin...
 */
void liberer_arborescence (struct arbo *a)
{
    if (a->tab != NULL)
    {
	int i ;

	for (i = 0 ; i < a->pos ; i++)
	    free (a->tab [i].chemin) ;
	free (a->tab) ;
    }
}

/******************************************************************************
 * Tri et affichagage de la structure de données
 */

int comparer (const void *p1, const void *p2)
{
    const struct fichier *f1 = p1 ;	// pas besoin de cast car p1 est void *
    const struct fichier *f2 = p2 ;

    return f1->taille - f2->taille ;
}

void trier_arborescence (struct arbo *a)
{
    qsort (a->tab, a->pos, sizeof (*a->tab), comparer) ;
}

void afficher_arborescence (struct arbo *a)
{
    int i ;

    for (i = 0 ; i < a->pos ; i++)
	printf ("%10ju\t%s\n", (uintmax_t) a->tab [i].taille,
		               a->tab [i].chemin) ;
}

/******************************************************************************
 * Extraction de tous les fichiers et stockage dans la structure de données
 */

void lire_repertoire (const char *, struct arbo *) ;
void lire_fichier (const char *, struct arbo *, const struct stat *) ;

/*
 * Lit une arborescence indifférenciée (i.e. fichier, répertoire ou autre)
 */
void lire_arborescence (const char *chemin, struct arbo *a)
{
    struct stat stbuf ;

    if (lstat (chemin, &stbuf) == -1)
	raler (1, "cannot stat %s", chemin) ;

    if (S_ISDIR (stbuf.st_mode))
	lire_repertoire (chemin, a) ;
    else if (S_ISREG (stbuf.st_mode))
	lire_fichier (chemin, a, &stbuf) ;
    else
    {
	/* ignorer les autres types, cf énoncé */
    }
}

/*
 * Lit une arborescence à partir d'un répertoire racine
 */
void lire_repertoire (const char *chemin, struct arbo *a)
{
    DIR *dp ;
    struct dirent *d ;

    dp = opendir (chemin) ;
    if (dp == NULL)
	raler (1, "cannot open directory %s", chemin) ;

    while ((d = readdir (dp)) != NULL)
    {
	if (strcmp (d->d_name, ".") != 0 && strcmp (d->d_name, "..") != 0)
	{
	    char nc [PATH_MAX] ;	// nouveau chemin construit
	    int taille ;		// taille (idéale) du nouveau chemin

	    /*
	     * Il est très important de vérifier qu'on ne déborde
	     * pas du tableau déclaré. Snprintf renvoie le nombre
	     * de caractères qui aurait été placés dans nc, s'il
	     * y avait eu suffisamment de place (sans compter le
	     * '\0'). Il suffit de vérifier que cette taille n'est
	     * pas supérieure à la taille de la variable.
	     * L'utilisation de snprintf n'est pas forcément la
	     * méthode la plus efficace, mais elle est rigoureuse
	     * tout en conservant une lisibilité correcte.
	     */

	    taille = snprintf (nc, sizeof nc, "%s/%s", chemin, d->d_name) ;
	    if (taille < 0)
		raler (0, "erreur de conversion snprintf pour '%s/%s'",
				chemin, d->d_name) ;
	    if ((unsigned) taille >= sizeof nc)
		raler (0, "chemin '%s/%s' trop long (> %zd)",
				chemin, d->d_name, PATH_MAX) ;

	    lire_arborescence (nc, a) ;
	}
    }

    if (closedir (dp) == -1)
	raler (1, "cannot close directory %s", chemin) ;
}

/*
 * Stocker le fichier dans le tableau. Le chemin réside en mémoire
 * dans un endroit non pérenne (variable locale de lire_repertoire ou
 * argv [1]), il faudra donc en créer une copie pour le conserver.
 */
void lire_fichier (const char *chemin, struct arbo *a, const struct stat *buf)
{
    ajouter_arborescence (a, chemin, buf->st_size) ;
}

/******************************************************************************
 * Main
 */

int main (int argc, char *argv [])
{
    struct arbo a ;

    prog = argv [0] ;

    if (argc != 2)
	usage () ;

    init_arborescence (&a) ;
    lire_arborescence (argv [1], &a) ;
    trier_arborescence (&a) ;
    afficher_arborescence (&a) ;
    liberer_arborescence (&a) ;

    exit (0) ;				// si on arrive là, tout est ok
}
