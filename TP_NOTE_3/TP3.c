#include <unistd.h>
#include <stdlib.h>
#include <string.h>
#include <fcntl.h>
#include <stdarg.h>
#include <stdio.h>			// pour les msg d'erreur uniquement
#include <stdnoreturn.h>		// C norme 2011
#include <sys/types.h>
#include <sys/wait.h>
#include <errno.h>
#include <ctype.h>

#define	CODE_ERREUR	255

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

    exit (CODE_ERREUR) ;
}

noreturn void usage (void)
{
    fprintf (stderr, "usage: %s [-d|-v] [fichier-spec]\n", prog) ;
    exit (1) ;
}

/******************************************************************************
 * Transformation de la spécification en structure d'arbre
 * => utiliser la fonction analyser_spec()
 */

struct sommet
{
    int val ;					// étiquette du sommet
    struct sommet *frere_suivant ;		// liste des frères
    struct sommet *premier_fils ;		// premier des fils
    /*
     * Ce dernier champ (dernier_fils) n'est utilisé que pendant la
     * construction de l'arbre, pour insérer en queue et ainsi
     * respecter l'ordre des processus dans la spécification.
     */
    struct sommet *dernier_fils ;		// pour insertion en queue
} ;

/*
 * Fonction interne pour l'analyse de la spécification : analyse une
 * ligne de la spécification (la ligne pointée par *spec), et
 * retourne la profondeur, la valeur, et la position suivante (via
 * *spec) et un code pour dire si on a rencontré une erreur, une fin
 * de spéc ou si tout baigne.
 */

enum ret_al { AL_ERR, AL_FIN, AL_OK } ;		// retour de analyser_ligne

enum ret_al analyser_ligne (char **spec, int *prof, int *val)
{
    *prof = *val = 0 ;				// mettre les compteurs à 0

    if (**spec == '\0')				// spec terminée
	return AL_FIN ;

    /*
     * Compter les espaces pour avoir la profondeur
     */

    while (**spec == ' ')
    {
	(*spec)++ ;
	(*prof)++ ;
    }

    /*
     * Récupérer la valeur
     */

    char *debut = *spec ;			// vérifier au moins un chiffre
    while (isdigit (**spec))
    {
	*val = *val * 10 + (**spec - '0') ;
	(*spec)++ ;
    }
    if (debut == *spec)				// on n'a pas avancé => pb
	return AL_ERR ;

    /*
     * Vérifier qu'on est bien à la fin d'une ligne
     */

    if (**spec != '\n')
	return AL_ERR ;
    (*spec)++ ;					// passer au sommet suivant

    return AL_OK ;
}

/*
 * Fonction interne pour l'analyse de la spécification : crée
 * un sommet isolé avec l'étiquette indiquée.
 */

struct sommet *creer_sommet (int val)
{
    struct sommet *s ;

    s = malloc (sizeof *s) ;
    if (s == NULL)
	raler (1, "cannot malloc %d bytes", sizeof *s) ;

    s->frere_suivant = NULL ;
    s->premier_fils = NULL ;
    s->dernier_fils = NULL ;
    s->val = val ;
    return s ;
}

/*
 * Fonction interne pour l'analyse de la spécification : analyse la
 * spécification à partir de la profondeur donnée.
 * Pré-conditions :
 *   - numl pointe sur le num de la ligne courante de la spéc, pour les msg err
 *   - spec pointe sur la spéc de la racine du sous-arbre à étudier
 *	qui doit exister et avoir la profondeur attendue
 *   - prof donne la profondeur attendue pour cette racine
 * Post-conditions :
 *   - valeur de retour : racine du sous-arbre étudié
 *   - spec pointe sur la première ligne suivant le sous-arbre
 *   - numl pointe sur le numéro de de la première ligne suivant le sous-arbre
 */

struct sommet *analyser_spec_profondeur (int *numl, char **spec, int prof)
{
    char *nspec ;
    int nprof, val ;
    struct sommet *s ;
    enum ret_al r ;

    /*
     * Analyser la racine du sous-arbre
     */

    nspec = *spec ;				// ne pas avancer si pas ok
    r = analyser_ligne (&nspec, &nprof, &val) ;	// lire la ligne numl
    switch (r)
    {
	case AL_ERR :
	    raler (0, "cannot parse spec at line %d", *numl) ;
	case AL_FIN :
	    return NULL ;
	case AL_OK :				// on continue en séquence
	    break ;
	default :				// programmation défensive
	    raler (0, "internal error at line %d", *numl) ;
    }
    if (nprof != prof)
	raler (0, "unexpected depth (%d) at line %d", nprof, *numl) ;

