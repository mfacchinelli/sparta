/* ----------------------------------------------------------------------
   SPARTA - Stochastic PArallel Rarefied-gas Time-accurate Analyzer
   http://sparta.sandia.gov
   Steve Plimpton, sjplimp@sandia.gov, Michael Gallis, magalli@sandia.gov
   Sandia National Laboratories

   Copyright (2014) Sandia Corporation.  Under the terms of Contract
   DE-AC04-94AL85000 with Sandia Corporation, the U.S. Government retains
   certain rights in this software.  This software is distributed under 
   the GNU General Public License.

   See the README file in the top-level SPARTA directory.
------------------------------------------------------------------------- */

#ifdef COMMAND_CLASS

CommandStyle(move_surf,MoveSurf)

#else

#ifndef SPARTA_MOVE_SURF_H
#define SPARTA_MOVE_SURF_H

#include "stdio.h"
#include "pointers.h"
#include "surf.h"

namespace SPARTA_NS {

class MoveSurf : protected Pointers {
 public:
  int mode;                 // 0 = move_surf command, 1 = fix move/surf command
  int groupbit;             // FixMoveSurf sets surf group

  MoveSurf(class SPARTA *);
  ~MoveSurf();
  void command(int, char **);
  void process_args(int, char **);
  void move_points(double, Surf::Point *);
  bigint remove_particles();

 private:
  int me,nprocs;
  int dim,action;
  char *file,*entry;
  double theta;
  double delta[3],rvec[3],origin[3];
  FILE *fp;

  int *pselect;                    // 1 if point is moved, else 0
  
  int nread;
  int *readindex;
  double **oldcoord,**newcoord;
  
  void readfile();
  void update_points(double);
  void translate_2d(double, Surf::Point *);
  void translate_3d(double, Surf::Point *);
  void rotate_2d(double, Surf::Point *);
  void rotate_3d(double, Surf::Point *);
};

}

#endif
#endif

/* ERROR/WARNING messages:

E: Illegal ... command

Self-explanatory.  Check the input script syntax and compare to the
documentation for the command.  You can use -echo screen as a
command-line option when running SPARTA to see the offending line.

*/
