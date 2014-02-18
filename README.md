EasyMotion Plugin for Qt Creator
===========

An EasyMotion plugin for Qt Creator

#  Jump to any position on screen.   
1. Press `<control + ;>` to trigger EasyMotion. 
2. Press the character you want to jump to, then the some or all occurrences of the character on screen will be covered by some red character with yellow background. Here, if the character you type is not upper case letter, the search will not be case sensative. If you type in a upper case letter, the search will be case sensative. For example, typing `s` will trigger EasyMotion to search for all occurrences of `s` and `S`;  typing `S` will trigger EasyMotion to search for all occurrences of only `S`.  
3. If the position you want to jump to is not covered with red text, then you can hit `<Enter>` to make EasyMotion show the next group of target positions. Pressing `<Shift + Enter>` will make EasyMotion show the previous group of target positions.
4. Press the red character text displayed at the position where you want to jump.
5. Pressing `<ESC>` will force EasyMotion into untriggered state.

# Jump to any position on the current line
1. Press `<control + '>`, then one line of red characters will be display on the line before or after the current line.
2. Look for the red character that is horizontally aligned with the position you want to jump to, and hit that character. EasyMotion will jump to corresponding position on the same line.
