# Copyright (c) 2007-2024 Paul Mattes.
# All rights reserved.
#
# Redistribution and use in source and binary forms, with or without
# modification, are permitted provided that the following conditions are met:
#     * Redistributions of source code must retain the above copyright
#       notice, this list of conditions and the following disclaimer.
#     * Redistributions in binary form must reproduce the above copyright
#       notice, this list of conditions and the following disclaimer in the
#       documentation and/or other materials provided with the distribution.
#     * Neither the names of Paul Mattes nor the names of his contributors
#       may be used to endorse or promote products derived from this software
#       without specific prior written permission.
#
# THIS SOFTWARE IS PROVIDED BY PAUL MATTES "AS IS" AND ANY EXPRESS OR IMPLIED
# WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE IMPLIED WARRANTIES OF
# MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE ARE DISCLAIMED. IN NO
# EVENT SHALL PAUL MATTES BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL,
# SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED
# TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR
# PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY OF
# LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT (INCLUDING
# NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE OF THIS
# SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
# 
# Makefile for the Webpage directory

PAGES = documentation-bugs.html documentation-manpages.html \
	documentation-misc.html documentation-other.html documentation.html \
	index.html screenshots.html x026.html

SCREENS = c3270-thumb.png c3270.png cyrillic-thumb.png cyrillic.png \
	japanese-thumb.png japanese.png keypad-thumb.png keypad.png \
	wc3270-thumb.png wc3270.png x3270_main-thumb.png x3270_main.png

OTHERS = $(SCREENS) SS-HCS12-1372-00.pdf c3270-man.html ibm_hosts.html \
	 man/FAQ.html pr3287-man.html s3270-man.html styles.css \
	 tcl3270-man.html wc3270-man.html x026.gif x3270-man.html \
	 x3270.jpg .htaccess c3270-help/.htaccess wc3270-help/.htaccess \
	 wx3270-help/.htaccess x3270-help/.htaccess

all: $(PAGES) webpage.tgz

clean:
	$(RM) $(PAGES) webpage.tgz

# Rules for HTML files with the navigation bar.
documentation.html: documentation-body.html mkpages.bash version.txt
	./mkpages.bash -I../Common documentation
documentation-bugs.html: documentation-bugs-body.html mkpages.bash version.txt
	./mkpages.bash -I../Common documentation-bugs
documentation-manpages.html: documentation-manpages-body.html mkpages.bash  version.txt
	./mkpages.bash -I../Common documentation-manpages
documentation-misc.html: documentation-misc-body.html mkpages.bash version.txt
	./mkpages.bash -I../Common documentation-misc
documentation-other.html: documentation-other-body.html mkpages.bash version.txt
	./mkpages.bash -I../Common documentation-other
index.html: index-body.html mkpages.bash version.txt
	./mkpages.bash -I../Common index
screenshots.html: screenshots-body.html mkpages.bash version.txt
	./mkpages.bash -I../Common screenshots
x026.html: x026-body.html mkpages.bash version.txt
	./mkpages.bash -I../Common x026

c3270-man.html:
	cd ../c3270 && $(MAKE) -f Makefile.aux html/c3270-man.html
ibm_hosts.html:
	cd ../ibm_hosts && $(MAKE) -f Makefile.aux html/ibm_hosts.html
pr3287-man.html:
	cd ../pr3287 && $(MAKE) -f Makefile.aux html/pr3287-man.html
s3270-man.html:
	cd ../s3270 && $(MAKE) -f Makefile.aux html/s3270-man.html
tcl3270-man.html:
	cd ../tcl3270 && $(MAKE) -f Makefile.aux html/tcl3270-man.html
x3270-man.html:
	cd ../x3270 && $(MAKE) -f Makefile.aux html/x3270-man.html
wc3270-man.html:
	cd ../wc3270 && $(MAKE) -f Makefile.aux html/wc3270-man.html

WPDIR=../obj/Webpage
TARBALL=$(WPDIR)/webpage.tgz
$(WPDIR):
	mkdir -p $@

webpage.tgz: $(WPDIR) $(PAGES) $(OTHERS) mkpages.bash Makefile
	./mktar.bash -I../Common -I../c3270/html -I../ibm_hosts/html -I../pr3287/html -I../s3270/html -I../tcl3270/html -I../x3270/html -I../wc3270/html -o $(TARBALL) $(PAGES) $(OTHERS)
	@ls -l $(TARBALL)
