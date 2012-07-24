XX_SH(APL Support)
XX_FB(x3270) supports the full XX_SM(APL2) character set and the entry of
XX_SM(APL) characters from the keyboard.
XX_PP
XX_SM(APL) characters are supported only in the special XX_FB(3270) font.
XX_PP
Keyboard entry of XX_SM(APL) characters is supported through the XX_FB(apl)
keymap modifier.
This modifier defines the "Alt" key as an XX_SM(APL) shift key, with a
typical XX_SM(APL) keyboard layout, e.g.,
"Alt" pressed with the XX_FB(A) key results in the XX_SM(APL) "alpha" symbol.
Overstruck characters such as "quad-quote" are not defined as single
keystrokes; instead they are entered as
composites (see XX_LINK(#Composite-Characters,XX_SM(COMPOSITE CHARACTERS))
above).
A special composite map,
XX_FB(apl),
is provided for this purpose.
XX_PP
XX_FB(Note:)
Some keyboards do not define the "Alt" key as a modifier, so keymaps that use
the "Alt" key will not function.
On a Sun for example, this can be remedied with the command:
XX_IP
XX_RS(xmodmap XX_DASH()e "add mod2 = Alt_L")
XX_PP
For convenience, an XX_FB(XX_DASHED(apl))
option is defined, which is an abbreviation for the following resource
definitions:
XX_RS(x3270.keymap: `XX_FI(your_keymap_name),apl'
XX_BR
x3270.charset: apl
XX_BR
x3270.composeMap: apl
XX_BR)
XX_PP
There are a number of XX_SM(APL) characters that are similar in appearance to
non-XX_SM(APL)
characters.
In particular, the XX_SM(APL) "stile", "slope," "tilde" and "quotedot"
characters are similar to the XX_SM(EBCDIC) "bar", "backslash," "tilde"
and "exclaim" characters.
The XX_SM(APL) characters are entered with the "Alt" key, and have slightly
different appearances.
XX_PP
The complete list of special XX_SM(APL) keysyms is as follows.
Entries marked with an asterisk (*) represent
simple aliases for standard XX_SM(EBCDIC) characters.
Entries marked with an (S) represent Sharp APL charatcers.
XX_PP
XX_TS(5,l c l l l.)
XX_TR(XX_TD(APL Symbol)	XX_TD(Hex)	XX_TD(x3270 XX_FI(apl_) Keysym )	XX_TD(x3270 Key)	XX_TD(x3270 Composed Keys))
XX_T_()
XX_TR(XX_TD(A underbar)	XX_TD(41)	XX_TD(Aunderbar)	XX_TD(Alt-A)	XX_TD(A + underbar))
XX_TR(XX_TD(alpha)	XX_TD(B0)	XX_TD(alpha)	XX_TD(Alt-a)	XX_TD(XX_NBSP))
XX_TR(XX_TD(B underbar)	XX_TD(42)	XX_TD(Bunderbar)	XX_TD(Alt-B)	XX_TD(B + underbar))
XX_TR(XX_TD(bar)	XX_TD(60*)	XX_TD(bar)	XX_TD(-)	XX_TD(XX_NBSP))
XX_TR(XX_TD(brace left)	XX_TD(C0)	XX_TD(braceleft)	XX_TD(Alt-{)	XX_TD(XX_NBSP))
XX_TR(XX_TD(brace right)	XX_TD(D0)	XX_TD(braceright)	XX_TD(Alt-})	XX_TD(XX_NBSP))
XX_TR(XX_TD(C underbar)	XX_TD(43)	XX_TD(Cunderbar)	XX_TD(Alt-C)	XX_TD(C + underbar))
XX_TR(XX_TD(circle)	XX_TD(9D)	XX_TD(circle)	XX_TD(Alt-o)	XX_TD(XX_NBSP))
XX_TR(XX_TD(circle bar)	XX_TD(ED)	XX_TD(circlebar)	XX_TD(XX_NBSP)	XX_TD(circle + bar))
XX_TR(XX_TD(circle slope)	XX_TD(CF)	XX_TD(circleslope)	XX_TD(XX_NBSP)	XX_TD(circle + slope))
XX_TR(XX_TD(circle star)	XX_TD(FD)	XX_TD(circlestar)	XX_TD(XX_NBSP)	XX_TD(circle + star))
XX_TR(XX_TD(circle stile)	XX_TD(CD)	XX_TD(circlestile)	XX_TD(XX_NBSP)	XX_TD(circle + stile))
XX_TR(XX_TD(colon)	XX_TD(7A*)	XX_TD(colon)	XX_TD(:)	XX_TD(XX_NBSP))
XX_TR(XX_TD(comma)	XX_TD(6B*)	XX_TD(comma)	`XX_TD(`,')'	XX_TD(XX_NBSP))
XX_TR(XX_TD(comma bar (S))	XX_TD(E5)	XX_TD(commabar)	XX_TD(XX_NBSP)	XX_TD(comma + bar))
XX_TR(XX_TD(D underbar)	XX_TD(44)	XX_TD(Dunderbar)	XX_TD(Alt-D)	XX_TD(D + underbar))
XX_TR(XX_TD(del)	XX_TD(BA)	XX_TD(del)	XX_TD(Alt-g)	XX_TD(XX_NBSP))
XX_TR(XX_TD(del stile)	XX_TD(DC)	XX_TD(delstile)	XX_TD(XX_NBSP)	XX_TD(del + stile))
XX_TR(XX_TD(del tilde)	XX_TD(FB)	XX_TD(deltilde)	XX_TD(XX_NBSP)	XX_TD(del + tilde))
XX_TR(XX_TD(delta)	XX_TD(BB)	XX_TD(delta)	XX_TD(Alt-h)	XX_TD(XX_NBSP))
XX_TR(XX_TD(delta stile)	XX_TD(DD)	XX_TD(deltastile)	XX_TD(XX_NBSP)	XX_TD(delta + stile))
XX_TR(XX_TD(delta underbar)	XX_TD(FC)	XX_TD(deltaunderbar)	XX_TD(XX_NBSP)	XX_TD(delta + underbar))
XX_TR(XX_TD(diamond)	XX_TD(70)	XX_TD(diamond)	XX_TD(XX_NBSP)	XX_TD(up caret + down caret))
XX_TR(XX_TD(dieresis)	XX_TD(72)	XX_TD(dieresis)	XX_TD(Alt-1)	XX_TD(XX_NBSP))
XX_TR(XX_TD(dieresis circle (S))	XX_TD(E5)	XX_TD(XX_HY(dieresis,circle))	XX_TD(XX_NBSP)	XX_TD(dieresis + circle))
XX_TR(XX_TD(dieresis dot)	XX_TD(EC)	XX_TD(dieresisdot)	XX_TD(XX_NBSP)	XX_TD(dieresis + dot))
XX_TR(XX_TD(dieresis jot (S))	XX_TD(E4)	XX_TD(dieresisjot)	XX_TD(XX_NBSP)	XX_TD(dieresis + jot))
XX_TR(XX_TD(divide)	XX_TD(B8)	XX_TD(divide)	XX_TD(Alt-+)	XX_TD(XX_NBSP))
XX_TR(XX_TD(dot)	XX_TD(4B*)	XX_TD(dot)	XX_TD(.)	XX_TD(XX_NBSP))
XX_TR(XX_TD(down arrow)	XX_TD(8B)	XX_TD(downarrow)	XX_TD(Alt-u)	XX_TD(XX_NBSP))
XX_TR(XX_TD(down caret)	XX_TD(78)	XX_TD(downcaret)	XX_TD(Alt-9)	XX_TD(XX_NBSP))
XX_TR(XX_TD(down caret tilde)	XX_TD(CB)	XX_TD(XX_HY(downcaret,tilde))	XX_TD(XX_NBSP)	XX_TD(down caret + tilde))
XX_TR(XX_TD(down shoe)	XX_TD(AB)	XX_TD(downshoe)	XX_TD(Alt-v)	XX_TD(XX_NBSP))
XX_TR(XX_TD(down stile)	XX_TD(8E)	XX_TD(downstile)	XX_TD(Alt-d)	XX_TD(XX_NBSP))
XX_TR(XX_TD(down tack)	XX_TD(AC)	XX_TD(downtack)	XX_TD(Alt-b)	XX_TD(XX_NBSP))
XX_TR(XX_TD(down tack jot)	XX_TD(FE)	XX_TD(downtackjot)	XX_TD(XX_NBSP)	XX_TD(down tack + jot))
XX_TR(XX_TD(down tack up tack)	XX_TD(DA)	XX_TD(XX_HY(downtack,uptack))	XX_TD(XX_NBSP)	XX_TD(down tack + up tack))
XX_TR(XX_TD(E underbar)	XX_TD(45)	XX_TD(Eunderbar)	XX_TD(Alt-E)	XX_TD(E + underbar))
XX_TR(XX_TD(epsilon)	XX_TD(B1)	XX_TD(epsilon)	XX_TD(Alt-e)	XX_TD(XX_NBSP))
XX_TR(XX_TD(epsilon underbar)	XX_TD(75)	XX_TD(XX_HY(epsilon,underbar))	XX_TD(XX_NBSP)	XX_TD(epsilon + underbar))
XX_TR(XX_TD(equal)	XX_TD(7E*)	XX_TD(equal)	XX_TD("=")	XX_TD(XX_NBSP))
XX_TR(XX_TD(equal underbar)	XX_TD(E1)	XX_TD(equalunderbar)	XX_TD(XX_NBSP)	XX_TD(equal + underbar))
XX_TR(XX_TD(euro (S))	XX_TD(E7)	XX_TD(euro)	XX_TD(XX_NBSP)	XX_TD(C + =))
XX_TR(XX_TD(F underbar)	XX_TD(46)	XX_TD(Funderbar)	XX_TD(Alt-F)	XX_TD(F + underbar))
XX_TR(XX_TD(G underbar)	XX_TD(47)	XX_TD(Gunderbar)	XX_TD(Alt-G)	XX_TD(G + underbar))
XX_TR(XX_TD(greater)	XX_TD(6E*)	XX_TD(greater)	XX_TD(>)	XX_TD(XX_NBSP))
XX_TR(XX_TD(H underbar)	XX_TD(48)	XX_TD(Hunderbar)	XX_TD(Alt-H)	XX_TD(H + underbar))
XX_TR(XX_TD(I underbar)	XX_TD(49)	XX_TD(Iunderbar)	XX_TD(Alt-I)	XX_TD(I + underbar))
XX_TR(XX_TD(iota)	XX_TD(B2)	XX_TD(iota)	XX_TD(Alt-i)	XX_TD(XX_NBSP))
XX_TR(XX_TD(iota underbar)	XX_TD(74)	XX_TD(iotaunderbar)	XX_TD(XX_NBSP)	XX_TD(iota + underbar))
XX_TR(XX_TD(J underbar)	XX_TD(51)	XX_TD(Junderbar)	XX_TD(Alt-J)	XX_TD(J + underbar))
XX_TR(XX_TD(jot)	XX_TD(AF)	XX_TD(jot)	XX_TD(alt-j)	XX_TD(XX_NBSP))
XX_TR(XX_TD(K underbar)	XX_TD(52)	XX_TD(Kunderbar)	XX_TD(Alt-K)	XX_TD(K + underbar))
XX_TR(XX_TD(L underbar)	XX_TD(53)	XX_TD(Lunderbar)	XX_TD(Alt-L)	XX_TD(L + underbar))
XX_TR(XX_TD(left arrow)	XX_TD(9F)	XX_TD(leftarrow)	XX_TD(Alt-[)	XX_TD(XX_NBSP))
XX_TR(XX_TD(left bracket)	XX_TD(AD)	XX_TD(leftbracket)	XX_TD([)	XX_TD(XX_NBSP))
XX_TR(XX_TD(left paren)	XX_TD(4D*)	XX_TD(leftparen)	`XX_TD(`(')'	XX_TD(XX_NBSP))
XX_TR(XX_TD(left shoe)	XX_TD(9B)	XX_TD(leftshoe)	XX_TD(Alt-z)	XX_TD(XX_NBSP))
XX_TR(XX_TD(less)	XX_TD(4C*)	XX_TD(less)	XX_TD(XX_LT)	XX_TD(XX_NBSP))
XX_TR(XX_TD(M underbar)	XX_TD(54)	XX_TD(Munderbar)	XX_TD(Alt-M)	XX_TD(M + underbar))
XX_TR(XX_TD(N underbar)	XX_TD(55)	XX_TD(Nunderbar)	XX_TD(Alt-N)	XX_TD(N + underbar))
XX_TR(XX_TD(not equal)	XX_TD(BE)	XX_TD(notequal)	XX_TD(Alt-8)	XX_TD(equal + slash))
XX_TR(XX_TD(not greater)	XX_TD(8C)	XX_TD(notgreater)	XX_TD(Alt-4)	XX_TD(less + equal))
XX_TR(XX_TD(not less)	XX_TD(AE)	XX_TD(notless)	XX_TD(Alt-6)	XX_TD(greater + equal))
XX_TR(XX_TD(O underbar)	XX_TD(56)	XX_TD(Ounderbar)	XX_TD(Alt-O)	XX_TD(O + underbar))
XX_TR(XX_TD(omega)	XX_TD(B4)	XX_TD(omega)	XX_TD(Alt-w)	XX_TD(XX_NBSP))
XX_TR(XX_TD(overbar)	XX_TD(A0)	XX_TD(overbar)	XX_TD(Alt-2)	XX_TD(XX_NBSP))
XX_TR(XX_TD(P underbar)	XX_TD(57)	XX_TD(Punderbar)	XX_TD(Alt-P)	XX_TD(P + underbar))
XX_TR(XX_TD(plus)	XX_TD(4E*)	XX_TD(plus)	XX_TD(+)	XX_TD(XX_NBSP))
XX_TR(XX_TD(Q underbar)	XX_TD(58)	XX_TD(Qunderbar)	XX_TD(Alt-Q)	XX_TD(Q + underbar))
XX_TR(XX_TD(quad)	XX_TD(90)	XX_TD(quad)	XX_TD(Alt-l)	XX_TD(XX_NBSP))
XX_TR(XX_TD(quad divide)	XX_TD(EE)	XX_TD(quaddivide)	XX_TD(XX_NBSP)	XX_TD(quad + divide))
XX_TR(XX_TD(quad jot)	XX_TD(73)	XX_TD(quadjot)	XX_TD(XX_NBSP)	XX_TD(quad + jot))
XX_TR(XX_TD(quad quote)	XX_TD(DE)	XX_TD(quadquote)	XX_TD(XX_NBSP)	XX_TD(quad + quote))
XX_TR(XX_TD(quad slope)	XX_TD(CE)	XX_TD(quadslope)	XX_TD(XX_NBSP)	XX_TD(quad + slope))
XX_TR(XX_TD(query)	XX_TD(6F*)	XX_TD(query)	XX_TD(?)	XX_TD(XX_NBSP))
XX_TR(XX_TD(quote)	XX_TD(7D*)	XX_TD(quote)	XX_TD(')	XX_TD(XX_NBSP))
XX_TR(XX_TD(quote dot)	XX_TD(DB)	XX_TD(quotedot)	XX_TD(XX_NBSP)	XX_TD(quote + dot))
XX_TR(XX_TD(R underbar)	XX_TD(59)	XX_TD(Runderbar)	XX_TD(Alt-R)	XX_TD(R + underbar))
XX_TR(XX_TD(rho)	XX_TD(B3)	XX_TD(rho)	XX_TD(Alt-r)	XX_TD(XX_NBSP))
XX_TR(XX_TD(right arrow)	XX_TD(8F)	XX_TD(rightarrow)	XX_TD(Alt-])	XX_TD(XX_NBSP))
XX_TR(XX_TD(right bracket)	XX_TD(BD)	XX_TD(rightbracket)	XX_TD(])	XX_TD(XX_NBSP))
XX_TR(XX_TD(right paren)	XX_TD(5D*)	XX_TD(rightparen)	`XX_TD(`)')'	XX_TD(XX_NBSP))
XX_TR(XX_TD(right shoe)	XX_TD(9A)	XX_TD(rightshoe)	XX_TD(Alt-x)	XX_TD(XX_NBSP))
XX_TR(XX_TD(S underbar)	XX_TD(62)	XX_TD(Sunderbar)	XX_TD(Alt-S)	XX_TD(S + underbar))
XX_TR(XX_TD(semicolon)	XX_TD(5E*)	XX_TD(semicolon)	XX_TD(;)	XX_TD(XX_NBSP))
XX_TR(XX_TD(slash)	XX_TD(61*)	XX_TD(slash)	XX_TD(/)	XX_TD(XX_NBSP))
XX_TR(XX_TD(slash bar)	XX_TD(EA)	XX_TD(slashbar)	XX_TD(XX_NBSP)	XX_TD(slash + bar))
XX_TR(XX_TD(slope)	XX_TD(B7)	XX_TD(slope)	XX_TD(Alt-XX_BS)	XX_TD(XX_NBSP))
XX_TR(XX_TD(slope bar)	XX_TD(EB)	XX_TD(slopebar)	XX_TD(XX_NBSP)	XX_TD(slope + bar))
XX_TR(XX_TD(squad)	XX_TD(CC)	XX_TD(squad)	XX_TD(XX_NBSP)	XX_TD(quad + quad))
XX_TR(XX_TD(star)	XX_TD(5C*)	XX_TD(star)	XX_TD(*)	XX_TD(XX_NBSP))
XX_TR(XX_TD(stile)	XX_TD(BF)	XX_TD(stile)	XX_TD(Alt-|)	XX_TD(XX_NBSP))
XX_TR(XX_TD(T underbar)	XX_TD(63)	XX_TD(Tunderbar)	XX_TD(Alt-T)	XX_TD(T + underbar))
XX_TR(XX_TD(tilde)	XX_TD(80)	XX_TD(tilde)	XX_TD(Alt-~)	XX_TD(XX_NBSP))
XX_TR(XX_TD(times)	XX_TD(B6)	XX_TD(times)	XX_TD(Alt-=)	XX_TD(XX_NBSP))
XX_TR(XX_TD(U underbar)	XX_TD(64)	XX_TD(Uunderbar)	XX_TD(Alt-U)	XX_TD(U + underbar))
XX_TR(XX_TD(underbar)	XX_TD(6D*)	XX_TD(underbar)	XX_TD("_")	XX_TD(XX_NBSP))
XX_TR(XX_TD(up arrow)	XX_TD(8A)	XX_TD(uparrow)	XX_TD(Alt-y)	XX_TD(XX_NBSP))
XX_TR(XX_TD(up caret)	XX_TD(71)	XX_TD(upcaret)	XX_TD(Alt-0)	XX_TD(XX_NBSP))
XX_TR(XX_TD(up caret tilde)	XX_TD(CA)	XX_TD(upcarettilde)	XX_TD(XX_NBSP)	XX_TD(up caret + tilde))
XX_TR(XX_TD(up shoe)	XX_TD(AA)	XX_TD(upshoe)	XX_TD(Alt-c)	XX_TD(XX_NBSP))
XX_TR(XX_TD(up shoe jot)	XX_TD(DF)	XX_TD(upshoejot)	XX_TD(XX_NBSP)	XX_TD(up shoe + jot))
XX_TR(XX_TD(up stile)	XX_TD(8D)	XX_TD(upstile)	XX_TD(Alt-s)	XX_TD(XX_NBSP))
XX_TR(XX_TD(up tack)	XX_TD(BC)	XX_TD(uptack)	XX_TD(Alt-n)	XX_TD(XX_NBSP))
XX_TR(XX_TD(up tack jot)	XX_TD(EF)	XX_TD(uptackjot)	XX_TD(XX_NBSP)	XX_TD(up tack + jot))
XX_TR(XX_TD(V underbar)	XX_TD(65)	XX_TD(Vunderbar)	XX_TD(Alt-V)	XX_TD(V + underbar))
XX_TR(XX_TD(W underbar)	XX_TD(66)	XX_TD(Wunderbar)	XX_TD(Alt-W)	XX_TD(W + underbar))
XX_TR(XX_TD(X underbar)	XX_TD(67)	XX_TD(Xunderbar)	XX_TD(Alt-X)	XX_TD(X + underbar))
XX_TR(XX_TD(Y underbar)	XX_TD(68)	XX_TD(Yunderbar)	XX_TD(Alt-Y)	XX_TD(Y + underbar))
XX_TR(XX_TD(Z underbar)	XX_TD(69)	XX_TD(Zunderbar)	XX_TD(Alt-Z)	XX_TD(Z + underbar))
XX_TE()
XX_SH(XIM Support)
When compiled with DBCS support, XX_FB(x3270) supports multi-byte input methods
via the XIM protocol.
XX_LP
The input method is selected by the XMODIFIERS environment variable or the
XX_FB(XX_DASHED(im)) command-line option.
XX_LP
The preedit type is specified by the XX_FB(XX_DASHED(pt)) command-line option,
with a default of XX_FB(OverTheSpot+1).
