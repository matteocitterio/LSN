/****************************************************************
*****************************************************************
    _/    _/  _/_/_/  _/       Numerical Simulation Laboratory
   _/_/  _/ _/       _/       Physics Department
  _/  _/_/    _/    _/       Universita' degli Studi di Milano
 _/    _/       _/ _/       Prof. D.E. Galli
_/    _/  _/_/_/  _/_/_/_/ email: Davide.Galli@unimi.it
*****************************************************************
*****************************************************************/

#include <iostream>
#include <fstream>
#include <ostream>
#include <cmath>
#include <iomanip>
#include <vector>
#include "MD_MC.h"

using namespace std;

int main(int argc, char *argv[]){

  /*
  I want to add command line arguments so that the program knows if equilibration is needed and where to write files
  */

  if (argc != 4){

    // Raise an error if the command lines arguments are wrong
    std::cerr << "Usage: " << argv[0] << " [bool(equilibration)] [Liquid|Solid|Gas] [int fileNumber]" << std::endl;
    return 1;

  }

  // Read the command line argurments
  int equilibration = stoi(argv[1]);                                          // Equilibration argument: 0/1
  string input = argv[2];                                                     // Phase name (string)
  int file_number = stoi(argv[3]);                                            // The number of the input file (used in equilibration_routine.sh)

  Input(equilibration, input, file_number);                                   // Inizialization & equilibration
  int nconf = 1;
  for(int iblk=1; iblk <= nblk; iblk++)                                       // Simulation
  {

    Reset(iblk);                                                              // Reset block averages

    for(int istep=1; istep <= nstep; istep++)
    {
      Move();                                                                 // Evolve the system
      Measure();                                                              // Measure insteresting quantities
      Accumulate();                                                           // Update block averages
      if(istep%10 == 0){
//      ConfXYZ(nconf);//Write actual configuration in XYZ format //Commented to avoid "filesystem full"! 
        nconf += 1;
      }
    }
    Averages(iblk, input, equilibration);                                     // Print results for current block in the output file
  }
  ConfFinal(input);                                                           // Write final configuration

  return 0;
}


