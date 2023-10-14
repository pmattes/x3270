#!/usr/bin/env python3
#
# Copyright (c) 2023 Paul Mattes.
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
# SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO,
# PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR PROFITS;
# OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY,
# WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR
# OTHERWISE) ARISING IN ANY WAY OUT OF THE USE OF THIS SOFTWARE, EVEN IF
# ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
#
# x3270 test target host, TELNET protocol definitions.

import enum

class telcmd(enum.IntEnum):
    IAC   = 255,    # interpret as command:
    DONT  = 254,    #  you are not to use option
    DO    = 253,    #  please, you use option
    WONT  = 252,    #  I won't use option
    WILL  = 251,    #  I will use option
    SB    = 250,    #  interpret as subnegotiation
    GA    = 249,    #  you may reverse the line
    EL    = 248,    #  erase the current line
    EC    = 247,  	#  erase the current character
    AYT   = 246,    #  are you there
    AO    = 245,	#  abort output--but let prog finish
    IP    = 244,	#  interrupt process--permanently
    BREAK = 243,	#  break
    DM    = 242,	#  data mark--for connect. cleaning
    NOP   = 241,	#  nop
    SE    = 240,	#  end sub negotiation
    EOR   = 239,	#  end of record (transparent mode)
    SUSP  = 237,	#  suspend process
    xEOF  = 236	    #  end of file

class telopt(enum.IntEnum):
	BINARY   = 0,	# 8-bit data path
	ECHO     = 1,	# echo
	RCP      = 2,	# prepare to reconnect
	SGA      = 3,	# suppress go ahead
	NAMS     = 4,	# approximate message size
	STATUS   = 5,	# give status
	TM       = 6,	# timing mark
	RCTE     = 7,	# remote controlled transmission and echo
	NAOL     = 8,	# negotiate about output line width
	NAOP     = 9,	# negotiate about output page size
	NAOCRD   = 10,	# negotiate about CR disposition
	NAOHTS   = 11,	# negotiate about horizontal tabstops
	NAOHTD   = 12,	# negotiate about horizontal tab disposition
	NAOFFD   = 13,	# negotiate about formfeed disposition
	NAOVTS   = 14,	# negotiate about vertical tab stops
	NAOVTD   = 15,	# negotiate about vertical tab disposition
	NAOLFD   = 16,	# negotiate about output LF disposition
	XASCII   = 17,	# extended ascic character set
	LOGOUT   = 18,	# force logout
	BM       = 19,	# byte macro
	DET      = 20,	# data entry terminal
	SUPDUP   = 21,	# supdup protocol
	SUPDUPOUTPUT = 22,	# supdup output
	SNDLOC   = 23,	# send location
	TTYPE    = 24,	# terminal type
	EOR      = 25,	# end or record
	TUID     = 26,	# TACACS user identification
	OUTMRK   = 27,	# output marking
	TTYLOC   = 28,	# terminal location number
	OLD_3270REGIME = 29,	# old - 3270 regime
	X3PAD    = 30,	# X.3 PAD
	NAWS     = 31,	# window size
	TSPEED   = 32,	# terminal speed
	LFLOW    = 33,	# remote flow control
	LINEMODE = 34,	# linemode option
	XDISPLOC = 35,	# X Display Location
	OLD_ENVIRON = 36,	# old - Environment variables
	AUTHENTICATION = 37,   # authenticate
	ENCRYPT = 38,	# encryption option
	NEW_ENVIRON = 39,	# new - environment variables
	TN3270E = 40,	# extended 3270 regime
	STARTTLS = 46,	# permit TLS negotiation
	EXOPL   = 255	# extended-options-list

class telqual(enum.IntEnum):
    IS   = 0,     # option is...
    SEND = 1,     # please send option
    INFO = 2,     # more info about option

class teltls(enum.IntEnum):
	FOLLOWS = 1,		# TLS negotiation follows

class telobj(enum.IntEnum):	# NEW-ENVIRONMENT qualifiers
	VAR = 0,
	VALUE = 1,
	ESC = 2,
	USERVAR = 3

class tn_state(enum.Enum):
    DATA = enum.auto(),  # receiving data
    IAC  = enum.auto(),  # got an IAC
    WILL = enum.auto(),  # got an IAC WILL
    WONT = enum.auto(),  # got an IAC WONT
    DO   = enum.auto(),  # got an IAC DO
    DONT = enum.auto(),  # got an IAC DONT
    SB   = enum.auto(),  # got an IAC SB
    SB_IAC = enum.auto() # got an IAC after an IAC SB
