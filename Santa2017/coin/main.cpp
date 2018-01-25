//Kaggle 2017 - Santa Gift Matching Challenge (Round 1)
//Matthew Galati (matthew.galati@sas.com) and Rob Pratt (rob.pratt@sas.com)
//----------------------------------------------------------------------------
//Input:
//  child -> gift wish list
//  gift  -> child wish list
//----------------------------------------------------------------------------
#include <cstdio>
#include <iostream>
#include <fstream>
#include <sstream>
#include <cassert>
#include <cmath>
#include <cstdint>
#include <cstring>
#include <iomanip>
#include <time.h>
#include <algorithm>
using namespace std;
//----------------------------------------------------------------------------
#include "CbcModel.hpp"
#include "CoinPackedVector.hpp"
#include "CoinPackedMatrix.hpp"
#include "OsiSolverInterface.hpp"
#include "OsiClpSolverInterface.hpp"
//----------------------------------------------------------------------------
//#define DEBUG
#ifdef DEBUG
#define nChildren     10000
#else
#define nChildren     1000000
#endif
#define nGiftTypes    1000
#define nGiftsPerType 1000
#define giftListLen   1000//number of children a gift ranks
#define childListLen  10  //number of gifts a child ranks 
//----------------------------------------------------------------------------
int twinBeg = 0;
#ifdef DEBUG
int twinEnd = 40;
#else
int twinEnd = 4000;
#endif
int nTwins  = twinEnd - twinBeg;
int dummyGift = nGiftTypes;
//----------------------------------------------------------------------------
string childListFile = "/bigdisk/lax/magala/santa/child_wishlist.csv";
string giftListFile = "/bigdisk/lax/magala/santa/gift_goodkids.csv";
string solutionFile = "/bigdisk/lax/magala/santa/submission121517_7.csv";
string solutionNewFile = "/bigdisk/lax/magala/santa/submissionX.csv";
//----------------------------------------------------------------------------
//c,g: child c, gift g
int solutionCtoG[nChildren];
int solutionNew[nChildren];
//c,o -> g : child c, gift g (pref=o) 
int childList[nChildren*childListLen];
//g,o -> c : gift g, child c (pref=o) 
int giftList[nGiftTypes*giftListLen];
//----------------------------------------------------------------------------
#define indexCG(c,g) ((c)*nGiftTypes+(g))
#define indexGC(g,c) ((g)*nChildren+(c))
//----------------------------------------------------------------------------
//#define BUILD_NAMES
//----------------------------------------------------------------------------
class SantaData {
public:
   //c,g -> o : child c, gift g (pref=o)
   int * giftOrder;
   //g,c -> o : gift g, child c (pref=o) 
   int * childOrder;
   //arcs (in columns order)
   vector< pair<int,int> > arcsV;
public:
   OsiSolverInterface * osi;

public:
   SantaData(){
      osi = new OsiClpSolverInterface();
      assert(osi);
      osi->setObjSense(-1); //max

      giftOrder = new int[nChildren*nGiftTypes];
      assert(giftOrder);
      childOrder = new int[nGiftTypes*nChildren];
      assert(childOrder);
   }
   
   ~SantaData(){
      delete osi;
      delete [] giftOrder;
      delete [] childOrder;
   }
};

//----------------------------------------------------------------------------
void readChildListFile(SantaData & sData){
   //each row represents the ChildId, followed by 10 GiftIds (in preference order)
   ifstream is;
   is.open(childListFile.c_str());
   
   int * giftOrder = sData.giftOrder;
   for(int i = 0; i < nChildren*nGiftTypes; i++){
      giftOrder[i] = -1;
   }

   int  c, g, childId, giftId;
   char chr;
   int offset   = 0;
   for(c = 0; c < nChildren; c++){
      is >> childId >> chr;
      assert(childId == c);
      for(g = 0; g < childListLen-1; g++){
         is >> giftId >> chr;
         giftOrder[indexCG(c,giftId)] = g;
         childList[offset] = giftId;
         offset++;
      }
      is >> giftId;
      giftOrder[indexCG(c,giftId)] = g;
      childList[offset] = giftId;
      offset++;
   }
   cout << "Done reading child list file." << endl;
   is.close();
}