void Input(int equilibration, std::string input, int file_number) {

  /*
  Reads and store the input parameters from the `input.in` format-like file. Carries out the needed equilibration as well.
  Inputs:
  - int equilibration: bool to control the equilibration routine, wheter it is needed or not
  - string input: needed to select the right folder for I/O managment
  - int file_number: if we are running `equilibration.sh` this will select the right `input.in` file with the correct temperature and parameters
  */

  ifstream ReadInput, ReadConf, ReadVelocity, Primes, Seed;                   // I/O managment files

  // User-friendly messages
  cout << "Classic Lennard-Jones fluid        " << endl;
  cout << "MD(NVE) / MC(NVT) simulation       " << endl << endl;
  cout << "Interatomic potential v(r) = 4 * [(1/r)^12 - (1/r)^6]" << endl << endl;
  cout << "Boltzmann weight exp(- beta * sum_{i<j} v(r_ij) ), beta = 1/T " << endl << endl;
  cout << "The program uses Lennard-Jones units " << endl;

  // Read seed for random numbers
  int p1, p2;
  Primes.open("Primes");
  Primes >> p1 >> p2 ;
  Primes.close();

  // Read input informations
  if (equilibration == 0) ReadInput.open("input.in");                         // Equilibration already done, go on with the simulation
  else {                                                                      // Read equilibration input file
    ifstream infile("./" + input + "/inputs/filenames.txt");                  // Contains the name of the files
    vector<string> filenames;

    if (infile){                                                              // Read file names
      string line;
      while (getline(infile, line)){
        filenames.push_back(line);
      }
    }
    infile.close();

    string filename = filenames[file_number - 1];                             // name of the input file needed
    cout <<"Doing equilibration with: " << filename << endl;

    ReadInput.open(filename);                                                 // open the proper file
  }

  // Read input parameters from file
  ReadInput >> iNVET;
  ReadInput >> restart;

  if(restart) Seed.open("seed.out");
  else Seed.open("seed.in");
  Seed >> seed[0] >> seed[1] >> seed[2] >> seed[3];
  rnd.SetRandom(seed,p1,p2);
  Seed.close();

  ReadInput >> temp;
  beta = 1.0/temp;
  cout << "Temperature = " << temp << endl;

  ReadInput >> npart;
  cout << "Number of particles = " << npart << endl;

  ReadInput >> rho;
  cout << "Density of particles = " << rho << endl;
  vol = (double)npart/rho;
  box = pow(vol,1.0/3.0);
  cout << "Volume of the simulation box = " << vol << endl;
  cout << "Edge of the simulation box = " << box << endl;

  ReadInput >> rcut;
  cout << "Cutoff of the interatomic potential = " << rcut << endl << endl;
    
  ReadInput >> delta;
  ReadInput >> nblk;
  ReadInput >> nstep;

  cout << "The program perform Metropolis moves with uniform translations" << endl;
  cout << "Moves parameter = " << delta << endl;
  cout << "Number of blocks = " << nblk << endl;
  cout << "Number of steps in one block = " << nstep << endl << endl;
  ReadInput.close();

  // Prepare arrays for measurements
  iv = 0;                                                                     // Potential energy
  it = 1;                                                                     // Temperature
  ik = 2;                                                                     // Kinetic energy
  ie = 3;                                                                     // Total energy
  iw = 4;                                                                     // Pressure
  //n_props = 5;                                                              // Number of observables, already defined in `*.h` file


  // Read initial configuration
  cout << "Read initial configuration" << endl << endl;
  if(restart)
  {
    ReadConf.open(input+"/config.out");
    ReadVelocity.open(input+"/velocity.out");
    for (int i=0; i<npart; ++i) ReadVelocity >> vx[i] >> vy[i] >> vz[i];
  }
  else {
    ReadConf.open("config.in");
    cout << "Prepare velocities with center of mass velocity equal to zero " << endl;
    double sumv[3] = {0.0, 0.0, 0.0};
    for (int i=0; i<npart; ++i)
    {
      vx[i] = rnd.Gauss(0.,sqrt(temp));
      vy[i] = rnd.Gauss(0.,sqrt(temp));
      vz[i] = rnd.Gauss(0.,sqrt(temp));
      sumv[0] += vx[i];
      sumv[1] += vy[i];
      sumv[2] += vz[i];
    }
    for (int idim=0; idim<3; ++idim) sumv[idim] /= (double)npart;
    double sumv2 = 0.0, fs;
    for (int i=0; i<npart; ++i)
    {
      vx[i] = vx[i] - sumv[0];
      vy[i] = vy[i] - sumv[1];
      vz[i] = vz[i] - sumv[2];
      sumv2 += vx[i]*vx[i] + vy[i]*vy[i] + vz[i]*vz[i];
    }
    sumv2 /= (double)npart;
    fs = sqrt(3 * temp / sumv2);   // fs = velocity scale factor 
    cout << "velocity scale factor: " << fs << endl << endl;
    for (int i=0; i<npart; ++i)
    {
      vx[i] *= fs;
      vy[i] *= fs;
      vz[i] *= fs;
    }
  }

  // Apply PBCs to positions

  for (int i=0; i<npart; ++i)
  {
    ReadConf >> x[i] >> y[i] >> z[i];
    x[i] = Pbc( x[i] * box );
    y[i] = Pbc( y[i] * box );
    z[i] = Pbc( z[i] * box );
  }
  ReadConf.close();

  for (int i=0; i<npart; ++i)
  {
    if(iNVET)
    {
      xold[i] = x[i];
      yold[i] = y[i];
      zold[i] = z[i];
    }
    else
    {
      xold[i] = Pbc(x[i] - vx[i] * delta);
      yold[i] = Pbc(y[i] - vy[i] * delta);
      zold[i] = Pbc(z[i] - vz[i] * delta);
    }
  }
  
  // Evaluate properties of the initial configuration
  Measure();

  // Print initial values for measured properties
  cout << "Initial potential energy = " << walker[iv] / (double)npart << endl;
  cout << "Initial temperature      = " << walker[it] << endl;
  cout << "Initial kinetic energy   = " << walker[ik] / (double)npart << endl;
  cout << "Initial total energy     = " << walker[ie] / (double)npart << endl;
  cout << "Initial total pressure   = " << walker[iw] << endl;

  return;
}


