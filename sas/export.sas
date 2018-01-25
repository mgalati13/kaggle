proc sort data=soldata out=sorted(rename=(ChildId=from GiftId=to));
   by ChildId;
run;

data _null_;
   file "&path.\submission011218_1.csv" delimiter=',';
   if _N_ = 1 then put 'ChildId,GiftId';
   set sorted;
   ChildId = input(substr(from,2),best.);
   GiftId  = input(substr(to,2),best.);
   put ChildId GiftId;
run;
