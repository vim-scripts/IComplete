" Vim global plugin for C/C++ codecompletion with icomplete
" Last Change:	2005 Mar 29
" Maintainer:	Martin Stubenschrott <stubenschrott@gmx.net>
" License:	This file is placed in the public domain.

if v:version < 700
	echomsg "You need at least Vim 7.0 to use icomplete"
	finish
end

" Only do this part when compiled with support for autocommands.
if has("autocmd")
	autocmd Filetype cpp,c,java,cs set omnifunc=cppcomplete#Complete
endif
