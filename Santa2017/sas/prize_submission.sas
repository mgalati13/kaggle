option fullstimer;

%let twinObs = 4000;

%let path = \\ordsrv3\ormp\sas\SantaGiftMatching;
libname santa "&path";

proc optmodel;
   /* declare parameters and read data */
   set <str> CHILDREN;
   set <str> GIFTS;
   read data santa.child_wishlist into CHILDREN=[node];
   read data santa.gift_goodkids  into GIFTS=[node];
   set <str,str> CHILD_ARCS;
   set <str,str> GIFT_ARCS;
   num childHappiness {CHILD_ARCS};
   num giftHappiness  {GIFT_ARCS};
   read data santa.child_arcs into CHILD_ARCS=[from to] childHappiness=weight;
   read data santa.gift_arcs  into GIFT_ARCS =[from to] giftHappiness=weight;
   set ARCS init CHILD_ARCS union GIFT_ARCS;
   put (card(ARCS))=;
   num happiness {<g,c> in ARCS} = 
     100 * (if <g,c> in CHILD_ARCS then childHappiness[g,c] else -1)
         + (if <g,c> in GIFT_ARCS  then giftHappiness[g,c]  else -1);

   /* augment arcs to include all gifts for twins */
   put 'Augmenting with twin arcs...';
   set TWIN_PAIRS = setof {i in 0..&twinObs-1 by 2} <'c'||put(i,Z6.),'c'||put(i+1,Z6.)>;
   set TWINS = union {<c1,c2> in TWIN_PAIRS} {c1,c2};
   ARCS = ARCS union (GIFTS cross TWINS);
   put (card(ARCS))=;

   /* add dummy gift node with dummy arc to each non-twin child */
   put 'Augmenting with dummy arcs...';
   str dummy_gift init 'g'||card(GIFTS);
   GIFTS = GIFTS union {dummy_gift};
   ARCS = ARCS union ({dummy_gift} cross (CHILDREN diff TWINS));
   put (card(ARCS))=;

   /* declare binary decision variables */
   /* Flow[g,c] = 1 if gift g is assigned to child c; 0 otherwise */
   var Flow {ARCS} binary;

   /* declare objective */
   max TotalHappiness = sum {<g,c> in ARCS} happiness[g,c] * Flow[g,c];

   /* declare flow balance constraints */
   con SupplyCon {g in GIFTS}:
      sum {<(g),c> in ARCS} Flow[g,c] <= 1000;
   SupplyCon[dummy_gift].ub = card(CHILDREN) - card(TWINS);
   con DemandCon {c in CHILDREN}:
      sum {<g,(c)> in ARCS} Flow[g,c] = 1;

   /* declare side constraints for twins */
   con TwinCon {g in GIFTS diff {dummy_gift}, <c1,c2> in TWIN_PAIRS}:
      Flow[g,c1] = Flow[g,c2];

   /* call mixed integer linear programming (MILP) solver */
   solve with milp / relobjgap=0;

   /* create solution data set */
   create data soldata from [GiftId ChildId]={<g,c> in ARCS: Flow[g,c].sol > 1e-6} happiness Flow;
quit;
