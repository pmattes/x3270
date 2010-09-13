#!/usr/local/bin/bash
# Construct charset/codepage resources for resources.c, based on X3270.xad
awk -F: '
BEGIN { inr=0; }
function stripcat(s,    i) {
    while(substr(s,1,1)==" " || substr(s,1,1)=="\t") s=substr(s,2)
    if (substr(s,length(s),1)=="\\") s=substr(s,1,length(s)-1)
    if (substr(s,length(s)-1,2)=="\\n") s=substr(s,1,length(s)-2)
    t=t s;
}
function strip(s,   i,x,ns,c) {
    while(substr(s,1,1)==" " || substr(s,1,1)=="\t") s=substr(s,2);
    if (substr(s,length(s)-2,2)=="\\n") s=substr(s,1,length(s)-2);
    x=""
    ns=0
    for (i=1; i<=length(s); i++) {
	c=substr(s,i,1)
	if (c==" " || c=="\t") ns=ns+1; else {
	    if (ns>0 && c!="\\") x=x " "
	    ns=0
	    if (c=="\"") x=x "\\\""; else x=x c
	}
    }
    printf "\"%s\"", x;
}
{ if (substr($0,1,14)=="x3270.charset." || substr($0,1,15)=="x3270.codepage.") {
    printf "%s", "{ \"" substr($1,7) "\","
    if (substr($0,length($0),1) == "\\") { print ""; inr=1; if ($2==" \\") t=""; else { t=""; stripcat($2); t=t "\\n\\\n"; } } else { printf " "; strip($2); print " }," }
    next
  }
}
inr==1 {
    stripcat($0)
    if (substr($0,length($0),1) != "\\") { strip(t); print " },"; inr=0 } else { t=t "\\n\\\n" }
}' ../x3270/X3270.xad
