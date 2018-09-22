SHELL=/bin/bash
CC = gcc
CFLAGS= -Wall -Werror -Wextra -g -fprofile-arcs -ftest-coverage

BIN = $(patsubst %.c,%,$(wildcard *.c))

TESTS = checkarg checkfile checkline checkcolumn checkdiag checksig checksize checkmemory
TESTSUP = checkposition checkgcov

.PHONY: all

all : $(BIN) $(TESTS) $(TESTSUP)

clean :
	@/bin/rm -f *.o *.gcda *.gcno *.c.gcov *.h.gcov *~ $(BIN)

scriptline:
	@echo "#!/bin/bash" > scriptl.sh
	@echo "magic=0" >> scriptl.sh
	@echo 'result=`echo "$$1*($$1*$$1+1)/2" | bc`' >> scriptl.sh
	@echo "while read line; do" >> scriptl.sh
	@echo '  for i in $$line; do' >> scriptl.sh
	@echo '    magic=$$(($$magic + $$i))' >> scriptl.sh
	@echo "  done" >> scriptl.sh
	@echo '  if [ $$magic -ne $$result ]; then' >> scriptl.sh
	@echo "    exit 1" >> scriptl.sh
	@echo "  fi" >> scriptl.sh
	@echo "  magic=0" >> scriptl.sh
	@echo "done < result" >> scriptl.sh
	@chmod +x scriptl.sh

scriptcolumn:
	@echo "#!/bin/bash" > scriptc.sh
	@echo "magic=0" >> scriptc.sh
	@echo 'result=`echo "$$1*($$1*$$1+1)/2" | bc`' >> scriptc.sh
	@echo 'for i in `seq 1 $$1`; do' >> scriptc.sh
	@echo "  magic=0 " >> scriptc.sh
	@echo '  c=`cat result | cut -d " " -f$$i`' >> scriptc.sh
	@echo '  for j in $$c; do' >> scriptc.sh
	@echo '	   magic=$$(($$magic + $$j))' >> scriptc.sh
	@echo "  done" >> scriptc.sh
	@echo '  if [ $$magic -ne $$result ] ;then' >> scriptc.sh
	@echo "    exit 1" >> scriptc.sh
	@echo "  fi" >> scriptc.sh
	@echo "done" >> scriptc.sh
	@chmod +x scriptc.sh

scriptdiag:
	@echo "#!/bin/bash" > scriptd.sh
	@echo 'result=`echo "$$1*($$1*$$1+1)/2" | bc`' >> scriptd.sh
	@echo "i=1" >> scriptd.sh
	@echo 'j=$$1' >> scriptd.sh
	@echo "magic1=0" >> scriptd.sh
	@echo "magic2=0" >> scriptd.sh
	@echo "while read line; do" >> scriptd.sh
	@echo '  c=`echo $$line | cut -d " " -f$$i`' >> scriptd.sh
	@echo '  magic1=$$(($$magic1 + $$c))' >> scriptd.sh
	@echo '  c=`echo $$line | cut -d " " -f$$j`' >> scriptd.sh
	@echo '  magic2=$$(($$magic2 + $$c))' >> scriptd.sh
	@echo '  i=$$(($$i + 1))' >> scriptd.sh
	@echo '  j=$$(($$j - 1))' >> scriptd.sh
	@echo "done < result" >> scriptd.sh
	@echo 'if [ $$magic1 -ne $$result -o $$magic2 -ne $$result ]; then' >> scriptd.sh
	@echo "  exit 1" >> scriptd.sh
	@echo "fi" >> scriptd.sh
	@chmod +x scriptd.sh

checkarg:
	@echo "[32m################### TESTING: args[0m"
	./$(BIN) 16 2> /dev/null || exit 0 && exit 1
	./$(BIN) 5 2> /dev/null || exit 0 && exit 1
	./$(BIN) "-6" 2> /dev/null || exit 0 && exit 1
	@echo -e "[32m################### TESTING: passed\n[0m"

