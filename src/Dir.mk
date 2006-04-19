CFLAGS		+= -I$(srcdir)

obj-y			:= main.o parse.o options.o readtags.o tree.o error.o list.o treeold.o

icomplete: $(obj-y)
	$(call cmd,ld,)

install-exec:
	$(INSTALL) --auto icomplete

targets		:= icomplete
subdirs		:=
