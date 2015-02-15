# Copyright (c) 2007-2015, Paul Mattes.
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

PAGES = credits.html documentation-bugs.html documentation-faq.html \
	documentation-manpages.html documentation-misc.html \
	documentation-other.html documentation-relnotes.html \
	documentation-ssl.html documentation.html download.html index.html \
	license.html screenshots.html x026.html

SCREENS = c3270-thumb.png c3270.png cyrillic-thumb.png cyrillic.png \
	japanese-thumb.png japanese.png keypad-thumb.png keypad.png \
	wc3270-thumb.png wc3270.png x3270_main-thumb.png x3270_main.png

OTHERS = $(SCREENS) Brackets.html Build-c3270.html Build-tcl3270.html \
	Build-x3270.html Charset.html HotBugs.html Keymap.html \
	SS-HCS12-1372-00.pdf Unix/c3270-man.html Unix/ibm_hosts.html \
	Unix/pr3287-man.html Unix/s3270-man.html Unix/tcl3270-man.html \
	Unix/x3270-man.html Unix/x3270-script.html Unix/x3270if.html \
	Windows/wc3270-man.html Windows/wc3270-script.html \
	Windows/wpr3287-man.html Windows/ws3270-man.html Windows/x3270if.html \
	Wishlist.html c3270-Resources.html c3270Keymap.html del.jpg delta.jpg \
	man/FAQ.html rest.html s3270-Resources.html styles.css \
	tcl3270-Resources.html wc3270-Resources.html wc3270Keymap.html \
	ws3270-Resources.html x026.gif x3270-Resources.html x3270.jpg

all: $(PAGES) webpage.tgz

clean:
	$(RM) $(PAGES) webpage.tgz

# Rules for HTML files with the navigation bar.
credits.html: credits-body.html mkpages.bash
	./mkpages.bash credits
documentation.html: documentation-body.html mkpages.bash
	./mkpages.bash documentation
documentation-bugs.html: documentation-bugs-body.html mkpages.bash
	./mkpages.bash documentation-bugs
documentation-faq.html: documentation-faq-body.html mkpages.bash
	./mkpages.bash documentation-faq
documentation-manpages.html: documentation-manpages-body.html mkpages.bash
	./mkpages.bash documentation-manpages
documentation-misc.html: documentation-misc-body.html mkpages.bash
	./mkpages.bash documentation-misc
documentation-other.html: documentation-other-body.html mkpages.bash
	./mkpages.bash documentation-other
documentation-relnotes.html: documentation-relnotes-body.html mkpages.bash
	./mkpages.bash documentation-relnotes
documentation-ssl.html: documentation-ssl-body.html mkpages.bash
	./mkpages.bash documentation-ssl
download.html: download-body.html mkpages.bash
	./mkpages.bash download
index.html: index-body.html mkpages.bash
	./mkpages.bash index
license.html: license-body.html mkpages.bash
	./mkpages.bash license
screenshots.html: screenshots-body.html mkpages.bash
	./mkpages.bash screenshots
x026.html: x026-body.html mkpages.bash
	./mkpages.bash x026

Unix/c3270-man.html:
	cd ../c3270 && $(MAKE) -f Makefile.aux html/c3270-man.html
Unix/ibm_hosts.html:
	cd ../x3270 && $(MAKE) -f Makefile.aux html/ibm_hosts.html
Unix/pr3287-man.html:
	cd ../pr3287 && $(MAKE) -f Makefile.aux html/pr3287-man.html
Unix/s3270-man.html:
	cd ../s3270 && $(MAKE) -f Makefile.aux html/s3270-man.html
Unix/tcl3270-man.html:
	cd ../tcl3270 && $(MAKE) -f Makefile.aux html/tcl3270-man.html
Unix/x3270-man.html:
	cd ../x3270 && $(MAKE) -f Makefile.aux html/x3270-man.html
Unix/x3270-script.html:
	cd ../x3270 && $(MAKE) -f Makefile.aux html/x3270-script.html
Unix/x3270if.html:
	cd ../x3270 && $(MAKE) -f Makefile.aux html/x3270if.html

Windows/wc3270-man.html:
	cd ../wc3270 && $(MAKE) -f Makefile.aux html/wc3270-man.html
Windows/wc3270-script.html:
	cd ../wc3270 && $(MAKE) -f Makefile.aux html/wc3270-script.html
Windows/wpr3287-man.html:
	cd ../wpr3287 && $(MAKE) -f Makefile.aux html/wpr3287-man.html
Windows/ws3270-man.html:
	cd ../ws3270 && $(MAKE) -f Makefile.aux html/ws3270-man.html
Windows/x3270if.html:
	cd ../wc3270 && $(MAKE) -f Makefile.aux html/x3270if.html

c3270-Resources.html:
	cd ../c3270 && $(MAKE) -f Makefile.aux html/Resources.html
s3270-Resources.html:
	cd ../s3270 && $(MAKE) -f Makefile.aux html/Resources.html
tcl3270-Resources.html:
	cd ../tcl3270 && $(MAKE) -f Makefile.aux html/Resources.html
wc3270-Resources.html:
	cd ../wc3270 && $(MAKE) -f Makefile.aux html/Resources.html
ws3270-Resources.html:
	cd ../ws3270 && $(MAKE) -f Makefile.aux html/Resources.html
x3270-Resources.html:
	cd ../x3270 && $(MAKE) -f Makefile.aux html/Resources.html

webpage.tgz: $(PAGES) $(OTHERS) mkpages.bash Makefile
	tar -czhf webpage.tgz $(PAGES) $(OTHERS)
