%let childWishListLen = 10;
%let giftWishListLen  = 1000;

%let path = \\ordsrv3\ormp\sas\SantaGiftMatching;
libname santa "&path";

data santa.child_wishlist;
   infile "&path.\child_wishlist.csv" delimiter = ',' MISSOVER DSD lrecl=32767;
   input childId gift0-gift%eval(&childWishListLen-1);
   node = 'c'||put(childId,z6.);
run;

data santa.gift_goodkids;
   infile "&path.\gift_goodkids.csv" delimiter = ',' MISSOVER DSD lrecl=32767;
   input giftId child0-child%eval(&giftWishListLen-1);
   node = 'g'||put(giftId,z3.);
run;

data santa.arcs1(keep=from to weight);
   set santa.child_wishlist;
   array giftPref[0:%eval(&childWishListLen-1)] gift0-gift%eval(&childWishListLen-1);
   from = 'c'||put(childId,z6.);
   do g = 0 to &childWishListLen-1;
      to = 'g'||put(giftPref[g],z3.);
      weight = 2 * (&childWishListLen - g);
      output;
   end;
run;

data santa.arcs2(keep=from to weight);
   set santa.gift_goodkids;
   array childPref[0:%eval(&giftWishListLen-1)] child0-child%eval(&giftWishListLen-1);
   to = 'g'||put(giftId,z3.);
   do c = 0 to &giftWishListLen-1;
      from = 'c'||put(childPref[c],z6.);
      weight = 2 * (&giftWishListLen - c);
      output;
   end;
run;
