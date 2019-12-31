#!/usr/bin/env python
# Python version of sb3270 (s3270 clone using b3270)

from xml.parsers import expat
import shlex, subprocess
import sys
import argparse
import os
import codecs

# Process command-line arguments.
parser = argparse.ArgumentParser()
parser.add_argument("-debug",
        help="display debug information",
        action="store_true")
parser.add_argument("-trace",
        help="trace b3270",
        action="store_true")
args = parser.parse_args()

# State class. Keeps track of the emulator state, produces the s3270 prompt.
class State:
    def __init__(self):
        # Keyboard lock flag
        self.keyboardLock = False
        # Hostname
        self.host = ""
        # Connection state
        self.state = ""
        # Model number
        self.model = ""
        # Logical rows
        self.rows = 0
        # Logical columns
        self.cols = 0
        # Formatted state
        self.formatted = False
        # Cursor row and column
        self.row = 0
        self.col = 0

    # Prompt method
    def Prompt(self,time):
        s=""
        # 1 keyboard
        if (self.keyboardLock):
            s = s + "L "
        else:
            s = s + "U "
        # 2 formatted (not yet)
        if (self.formatted):
            s = s + "F "
        else:
            s = s + "U "
        # 3 protected (not possible, fake it)
        s = s + "U "
        # 4 connection
        if (self.host != ""):
            s = s + "C(" + self.host + ") "
        else:
            s = s + "N "
        # 5 state
        if (self.host != "" and self.state != ""):
            if ("nvt" in self.state):
                if ("char" in self.state):
                    s = s + "C "
                else:
                    s = s + "L "
            else:
                if ("3270" in self.state):
                    s = s + "I "
                else:
                    s = s + "P "
        else:
            s = s + "N "
        # 6 model
        s = s + self.model[9] + " "
        # 7 rows
        # 8 cols
        s = s + "{} {} ".format(self.rows, self.cols)
        # 9 cursor row (0-origin)
        # 10 cursor col (0-origin)
        s = s + "{} {} ".format(self.row, self.col)
        # 11 window number (0x0)
        s = s + "0x0 "
        # 12 timing
        s = s + time;
        return s

# Debug output method, probably does not work on Windows.
def Debug(text):
    global args
    if (args.debug):
        print("[33m" + text + "[0m")

# XML parser class
class Parser:

    # Constructor.
    def __init__(self, state):
        # Set up the expat object and handlers.
        self._parser = expat.ParserCreate("UTF-8")
        self._parser.StartElementHandler = self.start
        self._parser.EndElementHandler = self.end
        self._parser.CharacterDataHandler = self.data
        # Not time to read from stdin yet.
        self.canRead = False
        # Remember the state object.
        self.state = state

    # Check and reset the canRead flag.
    def CanRead(self):
        if (self.canRead):
            self.canRead = False
            return True
        else:
            return False

    # Push a line of input into the expat parser.
    def feed(self, data):
        self._parser.Parse(data, 0)

    # Clean up.
    def close(self):
        self._parser.Parse("", 1) # end of data
        del self._parser # get rid of circular references

    # Process the start of an element. This is where most of the logic is.
    def start(self, tag, attrs):
        Debug("START " + repr(tag) + " " + repr(attrs))
        if (tag == "ready"):
            self.canRead = True
        if (tag == "run-result"):
            if ("text" in attrs):
                for line in attrs["text"].splitlines():
                    print("data: " + line)
            print(self.state.Prompt(attrs["time"]))
            if (attrs["success"] == "true"):
                print("ok")
            else:
                print("error")
            self.canRead = True
        if (tag == "oia"):
            # The only thing we care about in the OIA is the keyboard lock,
            # and we don't care what it is.
            if ("field" in attrs and attrs["field"] == "lock"):
                self.state.keyboardLock = "value" in attrs

        if (tag == "connection"):
            # Process the connection state. All we need is the host name, or
            # lack thereof.
            self.state.host = ""
            if ("host" in attrs):
                self.state.host = attrs["host"]
            self.state.state = ""
            if ("state" in attrs):
                self.state.state = attrs["state"]

        if (tag == "terminal-name"):
            # Process the model number.
            self.state.model = attrs["text"]

        if (tag == "erase"):
            if ("logical-rows" in attrs):
                self.state.rows = attrs["logical-rows"]
            if ("logical-cols" in attrs):
                self.state.cols = attrs["logical-cols"]

        if (tag == "formatted"):
            # Process the formatted indication.
            self.state.formatted = not ("state" in attrs and attrs["state"] == "false");

        if (tag == "cursor" and "row" in attrs):
            # Process the cursor address.
            self.state.row = int(attrs["row"]) - 1
            self.state.col = int(attrs["column"]) - 1

    # Process the end of an element.
    def end(self, tag):
        Debug("END " + repr(tag))
	if (tag == "initialize"):
            self.canRead = True

    # Process inter-element data.
    def data(self, data):
        Debug("DATA " + repr(data))

# Render a string safe for XML 1.0
def XmlSafe(s):
    r = ""
    named = {
        '&': "amp",
        '"': "quot",
        "'": "apos",
        '<': "lt",
        '>': "gt",
    }
    numerics = "\x09\x0a\x0d";
    for c in s:
        if (c in named):
            r = r + "&" + named[c] + ";"
        elif (c in numerics):
            r = r + "&#" + str(ord(c)) + ";"
        elif (ord(c) < 32
                or ord(c) == 0x7f
                or (ord(c) >= 0x86 and ord(c) <= 0x9f)):
                r = r + " "
        else:
            r = r + c
    return r

# Create the state.
s = State()

# Create the parser.
p = Parser(s)

# Create the b3270 process.
b3270 = "b3270"
if (args.trace):
    command = [b3270,"-trace"]
else:
    command = [b3270]
command += ["-xrm","*unlockDelay: false","-xrm","*oerrLock: true"]
Debug("Command line is: " + repr(command))
b = subprocess.Popen(command, stdin=subprocess.PIPE, stdout=subprocess.PIPE)
b.stdin.write(b'<b3270-in>\n')

# Select from stdin and the parser until something breaks.
while (True):
    if (p.CanRead()):
        Debug("Reading from stdin")
        text = sys.stdin.readline()
        Debug("Got " + repr(text))
        if (text == ""):
            Debug("stdin EOF")
            b.stdin.write(b'</b3270-in>\n')
            break
        text = XmlSafe(text.rstrip('\r\n'))
        Debug("Expanded text is " + repr(text))
        if (sys.version_info.major >= 3):
            text = bytes(text, "UTF-8")
        b.stdin.write(b'<run actions="' + text + b'"/>\n')
        b.stdin.flush()
    Debug("Reading from b3270")
    line = b.stdout.readline()
    if (line == ""):
        Debug("b3270 EOF")
        p.close()
        break
    Debug("Got " + repr(line))
    p.feed(line)

# Clean up.
b.terminate()
