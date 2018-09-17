# ttbasic

A fork of TOYOSHIKI Tiny BASIC for Linux (https://github.com/vintagechips/ttbasic_lin)

To compile and run: `cc main.c -o main && ./main`

## Operation example

```text
> list
10 FOR I=2 TO -2 STEP -1; GOSUB 100; NEXT I
20 STOP
100 REM Subroutine
110 PRINT ABS(I); RETURN

OK
>run
2
1
0
1
2

OK
>
```

## Grammar

The grammar is the same as
PALO ALTO TinyBASIC by Li-Chen Wang
Except 4 point to show below.

1. The contracted form of the description is invalid.
2. Force abort key
   * PALO ALTO TinyBASIC -> [Ctrl]+[C]
   * TOYOSHIKI TinyBASIC -> [ESC]
3. SYSTEM command
   * SYSTEM return to Linux.
4. Other some beyond my expectations.

(C)2015 Tetsuya Suzuki
GNU General Public License
