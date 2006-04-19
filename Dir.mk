subdirs		:= src

install-data:
	$(SRCDIR_INSTALL) $(datadir)/vim/vimfiles/plugin icomplete.vim 
	$(SRCDIR_INSTALL) $(datadir)/vim/vimfiles/autoload cppcomplete.vim 
	$(SRCDIR_INSTALL) $(sysconfdir) icomplete.conf 
