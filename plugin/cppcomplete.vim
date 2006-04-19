" Vim global plugin for C/C++ codecompletion with icomplete
" Last Change:	2005 Oct 23
" Maintainer:	Martin Stubenschrott <stubenschrott@gmx.net>
" License:	This file is placed in the public domain.

" This function is used for the 'omnifunc' option.
function! cppcomplete#Complete(findstart, base)
	if a:findstart
		let line = getline('.')
		let start = col('.') - 1
		let s:cppcomplete_col = col('.') " for calling the external icomplete command, store the col
		let s:cppcomplete_line = line('.')
		let s:show_overloaded = 0	" set this to true, if we want to complete all overloaded functions after a (

		" save the current buffer as a temporary file
		let s:cppcomplete_tmpfile = tempname()
		exe "silent! write!" s:cppcomplete_tmpfile

		" Locate the start of the item, including "." and "->" and "(" and "::".
		while start > 0
			"if line[start - 1] =~ '\w\|\.\|(' " a . or ( is the start of our completion
			if line[start - 1] =~ '\w' " a . or ( is the start of our completion
				let start -= 1
			elseif line[start - 1] == '.'
				break
			elseif line[start - 1] == '('
				let start -= 1
				let s:show_overloaded = 1
				break
			elseif start > 1 && line[start - 2] == '-' && line[start - 1] == '>'
				break
			elseif start > 1 && line[start - 2] == ':' && line[start - 1] == ':'
				break
			else
				break
			endif
		endwhile
		return start
	endif

	" Return list of matches.
	let res = []

	" check if set g:cppcomplete_tagfile
	if exists('g:cppcomplete_tagfile')
		let s:tagparam = " --tagfile=" . g:cppcomplete_tagfile
	else
		let s:tagparam = ""
	endif

	" call icomplete and store the result in the array
	" java and c# users must build their tags file manually
	if &ft == "java" || &ft == "cs"
		let cmd =  "icomplete --cache=0 -c " . s:cppcomplete_col . " -l " . s:cppcomplete_line . s:tagparam . " " . s:cppcomplete_tmpfile
	else
		let cmd =  "icomplete --cache=1 -c " . s:cppcomplete_col . " -l " . s:cppcomplete_line . s:tagparam . " " . s:cppcomplete_tmpfile
	endif
	
	let icomplete_output = system(cmd)
	if v:shell_error != 0
		echo "Could not parse expression"
		return res
	endif

	" found some results, return them
	let g:ov = s:show_overloaded
	for m in split(icomplete_output, '\n')
		if s:show_overloaded == 0
			if m =~ '^\C' . a:base " only words which start with base text - \C force using case
				let ident =  matchstr(m,'^\([a-zA-Z_0-9(]\)\+\C') 
				call add(res, ident)
			endif
		else " complete function arguments
			" the second match is the function prototype
			let ident =  matchstr(m, '\s*(.*)', 0, 2)
			" placeholders
			if exists("g:cppcomplete_placeholders") && g:cppcomplete_placeholders == 1
				let ident = substitute(ident, '^(\s*', '\0<+', "")
				let ident = substitute(ident, '\s*)', '+>\0', "")
				let ident = substitute(ident, '\s*,\s*', '+>\0<+', "g")
			endif
			call add(res, ident)
		endif
	endfor
	let g:res = res
	return res

endfunc