void Move()
{
  int o;
  double p, energy_old, energy_new;
  double xnew, ynew, znew;

  if(iNVET) // Monte Carlo (NVT) move
  {
    for(int i=0; i<npart; ++i)
    {
    //Select randomly a particle (for C++ syntax, 0 <= o <= npart-1)
      o = (int)(rnd.Rannyu()*npart);

    //Old
      energy_old = Boltzmann(x[o],y[o],z[o],o);

    //New
      x[o] = Pbc( x[o] + delta*(rnd.Rannyu() - 0.5) );
      y[o] = Pbc( y[o] + delta*(rnd.Rannyu() - 0.5) );
      z[o] = Pbc( z[o] + delta*(rnd.Rannyu() - 0.5) );

      energy_new = Boltzmann(x[o],y[o],z[o],o);

    //Metropolis test
      p = exp(beta*(energy_old-energy_new));
      if(p >= rnd.Rannyu())  
      {
      //Update
        xold[o] = x[o];
        yold[o] = y[o];
        zold[o] = z[o];
        accepted = accepted + 1.0;
      } else {
        x[o] = xold[o];
        y[o] = yold[o];
        z[o] = zold[o];
      }
      attempted = attempted + 1.0;
    }
  } else // Molecular Dynamics (NVE) move
  {
    double fx[m_part], fy[m_part], fz[m_part];

    for(int i=0; i<npart; ++i){ //Force acting on particle i
      fx[i] = Force(i,0);
      fy[i] = Force(i,1);
      fz[i] = Force(i,2);
    }

    for(int i=0; i<npart; ++i){ //Verlet integration scheme

      xnew = Pbc( 2.0 * x[i] - xold[i] + fx[i] * pow(delta,2) );
      ynew = Pbc( 2.0 * y[i] - yold[i] + fy[i] * pow(delta,2) );
      znew = Pbc( 2.0 * z[i] - zold[i] + fz[i] * pow(delta,2) );

      vx[i] = Pbc(xnew - xold[i])/(2.0 * delta);
      vy[i] = Pbc(ynew - yold[i])/(2.0 * delta);
      vz[i] = Pbc(znew - zold[i])/(2.0 * delta);

      xold[i] = x[i];
      yold[i] = y[i];
      zold[i] = z[i];

      x[i] = xnew;
      y[i] = ynew;
      z[i] = znew;

      accepted = accepted + 1.0;
      attempted = attempted + 1.0;
    }
  }
  return;
}

double Boltzmann(double xx, double yy, double zz, int ip)
{
  double ene=0.0;
  double dx, dy, dz, dr;

  for (int i=0; i<npart; ++i)
  {
    if(i != ip)
    {
// distance ip-i in pbc
      dx = Pbc(xx - x[i]);
      dy = Pbc(yy - y[i]);
      dz = Pbc(zz - z[i]);

      dr = dx*dx + dy*dy + dz*dz;
      dr = sqrt(dr);

      if(dr < rcut)
      {
        ene += 1.0/pow(dr,12) - 1.0/pow(dr,6);
      }
    }
  }

  return 4.0*ene;
}

double Force(int ip, int idir){ //Compute forces as -Grad_ip V(r)
  double f=0.0;
  double dvec[3], dr;

  for (int i=0; i<npart; ++i){
    if(i != ip){
      dvec[0] = Pbc( x[ip] - x[i] );  // distance ip-i in pbc
      dvec[1] = Pbc( y[ip] - y[i] );
      dvec[2] = Pbc( z[ip] - z[i] );

      dr = dvec[0]*dvec[0] + dvec[1]*dvec[1] + dvec[2]*dvec[2];
      dr = sqrt(dr);

      if(dr < rcut){
        f += dvec[idir] * (48.0/pow(dr,14) - 24.0/pow(dr,8)); // -Grad_ip V(r)
      }
    }
  }
 
  return f;
}

