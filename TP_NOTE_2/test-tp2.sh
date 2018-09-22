#!/bin/sh

set -u

PROG=${PROG:-./lsrectri}
V=${VALGRIND:-}         # appeler avec la var. VALGRIND à "" ou "valgrind -q"

DIR=/tmp/tp2-dir.$$	# répertoire de test
RES=/tmp/tp2-res.$$	# sortie du test
LOG=test-tp2.log	# log : dans le répertoire courant

# trap "rm -rf $DIR $RES" EXIT	# pour supprimer les fichiers à la sortie

annoncer ()
{
    COURANT="$1"
}

fail ()
{
    echo "==> Échec du test '$COURANT'"
    echo "==> Log :"
    cat $LOG
    echo "==> Exit"
    exit 1
}

# creer un fichier de taille et de nom indiqués
creer_fichier ()
{
    local taille="$1" nom="$2"

    dd if=/dev/zero of=$nom bs=$taille count=1 2> /dev/null
}

# normaliser la sortie (supprimer espaces superflus gênant la comparaison)
normaliser ()
{
    sed -e 's/[ 	][ 	]*/ /g' -e 's/^ //'
}

exec 2> $LOG		# rediriger stderr du script vers un fichier
set -x

mkdir $DIR		# notre répertoire de travail

#
# Tester la syntaxe : le code de retour en cas d'erreur doit être != 0
#

annoncer "Syntaxe sans argument"
$V $PROG >&2						&& fail

annoncer "Syntaxe avec 2 arguments"
$V $PROG $DIR $DIR >&2					&& fail

#
# Tester un répertoire vide
#

annoncer "Répertoire vide"
$V $PROG $DIR > $RES && cmp /dev/null $RES		|| fail

#
# Un seul fichier
#

annoncer "Répertoire avec un seul fichier"
creer_fichier 1000 $DIR/f1
$V $PROG $DIR > $RES && test $(wc -l < $RES) -eq 1	|| fail

#
# Trois fichiers : ordre de tri correct ?
# Avec un fichier commençant par un point pour rire un peu...
#

annoncer "Répertoire avec 3 fichiers"
creer_fichier 500  $DIR/.f2
creer_fichier 2000 $DIR/f3
ATTENDU="500 $DIR/.f2
1000 $DIR/f1
2000 $DIR/f3"
$V $PROG $DIR | normaliser > $RES && echo "$ATTENDU" | diff -q $RES - || fail

#
# Exploration récursive d'un petit répertoire
#

annoncer "Exploration récursive"
mkdir -p $DIR/d1/d11 $DIR/d2/d21 $DIR/d2/d22
creer_fichier 700  $DIR/d1/f4
creer_fichier 400  $DIR/d1/f5
creer_fichier 800  $DIR/d2/d21/f6
creer_fichier 300  $DIR/d2/d21/f7
creer_fichier 100  $DIR/d2/d21/f8
creer_fichier 3000 $DIR/d2/d21/f9
ATTENDU="100 $DIR/d2/d21/f8
300 $DIR/d2/d21/f7
400 $DIR/d1/f5
500 $DIR/.f2
700 $DIR/d1/f4
800 $DIR/d2/d21/f6
1000 $DIR/f1
2000 $DIR/f3
3000 $DIR/d2/d21/f9"
$V $PROG $DIR | normaliser > $RES && echo "$ATTENDU" | diff -q $RES - || fail

#
# Résistance aux gros volumes (et aux liens symboliques)
#

annoncer "Gros volume"
$V $PROG /usr > $RES					|| fail

echo "Test réussi"
exit 0
