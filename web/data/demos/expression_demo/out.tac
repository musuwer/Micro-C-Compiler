# Micro C three-address code
# idx | instruction
000 | function main:
001 | decl int a
002 | t0 = 3 * 4
003 | t1 = 2 + t0
004 | a = t1
005 | decl int b
006 | t2 = 2 + 3
007 | t3 = t2 * 4
008 | b = t3
009 | decl int c
010 | c = 0
011 | t4 = a == 14
012 | t5 = b == 20
013 | t6 = t4 && t5
014 | t7 = 2 * 3
015 | t8 = 1 + t7
016 | t9 = t8 == 7
017 | t10 = t6 && t9
018 | t11 = a < 10
019 | t12 = b < 10
020 | t13 = t11 || t12
021 | t14 = !t13
022 | t15 = t10 && t14
023 | ifz t15 goto L0
024 | t16 = a + 2
025 | return t16
026 | goto L1
027 | L0:
028 | L1:
029 | return c
030 | end function
