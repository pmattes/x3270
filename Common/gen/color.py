#!/usr/bin/env python3

import Common.gen.gen as gen

g = gen.gen(43, 80)

# Ask for TN3270E.
out = g.telnet_do('tn3270e')
g.dump(out)

# Ask for the device type.
out = g.telnet_sb('tn3270e') + '0802' + g.telnet_se()
g.dump(out)

# Tell them what the device type is.
out = g.telnet_sb('tn3270e') + '0204' + g.atext(['IBM-3278-4-E']) + '01' + g.atext(['IBM0TEQ0']) + g.telnet_se()
g.dump(out)

# Tell them what TN3270E we will support (no BIND-IMAGE)
out = g.telnet_sb('tn3270e') + '03040204' + g.telnet_se()
g.dump(out)

# Draw the screen.
out = g.tn3270e('3270-data', 'none', 'error-response', 1)
out += g.ewa('reset,alarm,restore')
out += g.sba(1, 1) + g.sfe('3270', 'protect', 'fg', 'default') + g.text('Default')
out += g.sba(2, 1) + g.sfe('3270', 'protect', 'fg', 'neutralBlack') + g.text('Neutral/Black (same as background)')
out += g.sba(3, 1) + g.sfe('3270', 'protect', 'fg', 'blue') + g.text('Blue')
out += g.sba(4, 1) + g.sfe('3270', 'protect', 'fg', 'red') + g.text('Red')
out += g.sba(5, 1) + g.sfe('3270', 'protect', 'fg', 'pink') + g.text('Pink')
out += g.sba(6, 1) + g.sfe('3270', 'protect', 'fg', 'green') + g.text('Green')
out += g.sba(7, 1) + g.sfe('3270', 'protect', 'fg', 'turquoise') + g.text('Turquoise')
out += g.sba(8, 1) + g.sfe('3270', 'protect', 'fg', 'yellow') + g.text('Yellow')
out += g.sba(9, 1) + g.sfe('3270', 'protect', 'fg', 'neutralWhite') + g.text('Neutral/White (same as foreground)')
out += g.sba(10, 1) + g.sfe('3270', 'protect', 'fg', 'black') + g.text('Black')
out += g.sba(11, 1) + g.sfe('3270', 'protect', 'fg', 'deepBlue') + g.text('Deep Blue')
out += g.sba(12, 1) + g.sfe('3270', 'protect', 'fg', 'orange') + g.text('Orange')
out += g.sba(13, 1) + g.sfe('3270', 'protect', 'fg', 'purple') + g.text('Purple')
out += g.sba(14, 1) + g.sfe('3270', 'protect', 'fg', 'paleGreen') + g.text('Pale Green')
out += g.sba(15, 1) + g.sfe('3270', 'protect', 'fg', 'paleTurquoise') + g.text('Pale Turquoise')
out += g.sba(16, 1) + g.sfe('3270', 'protect', 'fg', 'grey') + g.text('Grey')
out += g.sba(17, 1) + g.sfe('3270', 'protect', 'fg', 'white') + g.text('White')
out += g.sba(18, 1) + g.sf('protect')
for color in ['default', 'neutralBlack', 'blue', 'red', 'pink', 'green', 'turquoise', 'yellow',
        'neutralWhite', 'black', 'deepBlue', 'orange', 'purple', 'paleGreen', 'paleTurquoise',
        'grey', 'white']:
        out += g.sa('fg', color) + g.text('X')
out += g.sa('all', '00') + g.sba(19, 1) + g.sf('modify') + g.ic() + g.sba(20, 1) + g.sf('protect')
out += g.eor()
g.dump(out)