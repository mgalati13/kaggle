# kaggle

Kaggle 2017 Santa Challenge:
https://www.kaggle.com/c/santa-gift-matching

Round 1 (the contest was relaunched because it was "too easy"). We were the 2nd team to find optimal and therefore placed 2nd (Team = "Matt and Rob")

We found optimal using SAS/OR's (MILP - mixed integer linear programming solver). See the code in sas/. 
The model is built using PROC OPTMODEL - a math programming modeling language in SAS. It took about 1 hour to build and solve.

We also found optimal using COIN/CBC (an open source mixed integer linear programming solver). See the code in coin/.
The model is built in using COIN's C++ APIs. The solver used at the time was Cbc 2.6.2. It took about 40 hours to build and solve.
You can download COIN here: https://www.coin-or.org/

Any questions send to:
  Matthew Galati (matthew.galati@sas.com)
  Rob Pratt (rob.pratt@sas.com)