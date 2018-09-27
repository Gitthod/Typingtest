CC = gcc
# Include the directoy for <*.h>
CFLAGS=-I$(IDIR) -pthread -Wall

IDIR := include

SRCDIR := src

ODIR := obj

# Directoy to put the executables
BINDIR := binaries

# Don't show an error if the directory exists
MKDIR_P := mkdir -p

# Libraries needed
LIBS := -lsqlite3

# Depend from all the header files
DEPS := $(wildcard $(IDIR)/*.h)

OBJ := $(patsubst $(SRCDIR)%, $(ODIR)%, $(patsubst %.c,%.o,$(wildcard $(SRCDIR)/*.c)))

# Pattern rule the $< automatic variable is the first prerequisite of the target, $@ is the target's name
# the -c option in GCC tells the compiler to not link the .o files
$(ODIR)/%.o:$(SRCDIR)/%.c $(DEPS) | $(ODIR)
	$(CC) -c -o $@ $< $(CFLAGS)


$(BINDIR)/2fingers: $(OBJ) | $(BINDIR) $(ODIR)
	$(CC) -o $(BINDIR)/2fingers $(OBJ) $(CFLAGS) $(LIBS)


$(BINDIR):
	${MKDIR_P} ${BINDIR}

$(ODIR) :
	${MKDIR_P} ${ODIR}

# Make a phony target so that make clean would run unconditionally even if a clean file was created
.PHONY: clean

# Files with a "~" at the end of their names (for example, example.txt~) are automatically created backup
# copies of documents edited in the gedit text editor or other applications.  It is safe to delete them,
# but there's no harm to leave them on your computer.  -f, --force ignore nonexistent files, never prompt.
cleanl:
	rm -f $(ODIR)/*.o *~ core $(IDIR)/*~

#-r, -R, --recursive   remove directories and their contents recursively
clean:
	rm -fr $(ODIR) $(BINDIR)
