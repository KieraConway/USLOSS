TARGET=libphase3.a
ASSIGNMENT=452phase3
CC=gcc
AR=ar
COBJS= phase3.o tables.o lists.o
CSRCS=${COBJS:.o=.c}
HDRS= sems.h phase3_helper.h tables.h lists.h

INCLUDE = ./usloss/include
CFLAGS = -Wall -g -I${INCLUDE} -I. -Wno-int-to-pointer-cast -Wno-pointer-to-int-cast


LDFLAGS = -L. -L./usloss/lib
TESTDIR=testcases
TESTS= test00 test01 test02 test03 test04 test05 test06 test07 test08 \
       test09 test10 test11 test12 test13 test14 test15 test16 test17 \
       test18 test19 test20 test21 test22 test23 test24 test25

# Use one of the following LIBS lines, depending on whose phase1/2 you are using
# lxu’s phase2 and phase1
LIBS = -l452phase2 -l452phase1 -lusloss -l452phase1\
		-l452phase2 -lphase3
# Your phase1 and your phase2
#LIBS = -lphase2 -lphase1 -lusloss -luser

$(TARGET):	$(COBJS)
		$(AR) -r $@ $(COBJS)

$(TESTS):	$(TARGET)  p1.o libuser.o
	$(CC) $(CFLAGS) -c $(TESTDIR)/$@.c
	$(CC) $(CFLAGS) -c libuser.c
	$(CC) $(LDFLAGS) -o $@ $@.o $(LIBS) p1.o libuser.o

clean:
	rm -f $(COBJS) $(TARGET) test*.o term* $(TESTS) libuser.o p1.o core

cleanAll:
	rm -f test??.c
	#make clean

phase3.o:	sems.h phase3_helper.h tables.h lists.h
	$(CC) $(CFLAGS) -c phase3.c

#Needed for CLion IDE can be ignored
all:
	$(CC) $(CFLAGS) -c $(TESTDIR)/test00.c
	$(CC) $(LDFLAGS) -o test00 test00.o $(LIBS) p1.o libuser.o
