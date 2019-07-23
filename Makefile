CC = gcc
# Include the directoy for <*.h>
CFLAGS=-I$(IDIR) -pthread -Wall -D_GNU_SOURCE

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
	@echo Building $@
	@$(CC) -c -o $@ $< $(CFLAGS)


$(BINDIR)/2fingers: $(OBJ) | $(BINDIR) $(ODIR)
	@echo Linking everything into an executable...
	@$(CC) -o $(BINDIR)/2fingers $(OBJ) $(CFLAGS) $(LIBS)
	@echo Program 2fingers was succesfully built in ./binaries


$(BINDIR):
	@$(MKDIR_P) $(BINDIR)
	@echo Created ./$(BINDIR) folder

$(ODIR) :
	@$(MKDIR_P) $(ODIR)
	@echo Created ./$(ODIR) folder

# Make a phony target so that make clean would run unconditionally even if a clean file was created
.PHONY: clean

#-r, -R, --recursive   remove directories and their contents recursively
clean:
	@echo Removing compiled files...
	@rm -fr $(ODIR) $(BINDIR)
	@echo Files were succesfully removed