//----------------------------------------------------------------------------
void readGiftListFile(SantaData & sData){
   //each row represents the GiftId, followed by 1000 ChildIds (in preference order)
   ifstream is;
   is.open(giftListFile.c_str());

   int * childOrder = sData.childOrder;
   for(int i = 0; i < nGiftTypes*nChildren; i++){
      childOrder[i] = -1;
   }

   int  c, g, childId, giftId;
   char chr;
   int offset = 0;
   for(g = 0; g < nGiftTypes; g++){
      is >> giftId >> chr;
      assert(giftId == g);
      for(c = 0; c < giftListLen-1; c++){
         is >> childId >> chr;
         //for debugging
#ifdef DEBUG
         if(childId >= nChildren)
            childId = 0;
      assert(childId < nChildren);
#endif
         childOrder[indexGC(g,childId)] = c;
         giftList[offset] = childId;
         offset++;
      }
      is >> childId;
#ifdef DEBUG
         if(childId >= nChildren)
            childId = 0;
#endif
      childOrder[indexGC(g,childId)] = c;
      giftList[offset] = childId;
      offset++;
   }
   cout << "Done reading gift list file." << endl;
   is.close();  
}
//----------------------------------------------------------------------------
void readSolutionFile(SantaData & sData){
   ifstream is;
   is.open(solutionFile.c_str());

   char chr;
   string str;
   int  c, childId, giftId;
   is >> str;
   //cout << str << endl;
   for(c = 0; c < nChildren; c++){
      is >> childId >> chr >> giftId;
      solutionCtoG[childId] = giftId;
      //cout << "childId " << childId << " giftId " << giftId << endl;
   }   
   cout << "Done reading solution file." << endl;
   is.close();
}
//----------------------------------------------------------------------------
void dumpSolutionFile(SantaData & sData){
   ofstream os;
   os.open(solutionNewFile.c_str());

   string str;
   int  c;
   os << "ChildId,GiftId" << endl;
   for(c = 0; c < nChildren; c++){
      os << c << "," << solutionNew[c] << endl;
   }   
   cout << "Done dumping solution file." << endl;
   os.close();
}
//----------------------------------------------------------------------------
double calculateScore(SantaData & sData){
   double score = 0.0;
   int * childOrder = sData.childOrder;
   int * giftOrder  = sData.giftOrder;
   int c,g,gOrder,cOrder;
   int childMisses = 0;
   int giftMisses  = 0;
   double childHappySum = 0.0;
   double giftHappySum  = 0.0;
   cout << setprecision(15);
   for(c = 0; c < nChildren; c++){
      g = solutionNew[c];
      //g = solutionCtoG[c];
      gOrder = giftOrder[indexCG(c,g)];
      cOrder = childOrder[indexGC(g,c)];
      //cout << "c " << c << " g " << g << " gOrder " << gOrder << endl;
      if(gOrder == -1){
         childHappySum -= 1.0; 
         childMisses++;
      }
      else{
         childHappySum += 2.0 * (childListLen-gOrder);
      }

      if(cOrder == -1){
         giftHappySum -= 1.0;
         giftMisses++;
      }
      else{
         giftHappySum += 2.0 * (giftListLen-cOrder);
      }

   }
   score = 100 * childHappySum + giftHappySum;
   cout << "ChildMisses  :" << childMisses << endl;
   cout << "GiftMisses   :" << giftMisses  << endl;
   cout << "ChildHappySum:" << childHappySum << endl;
   cout << "GiftHappySum :" << giftHappySum  << endl;
   cout << "Score        :" << score << endl;

   return score;
}