    s = creer_sommet (val) ;
    *spec = nspec ;
    (*numl)++ ;

    /*
     * Chercher les fils directs de la racine
     */

    while ((r = analyser_ligne (&nspec, &nprof, &val)) == AL_OK && nprof > prof)
    {
	struct sommet *f ;			// sous-arbre fils trouvé

	nspec = *spec ;
	f = analyser_spec_profondeur (numl, &nspec, prof + 1) ;

	// Est-ce le premier fils ou bien y-a-t'il déjà au moins un ?
	if (s->dernier_fils == NULL)
	{
	    s->premier_fils = s->dernier_fils = f ;
	}
	else
	{
	    s->dernier_fils->frere_suivant = f ;
	    s->dernier_fils = f ;
	}
	*spec = nspec ;
    }

    if (r == AL_ERR)
	raler (0, "cannot parse spec at line %d", *numl) ;
    // rien à faire si r == AL_FIN

    return s ;
}

/*
 * Fonction à utiliser : analyse une spécification donnée sous forme
 * d'une chaîne de caractères contenant le texte complet de la
 * spécification (cf énoncé). Ce texte est une succession de lignes
 * terminée par '\0' pour la fin de chaîne. Par exemple :
 * spec = "200\n 500\n  200\n   300\n  600\n 100\n  0\n\0"
 */

struct sommet *analyser_spec (char *spec)
{
    struct sommet *s ;				// arbre obtenu
    int numl ;					// numéro de ligne pour msg err

    numl = 1 ;
    s = analyser_spec_profondeur (&numl, &spec, 0) ;
    if (*spec != '\0')				// est-on bien à la fin ?
	raler (0, "invalid spec at line %d", numl) ;
    return s ;
}

/******************************************************************************
 * Lecture de la spécification avec les primitives systèmes : la
 * spécification est petite (on est limité à 254 processus), on peut
 * donc lire tout le fichier en mémoire. Mais comme on ne connaît pas
 * la taille a priori, il faut lire morceau par morceau jusqu'à la fin.
 */

#define	TAILLE_INIT	100		// pour la lecture de la spécification 
#define	PLACE_MIN	50		// place minimum dispo pour un read

char *lire_spec (int fd)
{
    char *spec ;			// la spec en cours de lecture
    size_t talloc ;			// taille de la zone mallocée
    size_t tspec ;			// taille de la spec (< talloc)
    ssize_t nlus ;			// retour de read

    /*
     * Intialiser la zone de lecture avec un premier bloc
     */

    talloc = TAILLE_INIT ;
    spec = malloc (talloc) ;
    if (spec == NULL)
	raler (1, "cannot malloc %d bytes", talloc) ;

    /*
     * Accumuler tous les morceaux lus dans une seule zone "mallocée"
     * (en laissant toujours une place pour le \0 de fin de chaîne
     * qu'on ajoutera après).
     * Pré-condition : place disponible dans la zone mallocée >= PLACE_MIN
     */

    tspec = 0 ;
    while ((nlus = read (fd, spec + tspec, talloc - tspec - 1)) > 0)
    {
	tspec += nlus ;
	if (talloc - tspec <= PLACE_MIN)
	{
	    talloc *= 2 ;
	    spec = realloc (spec, talloc) ;
	    if (spec == NULL)
		raler (1, "cannot realloc %d bytes", talloc) ;
	}
    }
    if (nlus == -1)
	raler (1, "specification reading") ;

    spec [tspec] = '\0' ;		// marqueur de fin de spec

    return spec ;
}

/******************************************************************************
 * Affichage de debug
 */

void afficher_arbre (struct sommet *racine, int prof)
{
    int i ;
    struct sommet *fils ;

    if (racine == NULL)
	return ;

    for (i = 0 ; i < prof ; i++)
	putchar (' ') ;
    printf ("%d\n", racine->val) ;

    fils = racine->premier_fils ;
    while (fils != NULL)
    {
	afficher_arbre (fils, prof+1) ;
	fils = fils->frere_suivant ;
    }
}

/******************************************************************************
 * Génération des processus
 */

/*
 * Attend la terminaison de tous les processus fils.
 * Renvoie le nombre de processus créés, ou la valeur d'erreur.
 */

