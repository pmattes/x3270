PAGES=credits.html documentation.html documentation-bugs.html \
	documentation-faq.html documentation-manpages.html \
	documentation-misc.html documentation-other.html \
	documentation-relnotes.html download.html index.html license.html \
	screenshots.html x026.html
SCREENS=c3270.png c3270-thumb.png cyrillic.png cyrillic-thumb.png \
	japanese.png japanese-thumb.png keypad.png keypad-thumb.png \
	x3270_main.png x3270_main-thumb.png wc3270.png wc3270-thumb.png
#SCREENS=c3270.jpg c3270-thumb.jpg cyrillic.jpg cyrillic-thumb.jpg \
#	japanese.jpg japanese-thumb.jpg keypad.jpg keypad-thumb.jpg \
#	x3270_main.jpg x3270_main-thumb.jpg
OTHERS=Brackets.html Build-c3270.html Build-pr3287.html Build-s3270.html \
	Build-tcl3270.html Build-wc3270.html Build-x3270.html \
	c3270Keymap.html c3270-man.html \
	Charset.html ibm_hosts.html Keymap.html \
	pr3287-man.html Resources.html s3270-man.html \
	tcl3270-man.html Wishlist.html x3270if.html x3270-man.html \
	wc3270-man.html wc3270Keymap.html x3270-script.html del.jpg \
	delta.jpg x3270.jpg x026.gif styles.css HotBugs.html \
	man/FAQ.html $(SCREENS)

all: $(PAGES) geo/index.html webpage.tgz

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

# Special rule for the GeoCities index, which is live, but links to bgp.nu.
geo/index.html: mkpages.bash index-body.html
	./mkpages.bash -g index geo/index.html

webpage.tgz: $(PAGES) $(OTHERS) mkpages.bash Makefile
	tar -czhf webpage.tgz $(PAGES) $(OTHERS)