void Measure() //Properties measurement
{
  double v = 0.0, kin=0.0, press = 0.0;
  double vij;
  double pij;
  double dx, dy, dz, dr;

  // cycle over pairs of particles
  for (int i=0; i<npart-1; ++i)
  {
    for (int j=i+1; j<npart; ++j)
    {
// distance i-j in pbc
      dx = Pbc(x[i] - x[j]);
      dy = Pbc(y[i] - y[j]);
      dz = Pbc(z[i] - z[j]);

      dr = dx*dx + dy*dy + dz*dz;
      dr = sqrt(dr);

      if(dr < rcut)
      {
        vij = 1.0/pow(dr,12) - 1.0/pow(dr,6);
        v += vij;
        pij = 1.0 / pow(dr, 12) - 0.5 / pow(dr, 6);
        press += pij;    
      }
    }          
  }

  for (int i=0; i<npart; ++i) kin += 0.5 * (vx[i]*vx[i] + vy[i]*vy[i] + vz[i]*vz[i]);             // Add kinetic energy

  walker[iv] = 4.0 * v;                                                                           // Potential energy
  walker[ik] = kin;                                                                               // Kinetic energy
  walker[it] = (2.0 / 3.0) * kin/(double)npart;                                                   // Temperature
  walker[ie] = 4.0 * v + kin;                                                                     // Total energy;
  walker[iw] = walker[it] * rho + 48 / (3 * vol) * press;                                         // pressure

  return;
}

void Reset(int iblk){

  /*
  Reset block averages
  */

  if (iblk == 1)
  {
    for (int i = 0; i < m_props; ++i)
    {
      glob_av[i] = 0;
      glob_av2[i] = 0;
    }
  }

  for (int i = 0; i < n_props; ++i)                                                               // For the Gr we will use m_props
  {
    blk_av[i] = 0;
  }
  blk_norm = 0;
  attempted = 0;
  accepted = 0;
}

void Accumulate(void){

  /*
  Update block averages
  */

  for (int i = 0; i < n_props; ++i)
  {
    blk_av[i] = blk_av[i] + walker[i];
  }
  blk_norm = blk_norm + 1.0;
}