checkfile:
	@echo "[32m################### TESTING: numbers of lines and columns[0m"
	./$(BIN) 6 > result && test $$(wc -l < result) -ne 6 && exit 1 || exit 0
	test $$(wc -w < result) -ne 36 && exit 1 || exit 0
	./$(BIN) 26 > result && test $$(wc -l < result) -ne 26 && exit 1 || exit 0
	test $$(wc -w < result) -ne 676 && exit 1 || exit 0
	@echo -e "[32m################### TESTING: passed\n[0m"

checkline: scriptline
	@echo "[32m################### TESTING: lines give the magic number[0m"
	./$(BIN)  6 > result && ./scriptl.sh  6 || exit 1
	./$(BIN) 10 > result && ./scriptl.sh 10 || exit 1
	./$(BIN) 26 > result && ./scriptl.sh 26 || exit 1
	@rm scriptl.sh ; rm result
	@echo -e "[32m################### TESTING: passed\n[0m"

checkcolumn: scriptcolumn
	@echo "[32m################### TESTING: columns give the magic number[0m"
	./$(BIN)  6 > result && ./scriptc.sh  6 || exit 1
	./$(BIN) 10 > result && ./scriptc.sh 10 || exit 1
	./$(BIN) 26 > result && ./scriptc.sh 26 || exit 1
	@rm scriptc.sh ; rm result
	@echo -e "[32m################### TESTING: passed\n[0m"

checkdiag: scriptdiag
	@echo "[32m################### TESTING: diagonals give the magic number[0m"
	./$(BIN)  6 > result && ./scriptd.sh  6 || exit 1
	./$(BIN) 10 > result && ./scriptd.sh 10 || exit 1
	./$(BIN) 26 > result && ./scriptd.sh 26 || exit 1
	@rm scriptd.sh ; rm result
	@echo -e "[32m################### TESTING: passed\n[0m"

checksig:
	@echo "[32m################### TESTING: sync with signal[0m"
	for i in $$(seq 0 1000) ; do ./$(BIN) 6 > /dev/null ; done
	@echo -e "[32m################### TESTING: passed\n[0m"

checksize:
	@echo "[32m################### TESTING: large square[0m"
	./$(BIN) 1002 > /dev/null || exit 1
	./$(BIN) 1000002 2> /dev/null || exit 0 && exit 1
	@echo -e "[32m################### TESTING: passed\n[0m"

checkmemory:
	@echo "[32m################### TESTING: memory leak or error[0m"
	valgrind --leak-check=full --trace-children=yes --error-exitcode=1 ./magicsquare 1002 > /dev/null 2>&1 || exit 1
	@echo -e "[32m################### TESTING: passed\n[0m"

# Ajoutez vos tests ici

# Test de l'algorithme de resolution (position de chaque valeur après les échanges des cases)
checkposition:
	@echo "[32m################### TESTING: Algo of resolution => position of all val[0m"
	./$(BIN) 6 > result
	@echo "35 1 6 26 19 24 " > tst
	@echo "3 32 7 21 23 25 " >> tst
	@echo "31 9 2 22 27 20 " >> tst
	@echo "8 28 33 17 10 15 " >> tst
	@echo "30 5 34 12 14 16 " >> tst
	@echo "4 36 29 13 18 11 " >> tst
	@diff result tst || exit 1
	./$(BIN) 10 > result
	@echo "92 99 1 8 15 67 74 51 58 40 " > tst
	@echo "98 80 7 14 16 73 55 57 64 41 " >> tst
	@echo "4 81 88 20 22 54 56 63 70 47 " >> tst
	@echo "85 87 19 21 3 60 62 69 71 28 " >> tst
	@echo "86 93 25 2 9 61 68 75 52 34 " >> tst
	@echo "17 24 76 83 90 42 49 26 33 65 " >> tst
	@echo "23 5 82 89 91 48 30 32 39 66 " >> tst
	@echo "79 6 13 95 97 29 31 38 45 72 " >> tst
	@echo "10 12 94 96 78 35 37 44 46 53 " >> tst
	@echo "11 18 100 77 84 36 43 50 27 59 " >> tst
	@diff result tst || exit 1
	@rm result; rm tst
	@echo -e "[32m################### TESTING: passed\n[0m"

checkgcov:
	@echo "[32m################### TESTING: gcov all[0m"
	@gcov $(BIN).c
	@echo -e "[32m################### TESTING: passed\n[0m"