//----------------------------------------------------------------------------
int solveModel(SantaData & sData){
   OsiClpSolverInterface * osiClp =
      dynamic_cast<OsiClpSolverInterface*>(sData.osi);
   const char * argv[20];
   int    argc      = 0;
   string cbcExe    = "cbc";
   string cbcSolve  = "-solve";
   string cbcQuit   = "-quit";
   argv[argc++] = cbcExe.c_str();
   argv[argc++] = cbcSolve.c_str();
   argv[argc++] = cbcQuit.c_str();
   CbcModel cbc(*osiClp);
   cbc.setObjSense(-1);
   cbc.setLogLevel(3);
   CbcMain0(cbc);
   CbcMain1(argc, argv, cbc);
   assert(cbc.isProvenOptimal());

   const double * solution = cbc.bestSolution();
   vector< pair<int,int> > & arcsV = sData.arcsV;
   vector< pair<int,int> >::iterator vpi;   
   int colIndex = 0;
   int g,c;
   for(vpi = arcsV.begin(); vpi != arcsV.end(); vpi++){
      g = (*vpi).first;
      c = (*vpi).second;
      if(solution[colIndex] > 0.5)
         solutionNew[c] = g;
      colIndex++;
   }
   
   return 0;
}

//----------------------------------------------------------------------------
string UtilIntToStr(const int i)
{
   stringstream ss;
   ss << i;
   return ss.str();
}

