# Mirror divergence report

20 seeds: 18 matched to their end point, 0 stopped at a difference, 2 stopped where the game itself is not deterministic.

## Matched

| Seed | Result | Floor | Steps |
|---|---|---|---|
| 1000 (`TK`) | reached_limit | 18 | 170 |
| 1001 (`TL`) | reached_limit | 18 | 171 |
| 1002 (`TM`) | reached_limit | 18 | 196 |
| 1003 (`TN`) | reached_limit | 18 | 193 |
| 1004 (`TP`) | reached_limit | 18 | 204 |
| 1005 (`TQ`) | reached_limit | 18 | 237 |
| 1006 (`TR`) | reached_limit | 18 | 225 |
| 1007 (`TS`) | reached_limit | 18 | 149 |
| 1008 (`TT`) | completed | 6 | 69 |
| 1009 (`TU`) | reached_limit | 18 | 216 |
| 1010 (`TV`) | reached_limit | 18 | 162 |
| 1011 (`TW`) | completed | 16 | 154 |
| 1012 (`TX`) | completed | 16 | 218 |
| 1013 (`TY`) | reached_limit | 18 | 136 |
| 1014 (`TZ`) | reached_limit | 18 | 180 |
| 1015 (`U0`) | reached_limit | 18 | 234 |
| 1017 (`U2`) | reached_limit | 18 | 201 |
| 1018 (`U3`) | reached_limit | 18 | 212 |

## Stopped where the game is not deterministic

After a discovery, `cardRandomRng` depends on the frame rate until the next floor, so these runs say nothing about the simulator past that point.

| Seed | Floor | Steps | First field |
|---|---|---|---|
| 1016 (`U1`) | 7 | 83 | `hand` |
| 1019 (`U4`) | 16 | 184 | `draw_pile` |
