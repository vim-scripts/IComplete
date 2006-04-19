#!/usr/bin/env zsh

#if [[ -z $1 ]]; then
#	echo "Usage: $0 path_to_java_docs/html/api"
#	echo "       This could be: /usr/share/doc/java-sdk-docs-1.5.0/html/api"
#	exit
#fi

IFS="
"

rm /tmp/tags.filelist
for dir in `cat package-list`; do
	ls ${dir:gs#.#/#}/*.html >> /tmp/tags.filelist
done
sed -e 's/\.html//' -e 's#/#.#g' < /tmp/tags.filelist | egrep --invert-match "(package-|UNKNOWN)" | xargs javap >> /tmp/tags.java
ctags --fields=afiKmsSzn --java-kinds=cfm /tmp/tags.java
