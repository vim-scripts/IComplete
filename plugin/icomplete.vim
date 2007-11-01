" Vim global plugin for C/C++ codecompletion with icomplete
" Last Change:	2005 Mar 29
" Maintainer:	Martin Stubenschrott <stubenschrott@gmx.net>
" License:	This file is placed in the public domain.

if v:version < 700
	echomsg "You need at least Vim 7.0 to use icomplete"
	finish
end

if has("autocmd")
	autocmd FileType cpp,c set omnifunc=cppcomplete#CompleteMain
	autocmd FileType cpp,c inoremap <expr> <C-X><C-O> cppcomplete#Complete()
	autocmd FileType cpp,c inoremap <expr> . cppcomplete#CompleteDot()
	autocmd FileType cpp,c inoremap <expr> > cppcomplete#CompleteArrow()
	autocmd FileType cpp,c inoremap <expr> : cppcomplete#CompleteColon()
endif

