#include <iostream>
#include <iomanip>
#include <string>
#include<getopt.h>

#include <particle_simulator.hpp>
#define HARD_DEBUG_PRINT_FEQ 1024

#include "io.hpp"
#include "hard_assert.hpp"
#include "cluster_list.hpp"
#include "hard.hpp"
#include "soft_ptcl.hpp"
#include "static_variables.hpp"

int main(int argc, char **argv){
  int arg_label;
  int mode=0; // 0: integrate to time; 1: times stability
  int idum=0;
  PS::F64 slowdown_factor=0;
  PS::F64 eta_4th=0;
  PS::F64 eta_2nd=0;
  PS::F64 e_err_ar = -1;
  PS::F64 e_err_hard = 1e-4;
  PS::S32 dt_min_power = -1;
  PS::F64 dt_max = -1;
  PS::S32 step_arc_limit = 100000;
  std::string filename="hard_dump";
  std::string fhardpar="input.par.hard";
#ifdef BSE
  std::string fbsepar = "input.par.bse";
#endif

  while ((arg_label = getopt(argc, argv, "k:E:A:a:D:d:e:s:m:b:i:h")) != -1)
    switch (arg_label) {
    case 'k':
        slowdown_factor = atof(optarg);
        break;
    case 'E':
        eta_4th = atof(optarg);
        break;
    case 'A':
        eta_2nd = atof(optarg);
        break;
    case 'a':
        e_err_ar = atof(optarg);
        break;
    case 'D':
        dt_max = atof(optarg);
        break;
    case 'd':
        dt_min_power = atoi(optarg);
        break;
#ifdef HARD_CHECK_ENERGY
    case 'e':
        e_err_hard = atof(optarg);
        break;
#endif
    case 's':
        step_arc_limit = atoi(optarg);
        break;
    case 'm':
        mode = atoi(optarg);
        break;
    case 'p':
        fhardpar = optarg;
        break;
#ifdef BSE
    case 'i':
        idum = atoi(optarg);
        break;
    case 'b':
        fbsepar = optarg;
        break;
#endif
    case 'h':
        std::cout<<"hard_debug.out [options] [hard_manager (defaulted: input.par.hard)] [cluster_data] (defaulted: hard_dump)\n"
                 <<"options:\n"
                 <<"    -k [double]:  change slowdown factor\n"
#ifdef HARD_CHECK_ENERGY
                 <<"    -e [double]:  hard energy limit\n"
#endif
                 <<"    -s [int]:     AR step count limit\n"
                 <<"    -E [double]:  Eta 4th for hermite \n"
                 <<"    -A [double]:  Eta 2nd for hermite \n"
                 <<"    -a [double]:  AR energy limit \n"
                 <<"    -D [double]:  hard time step max \n"
                 <<"    -d [int]:     hard time step min power\n"
                 <<"    -m [int]:     running mode: 0: evolve system to time_end; 1: stability check: "<<mode<<std::endl
                 <<"    -p [string]:  hard parameter file name: "<<fhardpar<<std::endl
#ifdef BSE
                 <<"    -i [int]      random seed to generate kick velocity\n"
                 <<"    -b [string]:  bse parameter file name: "<<fbsepar<<std::endl
#endif
                 <<"    -h         :  help\n";
        return 0;
    default:
        std::cerr<<"Unknown argument. check '-h' for help.\n";
        abort();
    }

  if (optind<argc) {
      filename=argv[argc-1];
  }

  std::cerr<<"Reading dump file:"<<filename<<std::endl;
  std::cerr<<"Hard manager parameter file:"<<fhardpar<<std::endl;

  std::cout<<std::setprecision(WRITE_PRECISION);

  HardManager hard_manager;
  FILE* fpar_in;
  if( (fpar_in = fopen(fhardpar.c_str(),"r")) == NULL) {
      fprintf(stderr,"Error: Cannot open file %s.\n", fhardpar.c_str());
      abort();
  }
  hard_manager.readBinary(fpar_in);
  fclose(fpar_in);

#ifdef STELLAR_EVOLUTION
#ifdef BSE
  std::cerr<<"BSE parameter file:"<<fbsepar<<std::endl;
  if( (fpar_in = fopen(fbsepar.c_str(),"r")) == NULL) {
      fprintf(stderr,"Error: Cannot open file %s.\n", fbsepar.c_str());
      abort();
  }
  IOParamsBSE bse_io;
  bse_io.input_par_store.readAscii(fpar_in);
  fclose(fpar_in);
  if (idum!=0) bse_io.idum.value = idum;
  hard_manager.ar_manager.interaction.bse_manager.initial(bse_io);

  if (hard_manager.ar_manager.interaction.stellar_evolution_write_flag) {
      hard_manager.ar_manager.interaction.fout_sse.open((filename+".sse").c_str(), std::ofstream::out);
      hard_manager.ar_manager.interaction.fout_bse.open((filename+".bse").c_str(), std::ofstream::out);
  }
#endif
#endif

#ifdef HARD_CHECK_ENERGY
  // Set hard energy limit
  if (e_err_hard>0 )
      hard_manager.energy_error_max = e_err_hard;
#endif

  // Set step limit for ARC sym
  if (step_arc_limit>0) 
      hard_manager.ar_manager.step_count_max = step_arc_limit;

  // set slowdown factor
  if(slowdown_factor>0)    
      hard_manager.ar_manager.slowdown_pert_ratio_ref = slowdown_factor;

  // set eta
  if(eta_4th>0) {
      hard_manager.h4_manager.step.eta_4th = eta_4th;
  }

  if(eta_2nd>0) {
      hard_manager.h4_manager.step.eta_2nd = eta_2nd;
  }

  // time step
  if(dt_min_power>0&&dt_max>0) 
      hard_manager.setDtRange(dt_max, dt_min_power);

  if(e_err_ar>0) 
      hard_manager.ar_manager.energy_error_relative_max = e_err_ar;

  hard_manager.checkParams();
  hard_manager.print(std::cerr);

  HardDump hard_dump;
  hard_dump.readOneCluster(filename.c_str());
  std::cerr<<"Dt: "<<hard_dump.time_end<<std::endl;

  // running mode
  if (mode==0) {
      //SystemHard sys;
      //sys.manager = &hard_manager;
      //sys.allocateHardIntegrator();

      // change ARC parameters
      //sys.driveForMultiClusterImpl(hard_dump.ptcl_bk.getPointer(), hard_dump.n_ptcl, hard_dump.ptcl_arti_bk.getPointer(), hard_dump.n_group, hard_dump.time_end, 0);
      HardIntegrator hard_int;
      hard_int.initial(hard_dump.ptcl_bk.getPointer(), hard_dump.n_ptcl, hard_dump.ptcl_arti_bk.getPointer(), hard_dump.n_group, hard_dump.n_member_in_group.getPointer(), &hard_manager, 0.0);

      auto& interrupt_binary = hard_int.integrateToTime(hard_dump.time_end);
      if (interrupt_binary.status!=AR::InterruptStatus::none) {
          hard_int.printInterruptBinaryInfo(std::cerr);
      }
      else {
          hard_int.driftClusterCMRecordGroupCMDataAndWriteBack(hard_dump.time_end);
      }

  }
  // test stability
  else if (mode==1) {
      typedef H4::ParticleH4<PtclHard> PtclH4;

      SearchGroupCandidate<PtclH4> group_candidate;
      auto* ptcl = hard_dump.ptcl_bk.getPointer();
      PS::S32 n_ptcl = hard_dump.n_ptcl;

      group_candidate.searchAndMerge(ptcl, n_ptcl);

      PS::ReallocatableArray<PtclH4> ptcl_new;
      PS::S32 n_group_in_cluster;

      SystemHard sys;
      sys.manager = &hard_manager;

      PS::ReallocatableArray<COMM::BinaryTree<PtclH4,COMM::Binary>> binary_table;
      PS::ReallocatableArray<SystemHard::GroupIndexInfo> n_member_in_group;
      PS::ReallocatableArray<PS::S32> i_cluster_changeover_update;
      // generate artificial particles, stability test is included
      sys.findGroupsAndCreateArtificialParticlesOneCluster(0, ptcl, n_ptcl, ptcl_new, binary_table, n_group_in_cluster, n_member_in_group, i_cluster_changeover_update, group_candidate, hard_dump.time_end);
  }

#ifdef STELLAR_EVOLUTION
#ifdef BSE
  auto& interaction = hard_manager.ar_manager.interaction;
  if (interaction.fout_sse.is_open()) interaction.fout_sse.close();
  if (interaction.fout_bse.is_open()) interaction.fout_bse.close();
#endif
#endif
  return 0;
}