//----------------------------------------------------------------------------
int buildModel(SantaData & sData){

   int i,c,ct,k,g,gOrder,cOrder,offset; 
         
   //---
   //--- build the arcs (g,c)
   //---
   set< pair<int,int> > arcs;
   //child arcs
   offset = 0;
   for(c = 0; c < nChildren; c++){
      for(k = 0; k < childListLen; k++){
         g = childList[offset];
         arcs.insert(make_pair(g,c));
         offset++;
      }
   }
   cout << "Size of child arcs = " << arcs.size() << endl;
   //gift arcs
   offset = 0;
   for(g = 0; g < nGiftTypes; g++){
      for(k = 0; k < giftListLen; k++){
         c = giftList[offset];
         assert(c < nChildren);
         arcs.insert(make_pair(g,c));
         offset++;
      }
   }
   cout << "Size of child and gift arcs = " << arcs.size() << endl;
   //twin arcs
   for(c = twinBeg; c < twinEnd; c++){
      for(g = 0; g < nGiftTypes; g++){
         arcs.insert(make_pair(g,c));
      }
   }
   cout << "Size of child, gift and twins arcs = " << arcs.size() << endl;
   //dummy arcs
   for(c = twinEnd; c < nChildren; c++){
      arcs.insert(make_pair(dummyGift,c));
   }
   cout << "Size of child, gift, twins and dummy arcs = " << arcs.size() << endl;

   //convert to vector and free set memory
   vector< pair<int,int> > & arcsV = sData.arcsV;
   set< pair<int,int> >::iterator si;
   arcsV.reserve(arcs.size());
   for(si = arcs.begin(); si != arcs.end(); si++){
      arcsV.push_back(*si);
   }
   arcs.clear();

   int nCols = static_cast<int>(arcsV.size());
   double * colLB    = new double[nCols];
   double * colUB    = new double[nCols];
   double * objCoeff = new double[nCols];
   int    * intVars  = new int[nCols];
   assert(colLB && colUB && objCoeff && nCols);
#ifdef BUILD_NAMES
   vector<string> colNames;
   colNames.reserve(nCols);
   string colName;
#endif   
   
   //---
   //--- build columns
   //---
   for(i = 0; i < nCols; i++){
      colLB[i] = 0.0;
      colUB[i] = 1.0;
   }
   double childHappy, giftHappy;
   int    colIndex = 0;
   int * giftOrder = sData.giftOrder;
   int * childOrder = sData.childOrder;
   vector< pair<int,int> >::iterator vpi;   
   for(vpi = arcsV.begin(); vpi != arcsV.end(); vpi++){
      g = (*vpi).first;
      c = (*vpi).second;
      assert(c < nChildren);
      assert(g <= nGiftTypes);
#ifdef BUILD_NAMES
      colName = "Flow[" + UtilIntToStr(g) + "," + UtilIntToStr(c) + "]";
      colNames.push_back(colName);
#endif
      childHappy = 0;
      giftHappy  = 0;
      if(g == dummyGift){
         childHappy = -1;
         giftHappy  = -1;
      }
      else{
         gOrder = giftOrder[indexCG(c,g)];
         cOrder = childOrder[indexGC(g,c)];
         if(gOrder == -1)
            childHappy -= 1.0; 
         else
            childHappy += 2.0 * (childListLen-gOrder);      
         if(cOrder == -1)
            giftHappy -= 1.0;       
         else
            giftHappy += 2.0 * (giftListLen-cOrder);
      }
      objCoeff[colIndex] = 100 * childHappy + giftHappy;
      colIndex++;
   }
   


   //---
   //--- build rows
   //---
   int      nRows    = nGiftTypes + 1 + nChildren + (nGiftTypes * nTwins/2);
   double * rowLB    = new double[nRows];
   double * rowUB    = new double[nRows];
   assert(rowLB && rowUB);
#ifdef BUILD_NAMES
   vector<string> rowNames;
   rowNames.reserve(nRows);
   string rowName;
#endif

   CoinPackedMatrix M(false, 0.0, 0.0);
   M.setDimensions(0, nCols);
   M.reserve(nRows, nCols);

   //build slices
   vector<int> * arcsG = new vector<int>[nGiftTypes+1]; 
   vector<int> * arcsC = new vector<int>[nChildren];
   int nChildTwins = nTwins / 2;
   int * arcsTwins1 = new int[nGiftTypes * nChildTwins]; 
   int * arcsTwins2 = new int[nGiftTypes * nChildTwins];
   colIndex = 0;   
   for(vpi = arcsV.begin(); vpi != arcsV.end(); vpi++){
      g = (*vpi).first;
      c = (*vpi).second;      
      arcsG[g].push_back(colIndex);
      arcsC[c].push_back(colIndex);
      
      //Twins are c,c+1
      if(g < nGiftTypes){
         if(c < nTwins){
            ct = c / 2;
            if(c % 2 == 0){
               arcsTwins1[g*nChildTwins+ct] = colIndex;//(g,c1)
            }
            else{
               arcsTwins2[g*nChildTwins+ct] = colIndex;//(g,c2)
            }
         }
      }
         
      colIndex++;
   }
   cout << "Done building slices." << endl;

   int rowIndex = 0;
   vector<int>::iterator vi;
   //supply constraints
   for(g = 0; g <= nGiftTypes; g++){
      CoinPackedVector row;
      for(vi = arcsG[g].begin(); vi != arcsG[g].end(); vi++)
         row.insert(*vi,1.0);
      M.appendRow(row);
      rowLB[rowIndex] = 0.0;
      rowUB[rowIndex] = nGiftsPerType;
#ifdef BUILD_NAMES
      rowName = "SupplyCon[" + UtilIntToStr(g) + "]";
      rowNames.push_back(rowName);
#endif
      rowIndex++;
   }
   rowUB[nGiftTypes] = nChildren - nTwins;
   cout << "Done building supply constraints rowIndex=" << rowIndex << endl;

   //demand constraints
   vector<int>     beg;
   vector<int>     ind;
   vector<double>  els;
   int     rowLen  = 0;
   int     elIndex = 0;
   beg.push_back(0);
   for(c = 0; c < nChildren; c++){
      //CoinPackedVector row;
      rowLen = 0;
      for(vi = arcsC[c].begin(); vi != arcsC[c].end(); vi++){
         ind.push_back(*vi);
         els.push_back(1.0);
         elIndex++;
         rowLen++;
      }
      beg.push_back(beg[c] + rowLen);
      //row.insert(*vi,1.0);
      //M.appendRow(row);//This is going to be very slow.
      rowLB[rowIndex] = 1.0;
      rowUB[rowIndex] = 1.0;
#ifdef BUILD_NAMES
      rowName = "DemandCon[" + UtilIntToStr(c) + "]";
      rowNames.push_back(rowName);
#endif
      rowIndex++;
   }
   M.appendRows(nChildren, &beg[0], &ind[0], &els[0]);
   cout << "Done building demand constraints rowIndex=" << rowIndex << endl;
   cout << "Number of nonzeros = " << M.getNumElements() << endl;

   //twin cons
   beg.clear();
   ind.clear();
   els.clear();
   elIndex = 0;
   beg.push_back(0);
   int rowIndexT = 0;
   for(g = 0; g < nGiftTypes; g++){
      for(c = twinBeg; c < nChildTwins; c++){
         ind.push_back(arcsTwins1[g*nChildTwins+c]);
         ind.push_back(arcsTwins2[g*nChildTwins+c]);
         els.push_back(1.0);
         els.push_back(-1.0);
         beg.push_back(beg[rowIndexT] + 2);
         rowLB[rowIndex] = 0.0;
         rowUB[rowIndex] = 0.0;
#ifdef BUILD_NAMES
         rowName = "TwinCon[" + UtilIntToStr(g) + "," 
            + UtilIntToStr(2*c) + "," 
            + UtilIntToStr(2*c+1) + "]";
         rowNames.push_back(rowName);
#endif
         rowIndex++;
         rowIndexT++;
      }
   }
   M.appendRows(rowIndexT, &beg[0], &ind[0], &els[0]);
   cout << "Done building twin constraints rowIndex=" << rowIndex << endl;
   cout << "Number of nonzeros = " << M.getNumElements() << endl;
     
   OsiSolverInterface * osi = sData.osi;
   osi->loadProblem(M,
                    colLB,
                    colUB,
                    objCoeff,
                    rowLB,
                    rowUB);
   for(i = 0; i < nCols; i++)
      intVars[i] = i;
   osi->setInteger(intVars, nCols);
#ifdef BUILD_NAMES
   osi->setIntParam(OsiNameDiscipline,1);
   osi->setObjName("TotalHappiness");
   osi->setColNames(colNames,0,nCols,0);
   osi->setRowNames(rowNames,0,nRows,0);
   //   osi->writeMps("/ORDATA_NEW/user/magala/santaCBC3");
   osi->writeMps("santaTst");
#endif
   delete [] colLB;
   delete [] colUB;
   delete [] objCoeff;
   delete [] intVars;
   delete [] rowLB;
   delete [] rowUB;
   delete [] arcsC;
   delete [] arcsG;
   delete [] arcsTwins1;
   delete [] arcsTwins2;

   return 0;
}

