XX_SH(File Transfer)
The XX_FB(Transfer) XX_action implements XX_FB(IND$FILE) file transfer.
This XX_action requires that the XX_FB(IND$FILE)
program be installed on the XX_SM(IBM) host, and that the 3270 cursor
be located in a field that will accept a XX_SM(TSO) or XX_SM(VM/CMS) command.
XX_LP
ifelse(XX_PRODUCT,x3270,,XX_PRODUCT,s3270,,XX_PRODUCT,tcl3270,,`The XX_FB(Transfer) XX_action
can be entered at the command prompt with no parameters, which will cause it
to prompt interactively for the file names and options.
It can also be invoked with parameters to define the entire transfer.
')dnl
XX_LP
Because of the complexity and number of options for file transfer, the
parameters to the XX_FB(Transfer) XX_action take the unique form
of XX_FI(option)=XX_FI(value), and can appear in any order.
Note that if the XX_FI(value) contains spaces (such as a VM/CMS file name),
then the entire parameter must be quoted, e.g., "HostFile=xxx foo a".
The options are:
XX_LP
XX_TS(4,l c l l.)
XX_TR(XX_TD(Option)	XX_TD(Required?)	XX_TD(Default)	XX_TD(Other Values))
XX_T_()
XX_TR(XX_TD(Direction)	XX_TD(No)	XX_TD(receive)	XX_TD(send))
XX_TR(XX_TD(HostFile)	XX_TD(Yes)	XX_TD(XX_NBSP)	XX_TD(XX_NBSP))
XX_TR(XX_TD(LocalFile)	XX_TD(Yes)	XX_TD(XX_NBSP)	XX_TD(XX_NBSP))
XX_TR(XX_TD(Host)	XX_TD(No)	XX_TD(tso)	`XX_TD(`vm, cics')')
XX_TR(XX_TD(Mode)	XX_TD(No)	XX_TD(ascii)	XX_TD(binary))
XX_TR(XX_TD(Cr)	XX_TD(No)	XX_TD(remove)	`XX_TD(`add, keep')')
XX_TR(XX_TD(Remap)	XX_TD(No)	XX_TD(yes)	XX_TD(no))
XX_TR(XX_TD(Exist)	XX_TD(No)	XX_TD(keep)	`XX_TD(`replace, append')')
XX_TR(XX_TD(Recfm)	XX_TD(No)	XX_TD(XX_NBSP)	`XX_TD(`fixed, variable, undefined')')
XX_TR(XX_TD(Lrecl)	XX_TD(No)	XX_TD(XX_NBSP)	XX_TD(XX_NBSP))
XX_TR(XX_TD(Blksize)	XX_TD(No)	XX_TD(XX_NBSP)	XX_TD(XX_NBSP))
XX_TR(XX_TD(Allocation)	XX_TD(No)	XX_TD(XX_NBSP)	`XX_TD(`tracks, cylinders, avblock')')
XX_TR(XX_TD(PrimarySpace)	XX_TD(Sometimes)	XX_TD(XX_NBSP)	XX_TD(XX_NBSP))
XX_TR(XX_TD(SecondarySpace)	XX_TD(No)	XX_TD(XX_NBSP)	XX_TD(XX_NBSP))
XX_TR(XX_TD(Avblock)	XX_TD(Sometimes)	XX_TD(XX_NBSP)	XX_TD(XX_NBSP))
XX_TR(XX_TD(BufferSize)	XX_TD(No)	XX_TD(4096)	XX_TD(XX_NBSP))
XX_TE()
XX_LP
The option details are as follows.
XX_TPS()dnl
XX_TP(XX_FB(Direction))
XX_FB(send) to send a file to the host,
XX_FB(receive) to receive a file from the host.
XX_TP(XX_FB(HostFile))
The name of the file on the host.
XX_TP(XX_FB(LocalFile))
The name of the file on the local workstation.
XX_TP(XX_FB(Host))
The type of host (which dictates the form of the XX_FB(IND$FILE) command):
XX_FB(tso) (the default), XX_FB(vm) or XX_FB(cics).
XX_TP(XX_FB(Mode))
Use XX_FB(ascii) (the default) for a text file, which will be translated
between XX_SM(EBCDIC) and XX_SM(ASCII) as necessary.
Use XX_FB(binary) for non-text files.
XX_TP(XX_FB(Cr))
Controls how XX_FB(Newline) characters are handled when transferring
XX_FB(Mode=ascii) files.
XX_FB(remove) (the default) strips XX_FB(Newline) characters in local files
before transferring them to the host.
XX_FB(add) adds XX_FB(Newline) characters to each host file record before
transferring it to the local workstation.
XX_FB(keep) preserves XX_FB(Newline) characters when transferring a local file
to the host.
XX_TP(XX_FB(Remap))
Controls text translation for XX_FB(Mode=ascii) files.
The value XX_FB(yes) (the default) causes XX_PRODUCT to remap the text to ensure
maximum compatibility between the workstation's character set and encoding
and the host's XX_SM(EBCDIC) code page.
The value XX_FB(no) causes XX_PRODUCT to pass the text to or from the host
as-is, leaving all translation to the XX_FB(IND$FILE) program on the host.
XX_TP(XX_FB(Exist))
Controls what happens when the destination file already exists.
XX_FB(keep) (the default) preserves the file, causing the
XX_FB(Transfer) XX_action to fail.
XX_FB(replace) overwrites the destination file with the source file.
XX_FB(append) appends the source file to the destination file.
XX_TP(XX_FB(Recfm))
Controls the record format of files created on the host.
(XX_SM(TSO) and XX_SM(VM) hosts only.)
XX_FB(fixed) creates a file with fixed-length records.
XX_FB(variable) creates a file with variable-length records.
XX_FB(undefined) creates a file with undefined-length records (XX_SM(TSO) hosts
only).
The XX_FB(Lrecl) option controls the record length or maximum record length for
XX_FB(Recfm=fixed) and XX_FB(Recfm=variable) files, respectively.
XX_TP(XX_FB(Lrecl))
Specifies the record length (or maximum record length) for files created on
the host.
(XX_SM(TSO) and XX_SM(VM) hosts only.)
XX_TP(XX_FB(Blksize))
Specifies the block size for files created on the host.
(XX_SM(TSO) and XX_SM(VM) hosts only.)
XX_TP(XX_FB(Allocation))
Specifies the units for the XX_FB(PrimarySpace) and
XX_FB(SecondarySpace) options: XX_FB(tracks), XX_FB(cylinders) or
XX_FB(avblock). (XX_SM(TSO) hosts only.)
XX_TP(XX_FB(PrimarySpace))
Primary allocation for a file.
The units are given by the XX_FB(Allocation) option.
Required when the XX_FB(Allocation) is specified as something other than
XX_FB(default).
(XX_SM(TSO) hosts only.)
XX_TP(XX_FB(SecondarySpace))
Secondary allocation for a file.
The units are given by the XX_FB(Allocation) option. (XX_SM(TSO) hosts only.) 
XX_TP(XX_FB(Avblock))
Average block size, required when XX_FB(Allocation) specifies XX_FB(avblock).
(XX_SM(TSO) hosts only.)
XX_TP(XX_FB(BufferSize))
Buffer size for DFT-mode transfers.
Can range from 256 to 32768.
Larger values give better performance, but some hosts may not be able to
support them.
ifelse(XX_PLATFORM,windows,`XX_TP(XX_FB(WindowsCodePage))
Overrides the workstation code page used for XX_SM(ASCII) file transfers.
By default, the system XX_SM(ANSI) code page is used.
')dnl
XX_TPE()dnl
XX_LP
There are also resources that control the default values for each of the
file transfer parameters.
These resources have the same names as the XX_FB(Transfer) keywords, but with
XX_FB(ft) prepended. E.g., the default for the XX_FB(Mode) keyword is the
XX_FB(XX_PRODUCT.ftMode) resource.
