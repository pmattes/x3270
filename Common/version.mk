# Rule to set VERSION_TXT to the path of version.txt.
.PHONY: find-version
find-version: version.txt
	$(eval VERSION_TXT=$^)
# Rule to set DIRNAMES to the path of dirnames.
.PHONY: find-dirnames
find-dirnames: dirnames
	$(eval DIRNAMES=$^)