void Averages(int iblk, string input, int equilibration) {

  /*
  Print results for current block and I/O managment
  */

  ofstream Epot, Ekin, Etot, Temp, Press;
  const int wd = 12;

  cout << "Block number " << iblk << endl;
  cout << "Acceptance rate " << accepted / attempted << endl << endl;

  if (equilibration == 1)
  {
    input = input + "/Equilibration";                                                               // if we are doing equilibration

    Epot.open("./" + input + "/output_epot" + std::to_string(temp) + ".dat", ios::app);
    Ekin.open("./" + input + "/output_ekin" + std::to_string(temp) + ".dat", ios::app);
    Temp.open("./" + input + "/output_temp" + std::to_string(temp) + ".dat", ios::app);
    Etot.open("./" + input + "/output_etot" + std::to_string(temp) + ".dat", ios::app);
    Press.open("./" + input + "/output_press" + std::to_string(temp) + ".dat", ios::app);
    }

    else {
      Epot.open("./" + input + "/output_epot.dat", ios::app);
      Ekin.open("./" + input + "/output_ekin.dat", ios::app);
      Temp.open("./" + input + "/output_temp.dat", ios::app);
      Etot.open("./" + input + "/output_etot.dat", ios::app);
      Press.open("./" + input + "/output_press.dat", ios::app);
    }

    stima_pot = blk_av[iv]/blk_norm/(double)npart;                                                   // Potential energy per particle
    glob_av[iv] += stima_pot;                      
    glob_av2[iv] += stima_pot*stima_pot;
    err_pot=Error(glob_av[iv],glob_av2[iv],iblk);
    
    stima_kin = blk_av[ik]/blk_norm/(double)npart;                                                   // Kinetic energy
    glob_av[ik] += stima_kin;
    glob_av2[ik] += stima_kin*stima_kin;
    err_kin=Error(glob_av[ik],glob_av2[ik],iblk);

    stima_etot = blk_av[ie]/blk_norm/(double)npart;                                                   // Total energy
    glob_av[ie] += stima_etot;
    glob_av2[ie] += stima_etot*stima_etot;
    err_etot=Error(glob_av[ie],glob_av2[ie],iblk);

    stima_temp = blk_av[it]/blk_norm;                                                                 // Temperature
    glob_av[it] += stima_temp;
    glob_av2[it] += stima_temp*stima_temp;
    err_temp=Error(glob_av[it],glob_av2[it],iblk);

    stima_pres = blk_av[iw] / blk_norm;                                                               // Pressure
    glob_av[iw] += stima_pres;
    glob_av2[iw] += stima_pres * stima_pres;
    err_press = Error(glob_av[iw], glob_av2[iw], iblk);

    // Potential energy per particle
    Epot << setw(wd) << iblk <<  setw(wd) << stima_pot << setw(wd) << glob_av[iv]/(double)iblk << setw(wd) << err_pot << endl;
    // Kinetic energy
    Ekin << setw(wd) << iblk <<  setw(wd) << stima_kin << setw(wd) << glob_av[ik]/(double)iblk << setw(wd) << err_kin << endl;
    // Total energy
    Etot << setw(wd) << iblk <<  setw(wd) << stima_etot << setw(wd) << glob_av[ie]/(double)iblk << setw(wd) << err_etot << endl;
    // Temperature
    Temp << setw(wd) << iblk <<  setw(wd) << stima_temp << setw(wd) << glob_av[it]/(double)iblk << setw(wd) << err_temp << endl;
    // Pressure
    Press << setw(wd) << iblk << setw(wd) << stima_pres << setw(wd) << glob_av[iw] / (double)iblk << setw(wd) << err_press << endl;

    cout << "----------------------------" << endl << endl;

    Epot.close();
    Ekin.close();
    Etot.close();
    Temp.close();
    Press.close();
}


void ConfFinal(string input)
{
  ofstream WriteConf, WriteVelocity, WriteSeed;

  cout << "Print final configuration to file config.out" << endl << endl;
  WriteConf.open(input+"/config.out");
  WriteVelocity.open(input+"/velocity.out");
  for (int i=0; i<npart; ++i)
  {
    WriteConf << x[i]/box << "   " <<  y[i]/box << "   " << z[i]/box << endl;
    WriteVelocity << vx[i] << "   " <<  vy[i] << "   " << vz[i] << endl;
  }
  WriteConf.close();
  WriteVelocity.close();

  rnd.SaveSeed();
}

void ConfXYZ(int nconf){ //Write configuration in .xyz format
  ofstream WriteXYZ;

  WriteXYZ.open("frames/config_" + to_string(nconf) + ".xyz");
  WriteXYZ << npart << endl;
  WriteXYZ << "This is only a comment!" << endl;
  for (int i=0; i<npart; ++i){
    WriteXYZ << "LJ  " << Pbc(x[i]) << "   " <<  Pbc(y[i]) << "   " << Pbc(z[i]) << endl;
  }
  WriteXYZ.close();
}

double Pbc(double r)  //Algorithm for periodic boundary conditions with side L=box
{
    return r - box * rint(r/box);
}

double Error(double sum, double sum2, int iblk)
{
    return sqrt(fabs(sum2/(double)iblk - pow(sum/(double)iblk,2))/(double)iblk);
}

/****************************************************************
*****************************************************************
    _/    _/  _/_/_/  _/       Numerical Simulation Laboratory
   _/_/  _/ _/       _/       Physics Department
  _/  _/_/    _/    _/       Universita' degli Studi di Milano
 _/    _/       _/ _/       Prof. D.E. Galli
_/    _/  _/_/_/  _/_/_/_/ email: Davide.Galli@unimi.it
*****************************************************************
*****************************************************************/
