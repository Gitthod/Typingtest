flags=-Wall

2fingers: speed_test_sqlite.h speed_test.c speed_test_sqlite.c raw_term.h raw_term.c
	$(CC) -g -o 2fingers speed_test_sqlite.c raw_term.c speed_test.c -lsqlite3 -pthread $(flags)
