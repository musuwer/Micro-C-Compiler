# Micro C three-address code
# idx | instruction
000 | function main:
001 | decl int i
002 | i = 0
003 | decl int total
004 | total = 0
005 | L0:
006 | t0 = i < 3
007 | ifz t0 goto L1
008 | decl int j
009 | j = 0
010 | L2:
011 | t1 = j < 3
012 | ifz t1 goto L3
013 | t2 = total + i
014 | t3 = t2 + j
015 | total = t3
016 | t4 = j + 1
017 | j = t4
018 | goto L2
019 | L3:
020 | t5 = i + 1
021 | i = t5
022 | goto L0
023 | L1:
024 | return total
025 | end function