//----------------------------------------------------------------------------
int main(int argc, char ** argv){
   SantaData sData;
   clock_t start,end;
   double cpu_time_used;
   
   start=clock();
   readChildListFile(sData);
   readGiftListFile(sData);
   //readSolutionFile(sData);
   //memcpy(solutionNew, solutionCtoG, nChildren*sizeof(int));
   end=clock();
   cpu_time_used = ((double)(end-start))/CLOCKS_PER_SEC;
   cout << "Time used = " << cpu_time_used << endl;
   
   start=clock();
   buildModel(sData);
   end=clock();
   cpu_time_used = ((double)(end-start))/CLOCKS_PER_SEC;
   cout << "Time used = " << cpu_time_used << endl;

   start=clock();
   solveModel(sData);
   end=clock();
   cpu_time_used = ((double)(end-start))/CLOCKS_PER_SEC;
   cout << "Time used = " << cpu_time_used << endl;

   start=clock();
   calculateScore(sData);
   end=clock();
   cpu_time_used = ((double)(end-start))/CLOCKS_PER_SEC;
   cout << "Time used = " << cpu_time_used << endl;

   start=clock();
   dumpSolutionFile(sData);
   end=clock();
   cpu_time_used = ((double)(end-start))/CLOCKS_PER_SEC;
   cout << "Time used = " << cpu_time_used << endl;
}