int attendre_les_fils (int nfils)
{
    int i, somme ;

    somme = 0 ;
    for (i = 0 ; i < nfils ; i++)
    {
	int statut, code ;
	pid_t pid ;

	pid = wait (&statut) ;
	if (pid == -1)
	    raler (1, "cannot wait") ;

	if (! WIFEXITED (statut))
	    raler (0, "process %d did not stop with exit", pid) ;
	code = WEXITSTATUS (statut) ;
	if (code == CODE_ERREUR)
	    exit (code) ;

	if (somme + code >= CODE_ERREUR)
	    raler (0, "too many children") ;
	somme += code ;
    }

    return somme ;
}

/*
 * Génère les processus pour le sous-arbre (on suppose déjà créé
 * le processus pour la racine du sous-arbre). Renvoie le nombre
 * de processus créés, ou la valeur d'erreur.
 */

int genfork (struct sommet *racine, int verbose)
{
    struct sommet *fils ;
    int nproc ;				// nb de processus du sous-arbre
    int nfils ;				// nb de fils directs
    pid_t pidmoi, pidfils ;

    /*
     * Cas particulier pour l'arbre vide
     */

    if (racine == NULL)
	return 0 ;

    /*
     * Générer les processus pour les fils
     */

    pidmoi = getpid () ;
    nfils = 0 ;
    fils = racine->premier_fils ;

    while (fils != NULL)
    {
	nfils++ ;
	switch (pidfils = fork ())
	{
	    case -1 :
		raler (1, "cannot fork child %d of node %d",
					fils->val, racine->val) ;

	    case 0 :			// fils
		nproc = genfork (fils, verbose) ;
		exit (nproc) ;

	    default :			// père
		/*
		 * Affichage en mode verbose : pour ces affichages,
		 * on utilise fflush(stdout) pour éliminer les
		 * bufferisations intempestives en cas de redirection
		 * de la sortie standard. Sans le fflush(stdout), un
		 * printf("toto\n") redirigé vers un fichier conserve
		 * la donnée ("toto\n") dans un buffer. Si le processus
		 * crée alors un fils, le buffer est recopié, ce qui
		 * fait que le père *et* le fils afficheront "toto\n".
		 */

		if (verbose)
		{
		    printf ("pid %d -> %d\n", pidmoi, pidfils) ;
		    fflush (stdout) ;
		}

		break ;
	}
	fils = fils->frere_suivant ;
    }

    /*
     * Attendre le nombre de secondes spécifié
     */

    if (verbose)
    {
	printf ("pid %d : attente %d ms\n", getpid (), racine->val) ;
	fflush (stdout) ;
    }

    usleep (racine->val * 1000) ;

    /*
     * Attendre les fils et récupérer le nombre de processus total
     * du sous-arbre
     */

    nproc = attendre_les_fils (nfils) ;
    if (verbose)
    {
	printf ("pid %d <- %d processus\n", getpid (), nproc) ;
	fflush (stdout) ;
    }
    return nproc + 1 ;
}

/******************************************************************************
 * Libération de la mémoire
 */

void liberer_memoire (struct sommet *racine, char *spec)
{
    if (spec != NULL)
	free (spec) ;

    if (racine != NULL)
    {
	struct sommet *fils ;

	fils = racine->premier_fils ;
	while (fils != NULL)
	{
	    struct sommet *suivant ;

	    suivant = fils->frere_suivant ;
	    liberer_memoire (fils, NULL) ;
	    fils = suivant ;
	}
	free (racine) ;
    }
}

/******************************************************************************
 * Main
 */

int main (int argc, char *argv [])
{
    char *spec ;
    struct sommet *arbre ;
    int opt ;
    int debug, verbose ;
    int fd ;
    int r ;

    prog = argv [0] ;
    debug = 0 ;
    verbose = 0 ;
    while ((opt = getopt (argc, argv, "dv")) != -1)
    {
	switch (opt)
	{
	    case 'd' :
		debug = 1 ;
		break ;
	    case 'v' :
		verbose = 1 ;
		break ;
	    default :
		usage () ;
	}
    }

    if (debug && verbose)		// options mutuellement exclusives
	usage () ;

    switch (argc - optind)
    {
	case 0 :
	    spec = lire_spec (0) ;
	    break ;
	case 1 :
	    fd = open (argv [optind], O_RDONLY) ;
	    if (fd == -1)
		raler (1, "cannot open %s", argv [optind]) ;
	    spec = lire_spec (fd) ;
	    if (close (fd) == -1)
		raler (1, "cannot close %s", argv [optind]) ;
	    break ;
	default :
	    usage () ;
    }

    arbre = analyser_spec (spec) ;
    if (debug)
    {
	afficher_arbre (arbre, 0) ;
	r = 0 ;
    }
    else
	r = genfork (arbre, verbose) ;

    liberer_memoire (arbre, spec) ;

    exit (r) ;
}
