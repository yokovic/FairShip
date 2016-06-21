#include <math.h>
#include "TROOT.h"
#include "FairPrimaryGenerator.h"
#include "Pythia8/Pythia.h"

#include "Pythia8Generator.h"
#include "HNLPythia8Generator.h"
const Double_t cm = 10.; // pythia units are mm
int counter = 0;
using namespace Pythia8;

// -----   Default constructor   -------------------------------------------
Pythia8Generator::Pythia8Generator() 
{
  fUseRandom1 = kFALSE; 
  fUseRandom3 = kTRUE;
  fId         = 2212; // proton
  fMom        = 400;  // proton
  fFDs        = 7.7/10.4;    // correction for Pythia6 to match measured Ds production
  fextFile    = "";
  fInputFile  = NULL;
  fLogger = FairLogger::GetLogger();
  fPythia =  new Pythia8::Pythia();
}
// -------------------------------------------------------------------------

// -----   Default constructor   -------------------------------------------
Bool_t Pythia8Generator::Init() 
{
  if (fUseRandom1) fRandomEngine = new PyTr1Rng();
  if (fUseRandom3) fRandomEngine = new PyTr3Rng();
  if (fextFile != ""){
    if (0 == strncmp("/eos",fextFile,4) ) {
     char stupidCpp[100];
     strcpy(stupidCpp,"root://eoslhcb.cern.ch/");
     strcat(stupidCpp,fextFile);
     fLogger->Info(MESSAGE_ORIGIN,"Open external file with charm or beauty hadrons on eos: %s",stupidCpp);
     fInputFile  = TFile::Open(stupidCpp); 
     if (!fInputFile) {
      fLogger->Fatal(MESSAGE_ORIGIN, "Error opening input file. You may have forgotten to provide a krb5 token. Try kinit username@lxplus.cern.ch");
      return kFALSE; }
    }else{
      fLogger->Info(MESSAGE_ORIGIN,"Open external file with charm or beauty hadrons: %s",fextFile);
      fInputFile  = new TFile(fextFile);
      if (!fInputFile) {
       fLogger->Fatal(MESSAGE_ORIGIN, "Error opening input file");
     return kFALSE; }
    }
    if (fInputFile->IsZombie()) {
     fLogger->Fatal(MESSAGE_ORIGIN, "File is corrupted");
     return kFALSE; }
     fTree = (TTree *)fInputFile->Get("pythia6");
     fNevents = fTree->GetEntries();
     fn = firstEvent;
     fTree->SetBranchAddress("id",&hid);                // particle id
     fTree->SetBranchAddress("px",&hpx);   // momentum
     fTree->SetBranchAddress("py",&hpy);
     fTree->SetBranchAddress("pz",&hpz);
     fTree->SetBranchAddress("E",&hE);     
     fTree->SetBranchAddress("M",&hM);     
     fTree->SetBranchAddress("mid",&mid);   // mother
     fTree->SetBranchAddress("mpx",&mpx);   // momentum
     fTree->SetBranchAddress("mpy",&mpy);
     fTree->SetBranchAddress("mpz",&mpz);
     fTree->SetBranchAddress("mE",&mE);
     fPythia->readString("ProcessLevel:all = off");
     fPythia->readString("130:mayDecay  = off");
     fPythia->readString("310:mayDecay  = off");
     fPythia->readString("3122:mayDecay = off");
     fPythia->readString("3222:mayDecay = off");
  } else {  
   fPythia->setRndmEnginePtr(fRandomEngine);
   fPythia->settings.mode("Beams:idA",  fId);
   fPythia->settings.mode("Beams:idB",  2212);
   fPythia->settings.mode("Beams:frameType",  2);
   fPythia->settings.parm("Beams:eA",fMom);
   fPythia->settings.parm("Beams:eB",0.);
  }
  fPythia->init();
  return kTRUE;
}
// -------------------------------------------------------------------------


// -----   Destructor   ----------------------------------------------------
Pythia8Generator::~Pythia8Generator() 
{
}
// -------------------------------------------------------------------------

// -----   Passing the event   ---------------------------------------------
Bool_t Pythia8Generator::ReadEvent(FairPrimaryGenerator* cpg)
{
  Double_t x,y,z,px,py,pz,dl;
  Int_t im,id,key;
// take charm or beauty hadron from external file
// correct for too much Ds produced by pythia6
   bool l = true; 
   while(l){ 
     if (fn==fNevents) {fLogger->Warning(MESSAGE_ORIGIN, "End of input file. Rewind.");}
     fTree->GetEntry(fn%fNevents);
     fn++;
// check that entry is charm, otherwise continue reading
     if (mid[0]< 0){ l = false; }   
     else if ( int(fabs(hid[0]) ) != 431){ l = false; }
     else {
       Double_t rnr = gRandom->Uniform(0,1);
       if( rnr<fFDs ) { l = false; };
     }
   }          
   fPythia->event.reset();
   id = (Int_t)hid[0];
   fPythia->event.append( id, 1, 0, 0, hpx[0],  hpy[0],  hpz[0],  hE[0],  hM[0], 0., 9. );
//simulate displaced vertex, Pythia8 will not do it
   Double_t tau0 = fPythia->particleData.tau0(id); // ctau in mm
   dl = gRandom->Exp(tau0) / hM[0]; // mm/GeV    
   
  fPythia->next();
  counter+=1; 
// copy complete pythia6 and pythia8 event  
  px=mpx[0];
  py=mpy[0];
  pz=mpz[0];
  x=0.;
  y=0.;
  z=0.;
  id=mid[0];
  key=0;
  bool lx = true;
  bool first = true;
  while(lx){      
    fTree->GetEntry(fn%fNevents);
    if (mE[0] == 0){
     fn++;
     if (first){
      first = false; 
      x=mpx[0]; 
      y=mpy[0]; 
      z=mpz[0]; 
     }
     cpg->AddTrack((Int_t)hid[0],hpx[0],hpy[0],hpz[0],(mpx[0]+fPythia->event[0].xProd())/cm,
                                                      (mpy[0]+fPythia->event[0].yProd())/cm,
                                                      (mpz[0]+fPythia->event[0].zProd())/cm,-1,true);
     // mpx,mpy,mpz are the vertex coordinates with respect to charm hadron, first particle in Pythia is (system) 
     key++;
    } else {
     lx = false;
    }
  } 
  cpg->AddTrack(id,px,py,pz,x/cm,y/cm,z/cm,-1,false);

  for(Int_t ii=0; ii<fPythia->event.size(); ii++){
    id = fPythia->event[ii].id(); 
    if (id==90){continue;}
    Bool_t wanttracking=false;
    if(fPythia->event[ii].isFinal()){ wanttracking=true; }
    if (ii>1){
     z  = fPythia->event[ii].zProd()+dl*fPythia->event[1].pz();
     x  = fPythia->event[ii].xProd()+dl*fPythia->event[1].px();
     y  = fPythia->event[ii].yProd()+dl*fPythia->event[1].py();
    }else{
     z  = fPythia->event[ii].zProd();
     x  = fPythia->event[ii].xProd();
     y  = fPythia->event[ii].yProd();
    }
    pz = fPythia->event[ii].pz();
    px = fPythia->event[ii].px();  
    py = fPythia->event[ii].py();  
    im = fPythia->event[ii].mother1()+key;
    if (ii==0){im = key;}
    cpg->AddTrack(id,px,py,pz,x/cm,y/cm,z/cm,im,wanttracking);
  } 

  return kTRUE;
}
// -------------------------------------------------------------------------

ClassImp(Pythia8Generator)
