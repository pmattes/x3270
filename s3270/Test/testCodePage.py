#!/usr/bin/env python3
#
# Copyright (c) 2021-2025 Paul Mattes.
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
# s3270 code page tests

from subprocess import Popen, PIPE, DEVNULL
import unittest

from Common.Test.cti import *
from Common.Test.playback import playback

@requests_timeout
class TestS3270CodePage(cti):

    # s3270 SBCS code page test.
    def test_s3270_sbcs_code_page(self):

        # Start playback.
        pport, ts = unused_port()
        with playback(self, 's3270/Test/all_chars.trc', port=pport) as p:
            ts.close()

            # Start s3270.
            sport, ts = unused_port()
            s3270 = Popen(vgwrap(['s3270', '-httpd', str(sport), '-utf8', f'127.0.0.1:{pport}']))
            self.children.append(s3270)
            self.check_listen(sport)
            ts.close()

            cp_all_map = {
                'cp037': [' \xa0âäàáãåçñ¢.<(+|', '&éêëèíîïìß!$*);¬', '-/ÂÄÀÁÃÅÇÑ¦,%_>?',
                        'øÉÊËÈÍÎÏÌ`:#@\'="', 'Øabcdefghi«»ðýþ±', '°jklmnopqrªºæ¸Æ¤',
                        'µ~stuvwxyz¡¿ÐÝÞ®', '^£¥·©§¶¼½¾[]¯¨´×', '{ABCDEFGHI\xadôöòóõ',
                        '}JKLMNOPQR¹ûüùúÿ', '\\÷STUVWXYZ²ÔÖÒÓÕ', '0123456789³ÛÜÙÚ●'],
                'cp273': [' \xa0â{àáãåçñÄ.<(+!', '&éêëèíîïì~Ü$*);^', '-/Â[ÀÁÃÅÇÑö,%_>?',
                        'øÉÊËÈÍÎÏÌ`:#§\'="', 'Øabcdefghi«»ðýþ±', '°jklmnopqrªºæ¸Æ¤',
                        'µßstuvwxyz¡¿ÐÝÞ®', '¢£¥·©@¶¼½¾¬|¯¨´×', 'äABCDEFGHI\xadô¦òóõ',
                        'üJKLMNOPQR¹û}ùúÿ', 'Ö÷STUVWXYZ²Ô\\ÒÓÕ', '0123456789³Û]ÙÚ●'],
                'cp275': [' \xa0        É.<(+!', '&         $Ç*);^', '-/        ç,%_>?',
                        '         ã:ÕÃ\'="', ' abcdefghi      ', ' jklmnopqr      ',
                        ' ~stuvwxyz      ', '                ', 'õABCDEFGHI      ',
                        'éJKLMNOPQR      ', '\\ STUVWXYZ      ', '0123456789     ●'],
                'cp277': [' \xa0âäàáã}çñ#.<(+!', '&éêëèíîïìß¤Å*);^', '-/ÂÄÀÁÃ$ÇÑø,%_>?',
                        '¦ÉÊËÈÍÎÏÌ`:ÆØ\'="', '@abcdefghi«»ðýþ±', '°jklmnopqrªº{¸[]',
                        'µüstuvwxyz¡¿ÐÝÞ®', '¢£¥·©§¶¼½¾¬|¯¨´×', 'æABCDEFGHI\xadôöòóõ',
                        'åJKLMNOPQR¹û~ùúÿ', '\\÷STUVWXYZ²ÔÖÒÓÕ', '0123456789³ÛÜÙÚ●'],
                'cp278': [' \xa0â{àáã}çñ§.<(+!', '&`êëèíîïìß¤Å*);^', '-/Â#ÀÁÃ$ÇÑö,%_>?',
                        'øÉÊËÈÍÎÏÌé:ÄÖ\'="', 'Øabcdefghi«»ðýþ±', '°jklmnopqrªºæ¸Æ]',
                        'µüstuvwxyz¡¿ÐÝÞ®', '¢£¥·©[¶¼½¾¬|¯¨´×', 'äABCDEFGHI\xadô¦òóõ',
                        'åJKLMNOPQR¹û~ùúÿ', '\\÷STUVWXYZ²Ô@ÒÓÕ', '0123456789³ÛÜÙÚ●'],
                'cp280': [' \xa0âä{áãå\\ñ°.<(+!', '&]êë}íîï~ßé$*);^', '-/ÂÄÀÁÃÅÇÑò,%_>?',
                        'øÉÊËÈÍÎÏÌù:£§\'="', 'Øabcdefghi«»ðýþ±', '[jklmnopqrªºæ¸Æ¤',
                        'µìstuvwxyz¡¿ÐÝÞ®', '¢#¥·©@¶¼½¾¬|¯¨´×', 'àABCDEFGHI\xadôö¦óõ',
                        'èJKLMNOPQR¹ûü`úÿ', 'ç÷STUVWXYZ²ÔÖÒÓÕ', '0123456789³ÛÜÙÚ●'],
                'cp284': [' \xa0âäàáãåç¦[.<(+|', '&éêëèíîïìß]$*);¬', '-/ÂÄÀÁÃÅÇ#ñ,%_>?',
                        'øÉÊËÈÍÎÏÌ`:Ñ@\'="', 'Øabcdefghi«»ðýþ±', '°jklmnopqrªºæ¸Æ¤',
                        'µ¨stuvwxyz¡¿ÐÝÞ®', '¢£¥·©§¶¼½¾^!¯~´×', '{ABCDEFGHI\xadôöòóõ',
                        '}JKLMNOPQR¹ûüùúÿ', '\\÷STUVWXYZ²ÔÖÒÓÕ', '0123456789³ÛÜÙÚ●'],
                'cp285': [' \xa0âäàáãåçñ$.<(+|', '&éêëèíîïìß!£*);¬', '-/ÂÄÀÁÃÅÇÑ¦,%_>?',
                        'øÉÊËÈÍÎÏÌ`:#@\'="', 'Øabcdefghi«»ðýþ±', '°jklmnopqrªºæ¸Æ¤',
                        'µ¯stuvwxyz¡¿ÐÝÞ®', '¢[¥·©§¶¼½¾^]~¨´×', '{ABCDEFGHI\xadôöòóõ',
                        '}JKLMNOPQR¹ûüùúÿ', '\\÷STUVWXYZ²ÔÖÒÓÕ', '0123456789³ÛÜÙÚ●'],
                'cp297': [' \xa0âä@áãå\\ñ°.<(+!', '&{êë}íîïìß§$*);^', '-/ÂÄÀÁÃÅÇÑù,%_>?',
                        'øÉÊËÈÍÎÏÌµ:£à\'="', 'Øabcdefghi«»ðýþ±', '[jklmnopqrªºæ¸Æ¤',
                        '`¨stuvwxyz¡¿ÐÝÞ®', '¢#¥·©]¶¼½¾¬|¯~´×', 'éABCDEFGHI\xadôöòóõ',
                        'èJKLMNOPQR¹ûü¦úÿ', 'ç÷STUVWXYZ²ÔÖÒÓÕ', '0123456789³ÛÜÙÚ●'],
                'cp424': [' אבגדהוזחט¢.<(+|', '&יךכלםמןנס!$*);¬', '-/עףפץצקרש¦,%_>?', ' ת  \xa0   ⇔`:#@\'="', ' abcdefghi«»    ', '°jklmnopqr   ¸ ¤', 'µ~stuvwxyz     ®', '^£¥·©§¶¼½¾[]¯¨´×', '{ABCDEFGHI\xad     ', '}JKLMNOPQR¹     ', '\\÷STUVWXYZ²     ', '0123456789³    ●'],
                'cp500': [' \xa0âäàáãåçñ[.<(+!', '&éêëèíîïìß]$*);^', '-/ÂÄÀÁÃÅÇÑ¦,%_>?',
                        'øÉÊËÈÍÎÏÌ`:#@\'="', 'Øabcdefghi«»ðýþ±', '°jklmnopqrªºæ¸Æ¤',
                        'µ~stuvwxyz¡¿ÐÝÞ®', '¢£¥·©§¶¼½¾¬|¯¨´×', '{ABCDEFGHI\xadôöòóõ',
                        '}JKLMNOPQR¹ûüùúÿ', '\\÷STUVWXYZ²ÔÖÒÓÕ', '0123456789³ÛÜÙÚ●'],
                'cp803': ['          $.<(+|', 'א         !¢*);¬', '-/         ,%_>?', '          :#@\'="', ' בגדהוזחטי      ', ' ךכלםמןנסע      ', '  ףפץצקרשת      ', '                ', ' ABCDEFGHI      ', ' JKLMNOPQR      ', '  STUVWXYZ      ', '0123456789     ●'],
                'cp870': [' \xa0âäţáăčçć[.<(+!', '&éęëůíîľĺß]$*);^', '-/ÂÄ˝ÁĂČÇĆ|,%_>?',
                        'ˇÉĘËŮÍÎĽĹ`:#@\'="', '˘abcdefghiśňđýřş', '°jklmnopqrłńš¸˛¤',
                        'ą~stuvwxyzŚŇĐÝŘŞ', '·ĄżŢŻ§žźŽŹŁŃŠ¨´×', '{ABCDEFGHI\xadôöŕóő',
                        '}JKLMNOPQRĚűüťúě', '\\÷STUVWXYZďÔÖŔÓŐ', '0123456789ĎŰÜŤÚ●'],
                'cp871': [' \xa0âäàáãåçñþ.<(+!', '&éêëèíîïìßÆ$*);Ö', '-/ÂÄÀÁÃÅÇÑ¦,%_>?',
                        'øÉÊËÈÍÎÏÌð:#Ð\'="', 'Øabcdefghi«»`ý{±', '°jklmnopqrªº}¸]¤',
                        'µöstuvwxyz¡¿@Ý[®', '¢£¥·©§¶¼½¾¬|¯¨\\×', 'ÞABCDEFGHI\xadô~òóõ',
                        'æJKLMNOPQR¹ûüùúÿ', '´÷STUVWXYZ²Ô^ÒÓÕ', '0123456789³ÛÜÙÚ●'],
                'cp875': [' ΑΒΓΔΕΖΗΘΙ[.<(+!', '&ΚΛΜΝΞΟΠΡΣ]$*);^', '-/ΤΥΦΧΨΩΪΫ ,%_>?',
                        '¨ΆΈΉ∇ΊΌΎΏ`:#@\'="', '΅abcdefghiαβγδεζ', '°jklmnopqrηθικλμ',
                        '´~stuvwxyzνξοπρσ', '£άέήΐίόύΰώςτυφχψ', '{ABCDEFGHI\xadωϊϋ‘―',
                        '}JKLMNOPQR±½ ·’¦', '\\ STUVWXYZ²§  «¬', '0123456789³©  »●'],
                'cp880': [' \xa0ђѓё ѕіїј[.<(+!', '&љњћќ џЪ№Ђ]$*);^', '-/ЃЁ ЅІЇЈЉ¦,%_>?',
                        'ЊЋЌ  Џюаб :#@\'="', 'цabcdefghiдефгхи', 'йjklmnopqrклмноп',
                        'я stuvwxyzрстужв', 'ьызшэщчъЮАБЦДЕФГ', ' ABCDEFGHIХИЙКЛМ',
                        ' JKLMNOPQRНОПЯРС', '\\¤STUVWXYZТУЖВЬЫ', '0123456789ЗШЭЩЧ●'],
                'cp1026': [' \xa0âäàáãå{ñÇ.<(+!', '&éêëèíîïìßĞİ*);^', '-/ÂÄÀÁÃÅ[Ñş,%_>?',
                        "øÉÊËÈÍÎÏÌı:ÖŞ'=Ü", 'Øabcdefghi«»}`¦±', '°jklmnopqrªºæ˛Æ¤',
                        'µöstuvwxyz¡¿]$@®', '¢£¥·©§¶¼½¾¬|—¨´×', 'çABCDEFGHI\xadô~òóõ',
                        'ğJKLMNOPQR¹û\\ùúÿ', 'ü÷STUVWXYZ²Ô#ÒÓÕ', '0123456789³Û"ÙÚ●'],
                'cp1047': [' \xa0âäàáãåçñ¢.<(+|', '&éêëèíîïìß!$*);^', '-/ÂÄÀÁÃÅÇÑ¦,%_>?',
                        'øÉÊËÈÍÎÏÌ`:#@\'="', 'Øabcdefghi«»ðýþ±', '°jklmnopqrªºæ¸Æ¤',
                        'µ~stuvwxyz¡¿Ð[Þ®', '¬£¥·©§¶¼½¾Ý¨¯]´×', '{ABCDEFGHI\xadôöòóõ',
                        '}JKLMNOPQR¹ûüùúÿ', '\\÷STUVWXYZ²ÔÖÒÓÕ', '0123456789³ÛÜÙÚ●'],
                'cp1140': [' \xa0âäàáãåçñ¢.<(+|', '&éêëèíîïìß!$*);¬', '-/ÂÄÀÁÃÅÇÑ¦,%_>?',
                        'øÉÊËÈÍÎÏÌ`:#@\'="', 'Øabcdefghi«»ðýþ±', '°jklmnopqrªºæ¸Æ€',
                        'µ~stuvwxyz¡¿ÐÝÞ®', '^£¥·©§¶¼½¾[]¯¨´×', '{ABCDEFGHI\xadôöòóõ',
                        '}JKLMNOPQR¹ûüùúÿ', '\\÷STUVWXYZ²ÔÖÒÓÕ', '0123456789³ÛÜÙÚ●'],
                'cp1141': [' \xa0â{àáãåçñÄ.<(+!', '&éêëèíîïì~Ü$*);^', '-/Â[ÀÁÃÅÇÑö,%_>?',
                        'øÉÊËÈÍÎÏÌ`:#§\'="', 'Øabcdefghi«»ðýþ±', '°jklmnopqrªºæ¸Æ€',
                        'µßstuvwxyz¡¿ÐÝÞ®', '¢£¥·©@¶¼½¾¬|¯¨´×', 'äABCDEFGHI\xadô¦òóõ',
                        'üJKLMNOPQR¹û}ùúÿ', 'Ö÷STUVWXYZ²Ô\\ÒÓÕ', '0123456789³Û]ÙÚ●'],
                'cp1142': [' \xa0âäàáã}çñ#.<(+!', '&éêëèíîïìß€Å*);^', '-/ÂÄÀÁÃ$ÇÑø,%_>?',
                        '¦ÉÊËÈÍÎÏÌ`:ÆØ\'="', '@abcdefghi«»ðýþ±', '°jklmnopqrªº{¸[]',
                        'µüstuvwxyz¡¿ÐÝÞ®', '¢£¥·©§¶¼½¾¬|¯¨´×', 'æABCDEFGHI\xadôöòóõ',
                        'åJKLMNOPQR¹û~ùúÿ', '\\÷STUVWXYZ²ÔÖÒÓÕ', '0123456789³ÛÜÙÚ●'],
                'cp1143': [' \xa0â{àáã}çñ§.<(+!', '&`êëèíîïìß€Å*);^', '-/Â#ÀÁÃ$ÇÑö,%_>?',
                        'ø\\ÊËÈÍÎÏÌé:ÄÖ\'="', 'Øabcdefghi«»ðýþ±', '°jklmnopqrªºæ¸Æ]',
                        'µüstuvwxyz¡¿ÐÝÞ®', '¢£¥·©[¶¼½¾¬|¯¨´×', 'äABCDEFGHI\xadô¦òóõ',
                        'åJKLMNOPQR¹û~ùúÿ', 'É÷STUVWXYZ²Ô@ÒÓÕ', '0123456789³ÛÜÙÚ●'],
                'cp1144': [' \xa0âä{áãå\\ñ°.<(+!', '&]êë}íîï~ßé$*);^', '-/ÂÄÀÁÃÅÇÑò,%_>?',
                        'øÉÊËÈÍÎÏÌù:£§\'="', 'Øabcdefghi«»ðýþ±', '[jklmnopqrªºæ¸Æ€',
                        'µìstuvwxyz¡¿ÐÝÞ®', '¢#¥·©@¶¼½¾¬|¯¨´×', 'àABCDEFGHI\xadôö¦óõ',
                        'èJKLMNOPQR¹ûü`úÿ', 'ç÷STUVWXYZ²ÔÖÒÓÕ', '0123456789³ÛÜÙÚ●'],
                'cp1145': [' \xa0âäàáãåç¦[.<(+|', '&éêëèíîïìß]$*);¬', '-/ÂÄÀÁÃÅÇ#ñ,%_>?',
                        'øÉÊËÈÍÎÏÌ`:Ñ@\'="', 'Øabcdefghi«»ðýþ±', '°jklmnopqrªºæ¸Æ€',
                        'µ¨stuvwxyz¡¿ÐÝÞ®', '¢£¥·©§¶¼½¾^!¯~´×', '{ABCDEFGHI\xadôöòóõ',
                        '}JKLMNOPQR¹ûüùúÿ', '\\÷STUVWXYZ²ÔÖÒÓÕ', '0123456789³ÛÜÙÚ●'],
                'cp1146': [' \xa0âäàáãåçñ$.<(+|', '&éêëèíîïìß!£*);¬', '-/ÂÄÀÁÃÅÇÑ¦,%_>?',
                        'øÉÊËÈÍÎÏÌ`:#@\'="', 'Øabcdefghi«»ðýþ±', '°jklmnopqrªºæ¸Æ€',
                        'µ¯stuvwxyz¡¿ÐÝÞ®', '¢[¥·©§¶¼½¾^]~¨´×', '{ABCDEFGHI\xadôöòóõ',
                        '}JKLMNOPQR¹ûüùúÿ', '\\÷STUVWXYZ²ÔÖÒÓÕ', '0123456789³ÛÜÙÚ●'],
                'cp1147': [' \xa0âä@áãå\\ñ°.<(+!', '&{êë}íîïìß§$*);^', '-/ÂÄÀÁÃÅÇÑù,%_>?',
                        'øÉÊËÈÍÎÏÌµ:£à\'="', 'Øabcdefghi«»ðýþ±', '[jklmnopqrªºæ¸Æ€',
                        '`¨stuvwxyz¡¿ÐÝÞ®', '¢#¥·©]¶¼½¾¬|¯~´×', 'éABCDEFGHI\xadôöòóõ',
                        'èJKLMNOPQR¹ûü¦úÿ', 'ç÷STUVWXYZ²ÔÖÒÓÕ', '0123456789³ÛÜÙÚ●'],
                'cp1148': [' \xa0âäàáãåçñ[.<(+!', '&éêëèíîïìß]$*);^', '-/ÂÄÀÁÃÅÇÑ¦,%_>?',
                        'øÉÊËÈÍÎÏÌ`:#@\'="', 'Øabcdefghi«»ðýþ±', '°jklmnopqrªºæ¸Æ€',
                        'µ~stuvwxyz¡¿ÐÝÞ®', '¢£¥·©§¶¼½¾¬|¯¨´×', '{ABCDEFGHI\xadôöòóõ',
                        '}JKLMNOPQR¹ûüùúÿ', '\\÷STUVWXYZ²ÔÖÒÓÕ', '0123456789³ÛÜÙÚ●'],
                'cp1149': [' \xa0âäàáãåçñÞ.<(+!', '&éêëèíîïìßÆ$*);Ö', '-/ÂÄÀÁÃÅÇÑ¦,%_>?',
                        'øÉÊËÈÍÎÏÌð:#Ð\'="', 'Øabcdefghi«»`ý{±', '°jklmnopqrªº}¸]€',
                        'µöstuvwxyz¡¿@Ý[®', '¢£¥·©§¶¼½¾¬|¯¨\\×', 'þABCDEFGHI\xadô~òóõ',
                        'æJKLMNOPQR¹ûüùúÿ', '´÷STUVWXYZ²Ô^ÒÓÕ', '0123456789³ÛÜÙÚ●'],
                'cp1160': [' \xa0กขฃคฅฆง[¢.<(+|', '&่จฉชซฌญฎ]!$*);¬', '-/ฏฐฑฒณดต^¦,%_>?',
                        '฿๎ถทธนบปผ`:#@\'="', '๏abcdefghiฝพฟภมย', '๚jklmnopqrรฤลฦวศ',
                        '๛~stuvwxyzษสหฬอฮ', '๐๑๒๓๔๕๖๗๘๙ฯะัาำิ', '{ABCDEFGHI้ีึืุู',
                        '}JKLMNOPQRฺเแโใไ', '\\๊STUVWXYZๅๆ็่้๊', '0123456789๋์ํ๋€●'],
                'bracket': [' \xa0âäàáãåçñ¢.<(+|', '&éêëèíîïìß!$*);¬', '-/ÂÄÀÁÃÅÇÑ¦,%_>?',
                            'øÉÊËÈÍÎÏÌ`:#@\'="', 'Øabcdefghi«»ðýþ±', '°jklmnopqrªºæ¸Æ¤',
                            'µ~stuvwxyz¡¿Ð[Þ®', '^£¥·©§¶¼½¾Ý¨¯]´×', '{ABCDEFGHI\xadôöòóõ',
                            '}JKLMNOPQR¹ûüùúÿ', '\\÷STUVWXYZ²ÔÖÒÓÕ', '0123456789³ÛÜÙÚ●']
            }

            # Fill up the screen.
            p.send_records(1)

            # Check the SBCS code pages.
            for cp in cp_all_map.keys():
                self.get(f'http://127.0.0.1:{sport}/3270/rest/json/Set(codePage,{cp})')
                r = self.get(f'http://127.0.0.1:{sport}/3270/rest/json/Ascii1(2,2,12,16)')
                self.assertEqual(cp_all_map[cp], r.json()['result'])

        # Wait for the process to exit successfully.
        self.get(f'http://127.0.0.1:{sport}/3270/rest/json/Quit()')
        self.vgwait(s3270)

    # s3270 APL code page test.
    def test_s3270_apl_code_page(self):

        apl_chars = [
            ' ABCDEFGHI      ',
            ' QRSTUVWXY      ',
            '  bcdefghi      ',
            '⋄∧¨⌻⍸⍷⊢⊣∨       ',
            '∼  ⎸⎹│    ↑↓≤⌈⌊→',
            '⎕▌▐▀▄■    ⊃⊂¤○±←',
            '¯°─•      ∩∪⊥[≥∘',
            '⍺∊⍳⍴⍵ ×\\÷ ∇∆⊤]≠∣',
            '{⁼+∎└┌├┴§ ⍲⍱⌷⌽⍂⍉',
            '}⁾-┼┘┐┤┬¶ ⌶!⍒⍋⍞⍝',
            '≡₁ʂʃ⍤⍥⍪€  ⌿⍀∵⊖⌹⍕',
            '⁰¹²³⁴⁵⁶⁷⁸⁹ ⍫⍙⍟⍎ '
        ]

        # Start playback.
        pport, ts = unused_port()
        with playback(self, 's3270/Test/apl.trc', port=pport) as p:
            ts.close()

            # Start s3270.
            sport, ts = unused_port()
            s3270 = Popen(vgwrap(['s3270', '-httpd', str(sport), '-utf8', f'127.0.0.1:{pport}']))
            self.children.append(s3270)
            self.check_listen(sport)
            ts.close()

            # Fill up the screen.
            p.send_records(1)

        # Check the SBCS code pages.
        r = self.get(f'http://127.0.0.1:{sport}/3270/rest/json/Ascii1(2,2,12,16)')
        self.assertEqual(apl_chars, r.json()['result'])
        
        # Wait for the process to exit successfully.
        self.get(f'http://127.0.0.1:{sport}/3270/rest/json/Quit()')
        self.vgwait(s3270)

    # s3270 simplified Chinese test.
    def s3270_simplified_chinese(self, codePage):

        expect_chinese = [
            '   国务院总理李克强 2 月 14 日主持召开国务院常务会议，听取 2021 年全国两会建议提',
            '案办理情况汇报，要求汇聚众智做好今年政府工作；确定促进工业经济平稳增长和服务业特',
            '殊困难行业纾困发展的措施。                                                      '
        ]

        # Start playback.
        pport, ts = unused_port()
        with playback(self, 's3270/Test/935.trc', port=pport) as p:
            ts.close()

            # Start s3270.
            sport, ts = unused_port()
            s3270 = Popen(vgwrap(['s3270', '-httpd', str(sport), '-utf8', '-codepage', codePage, f'127.0.0.1:{pport}']))
            self.children.append(s3270)
            self.check_listen(sport)
            ts.close()

            # Fill up the screen.
            p.send_records(1)

            # Check the DBCS output.
            r = self.get(f'http://127.0.0.1:{sport}/3270/rest/json/Ascii1(2,1,3,80)')
            self.assertEqual(expect_chinese, r.json()['result'])
        
        # Wait for the process to exit successfully.
        self.get(f'http://127.0.0.1:{sport}/3270/rest/json/Quit()')
        self.vgwait(s3270)

    # Simplified Chinese tests.
    def test_s3270_cp935(self):
        self.s3270_simplified_chinese('cp935')
    def test_s3270_cp1388(self):
        self.s3270_simplified_chinese('cp1388')

    # s3270 code page 937 (traditional Chinese) test.
    def test_s3270_cp937(self):

        expect_chinese = [
            '   辦理中止委託轉帳代繳，須提供原代繳帳號，申辦資料如有疑義或不明，經本公司聯繫補',
            ' 正資料，請配合提供補正；不願提供者不予受理。                                   '
        ]

        # Start playback.
        pport, ts = unused_port()
        with playback(self, 's3270/Test/937.trc', port=pport) as p:
            ts.close()

            # Start s3270.
            sport, ts = unused_port()
            s3270 = Popen(vgwrap(['s3270', '-httpd', str(sport), '-utf8', '-codepage', 'cp937', f'127.0.0.1:{pport}']))
            self.children.append(s3270)
            self.check_listen(sport)
            ts.close()

            # Fill up the screen.
            p.send_records(1)

            # Check the DBCS output.
            r = self.get(f'http://127.0.0.1:{sport}/3270/rest/json/Ascii1(2,1,2,80)')
            self.assertEqual(expect_chinese, r.json()['result'])
        
        # Wait for the process to exit successfully.
        self.get(f'http://127.0.0.1:{sport}/3270/rest/json/Quit()')
        self.vgwait(s3270)

    # s3270 Japanese DBCS test.
    def s3270_japanese(self, codePage):

        expect_japanese = {
            'cp930': [
                ' Jｧﾆｧﾄｵﾍｵ ﾎｵﾍﾎ ｶﾅﾈ ｳﾅｴｵ ﾆｧｷｵ 930                                                ',
                '   高度な音声信号処理を行う高精度ボイスピックアップテクノロジーにより、高い通話品',
                ' 質を実現。 AI による機械学習アルゴリズムで実現されたノイズリダクションシステムが'],
            'cp939': [
                ' Japanese test for code page 930                                                ',
                '   高度な音声信号処理を行う高精度ボイスピックアップテクノロジーにより、高い通話品',
                ' 質を実現。 AI による機械学習アルゴリズムで実現されたノイズリダクションシステムが'],
            'cp1390': [
                ' Jｧﾆｧﾄｵﾍｵ ﾎｵﾍﾎ ｶﾅﾈ ｳﾅｴｵ ﾆｧｷｵ 930                                                ',
                '   高度な音声信号処理を行う高精度ボイスピッ アップテ ノロジーにより、高い通話品',
                ' 質を実現。 AI による機械学習アルゴリズムで実現されたノイズリダ ションシステムが'],
            'cp1399': [
                ' Japanese test for code page 930                                                ',
                '   高度な音声信号処理を行う高精度ボイスピックアップテクノロジーにより、高い通話品',
                ' 質を実現。 AI による機械学習アルゴリズムで実現されたノイズリダクションシステムが']
        }

        # Start playback.
        pport, ts = unused_port()
        with playback(self, 's3270/Test/930.trc', port=pport) as p:
            ts.close()

            # Start s3270.
            sport, ts = unused_port()
            s3270 = Popen(vgwrap(['s3270', '-httpd', str(sport), '-utf8', '-codepage', codePage, f'127.0.0.1:{pport}']))
            self.children.append(s3270)
            self.check_listen(sport)
            ts.close()

            # Fill up the screen.
            p.send_records(1)

            # Check the DBCS output.
            r = self.get(f'http://127.0.0.1:{sport}/3270/rest/json/Ascii1(1,1,3,80)')
            self.assertEqual(expect_japanese[codePage], r.json()['result'])
        
        # Wait for the process to exit successfully.
        self.get(f'http://127.0.0.1:{sport}/3270/rest/json/Quit()')
        self.vgwait(s3270)

    def test_s3270_japanese_930(self):
        self.s3270_japanese('cp930')
    def test_s3270_japanese_939(self):
        self.s3270_japanese('cp939')
    def test_s3270_japanese_1390(self):
        self.s3270_japanese('cp1390')
    def test_s3270_japanese_1399(self):
        self.s3270_japanese('cp1399')

    def async_reply(self, p: playback):
        '''Send '''
        got = p.nread(39)
        self.assertEqual('00000000007d5b7d115be40e8a82a3a589c1d0718d41d0619365a3a5ac97c9a19da5d2410fffef', got.hex())
        p.send_records(1)

    # s3270 Korean DBCS test.
    def s3270_korean(self, codePage: str, show_codepage: str):

        korean_text = "국민과함께하는민생토론회"

        # Start playback.
        pport, ts = unused_port()
        with playback(self, 's3270/Test/korean.trc', port=pport) as p:
            ts.close()

            # Start s3270.
            sport, ts = unused_port()
            s3270 = Popen(vgwrap(['s3270', '-httpd', str(sport), '-utf8', '-codepage', codePage, f'127.0.0.1:{pport}']))
            self.children.append(s3270)
            self.check_listen(sport)
            ts.close()

            # Check the codepage config.
            r = self.get(f'http://127.0.0.1:{sport}/3270/rest/json/Show(codepage)')
            out = r.json()['result'][0]
            self.assertEqual(out, show_codepage)

            # Fill up the screen.
            p.send_records(2)

            # Prepare for the Enter.
            reply_thread = threading.Thread(target=self.async_reply, args=[p])
            reply_thread.start()

            # Send the text.
            self.get(f'http://127.0.0.1:{sport}/3270/rest/json/String("{korean_text}")')
            self.get(f'http://127.0.0.1:{sport}/3270/rest/json/Enter())')

        # Wait for the thread.
        reply_thread.join(timeout=2)

        # Wait for the process to exit successfully.
        self.get(f'http://127.0.0.1:{sport}/3270/rest/json/Quit()')
        self.vgwait(s3270)

    def test_korean_933(self):
        self.s3270_korean('korean', 'cp933 sbcs gcsgid 1173 cpgid 833 dbcs gcsgid 934 cpgid 834')
    def test_korean_1364(self):
        self.s3270_korean('korean-euro', 'cp1364 sbcs gcsgid 1173 cpgid 833 dbcs gcsgid 934 cpgid 834')

    # s3270 PrintText() wrap test.
    def s3270_dbcs_wrap(self, which: str):

        japanese_text = "国内外の取材網を生かし国内外の取材網を生かし国内外の取材網を生かし国内外の取材網を生かし国内外の取材網を生かし"

        # Start playback.
        pport, ts = unused_port()
        with playback(self, 's3270/Test/dbcs-wrap.trc', port=pport) as p:
            ts.close()

            # Start s3270.
            sport, ts = unused_port()
            s3270 = Popen(vgwrap(['s3270', '-httpd', str(sport), '-utf8', '-codepage', 'japanese-latin', f'127.0.0.1:{pport}']))
            self.children.append(s3270)
            self.check_listen(sport)
            ts.close()

            # Fill up the screen.
            p.send_records(2)

            # Send the text.
            self.get(f'http://127.0.0.1:{sport}/3270/rest/json/String({japanese_text})')

        # Get the resulting text from the display. The result should have the wrapped character displayed properly
        # on the first line, and a space at the beginning of the second line.
        if which == 'PrintText':
            r = self.get(f'http://127.0.0.1:{sport}/3270/rest/json/PrintText(string)')
            result = r.json()['result']
            self.assertEqual('==>  ' + japanese_text[:38], result[21])
            self.assertEqual(' ' + japanese_text[38:] + '                                             ', result[22])
        elif which == 'Ascii1':
            r = self.get(f'http://127.0.0.1:{sport}/3270/rest/json/Ascii1(22, 1, 2, 80)')
            result = r.json()['result']
            self.assertEqual('==>  ' + japanese_text[:38], result[0])
            self.assertEqual(' ' + japanese_text[38:] + '                                             ', result[1])

        # Wait for the process to exit successfully.
        self.get(f'http://127.0.0.1:{sport}/3270/rest/json/Quit()')
        self.vgwait(s3270)
    # s3270 wrap tests.
    def test_s3270_dbcs_wrap_printtext(self):
        self.s3270_dbcs_wrap('PrintText')
    def test_s3270_dbcs_wrap_ascii1(self):
        self.s3270_dbcs_wrap('Ascii1')

if __name__ == '__main__':
    unittest.main()
