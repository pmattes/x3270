Copyrights
 Whenever a file with a copyright message at the top is modified, apply the following rules:
 - Update it to use the current year.
 - If the file has multiple date ranges, such as "Copyright 2000-2010, 2013-2014 ...", change the format to a single range, starting with the earliest year listed and ending with the current year.
 - If there is a comma immediately after the year or a range of years, remove it.
Comment headers
 - All new C functions need to have Doxygen-style comment headers.
 - All new Python functions need to have a simple comment header, which can just be a single line '#' comment.
Running ./configure and building
 - Before committing a change, Tools/config-both and Tools/make-both should be run to ensure that both Linux and Windows builds still work.
Code formatting and C standards
 - The code was originally written with a strict 80-column line size. This has since been relaxed to 132 columns.
 - Use of C99 features is permitted, including inline declarations.
Unit testing
 - Added or changed code should have as close to 100% unit test code coverage as possible. This is enforced informally at the moment, by periodically running gcov on the suite of unit tests. Gcov does not need to be run with every commit.
