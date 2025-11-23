#!/usr/bin/env python3

import gen

g = gen.Gen(43, 80)

# Ask for TN3270E.
out = g.telnet_do('tn3270e')
g.dump(out)

# Ask for the device type.
out = g.telnet_sb('tn3270e') + g.tn3270e_send('device-type') + g.telnet_se()
g.dump(out)

# Tell them what the device type is.
out = g.telnet_sb('tn3270e') + g.tn3270e_device_type('is', 'IBM-3278-4-E') + g.tn3270e_connect('IBM0TEQ0') + g.telnet_se()
g.dump(out)

# Tell them what TN3270E we will support (no BIND-IMAGE)
out = g.telnet_sb('tn3270e') + g.tn3270e_functions('is', 'responses,sysreq') + g.telnet_se()
g.dump(out)

# Draw the screen.
out = g.tn3270e('3270-data', 'none', 'error-response', 1)
out += g.cmd_ewa('reset,alarm,restore')
out += g.ord_sba(1, 1) + g.ord_sfe('3270', 'protect', 'fg', 'default') + g.text('Default')
out += g.ord_sba(2, 1) + g.ord_sfe('3270', 'protect', 'fg', 'neutralBlack') + g.text('Neutral/Black (same as background)')
out += g.ord_sba(3, 1) + g.ord_sfe('3270', 'protect', 'fg', 'blue') + g.text('Blue')
out += g.ord_sba(4, 1) + g.ord_sfe('3270', 'protect', 'fg', 'red') + g.text('Red')
out += g.ord_sba(5, 1) + g.ord_sfe('3270', 'protect', 'fg', 'pink') + g.text('Pink')
out += g.ord_sba(6, 1) + g.ord_sfe('3270', 'protect', 'fg', 'green') + g.text('Green')
out += g.ord_sba(7, 1) + g.ord_sfe('3270', 'protect', 'fg', 'turquoise') + g.text('Turquoise')
out += g.ord_sba(8, 1) + g.ord_sfe('3270', 'protect', 'fg', 'yellow') + g.text('Yellow')
out += g.ord_sba(9, 1) + g.ord_sfe('3270', 'protect', 'fg', 'neutralWhite') + g.text('Neutral/White (same as foreground)')
out += g.ord_sba(10, 1) + g.ord_sfe('3270', 'protect', 'fg', 'black') + g.text('Black')
out += g.ord_sba(11, 1) + g.ord_sfe('3270', 'protect', 'fg', 'deepBlue') + g.text('Deep Blue')
out += g.ord_sba(12, 1) + g.ord_sfe('3270', 'protect', 'fg', 'orange') + g.text('Orange')
out += g.ord_sba(13, 1) + g.ord_sfe('3270', 'protect', 'fg', 'purple') + g.text('Purple')
out += g.ord_sba(14, 1) + g.ord_sfe('3270', 'protect', 'fg', 'paleGreen') + g.text('Pale Green')
out += g.ord_sba(15, 1) + g.ord_sfe('3270', 'protect', 'fg', 'paleTurquoise') + g.text('Pale Turquoise')
out += g.ord_sba(16, 1) + g.ord_sfe('3270', 'protect', 'fg', 'grey') + g.text('Grey')
out += g.ord_sba(17, 1) + g.ord_sfe('3270', 'protect', 'fg', 'white') + g.text('White')
out += g.ord_sba(18, 1) + g.ord_sf('protect')
for color in ['default', 'neutralBlack', 'blue', 'red', 'pink', 'green', 'turquoise', 'yellow',
        'neutralWhite', 'black', 'deepBlue', 'orange', 'purple', 'paleGreen', 'paleTurquoise',
        'grey', 'white']:
        out += g.ord_sa('fg', color) + g.text('X')
out += g.ord_sa('all', '00') + g.ord_sba(19, 1) + g.ord_sf('modify') + g.ord_ic() + g.ord_sba(20, 1) + g.ord_sf('protect')
out += g.telnet_eor()
g.dump(out)