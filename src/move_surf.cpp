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

#include "stdio.h"
#include "string.h"
#include "move_surf.h"
#include "surf.h"
#include "grid.h"
#include "comm.h"
#include "update.h"
#include "domain.h"
#include "input.h"
#include "math_extra.h"
#include "math_const.h"
#include "memory.h"
#include "error.h"

using namespace SPARTA_NS;
using namespace MathExtra;
using namespace MathConst;

enum{READFILE,TRANSLATE,ROTATE};
enum{UNKNOWN,OUTSIDE,INSIDE,OVERLAP};           // several files

#define MAXLINE 256

/* ---------------------------------------------------------------------- */

MoveSurf::MoveSurf(SPARTA *sparta) : Pointers(sparta)
{
  me = comm->me;
  nprocs = comm->nprocs;

  // pselect = 1 if point is moved, else 0

  memory->create(pselect,surf->npoint,"move_surf:pselect");

  file = NULL;
  fp = NULL;
}

/* ---------------------------------------------------------------------- */

MoveSurf::~MoveSurf()
{
  memory->destroy(pselect);
  delete [] file;
  if (fp) fclose(fp);
}

/* ---------------------------------------------------------------------- */

void MoveSurf::command(int narg, char **arg)
{
  if (!surf->exist)
    error->all(FLERR,"Cannot move_surf with no surf elements defined");

  if (narg < 2) error->all(FLERR,"Illegal move_surf command");

  // process command-line args

  int igroup = surf->find_group(arg[0]);
  if (igroup < 0) error->all(FLERR,"Move_surf group ID does not exist");
  groupbit = surf->bitmask[igroup];

  process_args(narg-1,&arg[1]);
  mode = 0;

  // perform surface move

  if (me == 0) {
    if (screen) fprintf(screen,"Moving surfs ...\n");
    if (logfile) fprintf(logfile,"Moving surfs ...\n");
  }

  MPI_Barrier(world);
  double time1 = MPI_Wtime();

  dim = domain->dimension;

  // sort particles

  if (particle->exist) particle->sort();

  MPI_Barrier(world);
  double time2 = MPI_Wtime();

  // move points via chosen action by full amount

  move_points(1.0,surf->pts);

  // remake list of surf elements I own
  // assign split cell particles to parent split cell
  // assign surfs to grid cells

  surf->setup_surf();

  grid->unset_neighbors();
  grid->remove_ghosts();

  if (particle->exist && grid->nsplitlocal) {
    Grid::ChildCell *cells = grid->cells;
    int nglocal = grid->nlocal;
    for (int icell = 0; icell < nglocal; icell++)
      if (cells[icell].nsplit > 1)
	grid->combine_split_cell_particles(icell,1);
  }

  grid->clear_surf();
  grid->surf2grid(1);

  // NOTE: is this needed - move method to grid to avoid code duplication
  //if (dim == 2) check_point_near_surf_2d();
  //else check_point_near_surf_3d();

  MPI_Barrier(world);
  double time3 = MPI_Wtime();

  // re-setup owned and ghost cell info

  grid->setup_owned();
  grid->acquire_ghosts();
  grid->reset_neighbors();
  comm->reset_neighbors();

  MPI_Barrier(world);
  double time4 = MPI_Wtime();

  // flag cells and corners as OUTSIDE or INSIDE
  // reallocate per grid cell arrays in per grid computes
  //   local grid cell counts could have changed due to split cell changes

  grid->set_inout();
  grid->type_check();

  // DEBUG
  //grid->debug();

  MPI_Barrier(world);
  double time5 = MPI_Wtime();

  // remove particles as needed due to surface move

  bigint ndeleted;
  if (particle->exist) ndeleted = remove_particles();

  MPI_Barrier(world);
  double time6 = MPI_Wtime();

  double time_total = time6-time1;

  if (comm->me == 0) {
    if (screen) {
      if (particle->exist)
	fprintf(screen,"  " BIGINT_FORMAT " deleted particles\n",ndeleted);
      fprintf(screen,"  CPU time = %g secs\n",time_total);
      fprintf(screen,"  sort/surf2grid/ghost/inout/particle percent = "
	      "%g %g %g %g %g\n",
              100.0*(time2-time1)/time_total,100.0*(time3-time2)/time_total,
              100.0*(time4-time3)/time_total,100.0*(time5-time4)/time_total,
	      100.0*(time6-time5)/time_total);
    }
    if (logfile) {
      if (particle->exist)
	fprintf(logfile,"  " BIGINT_FORMAT " deleted particles\n",ndeleted);
      fprintf(logfile,"  CPU time = %g secs\n",time_total);
      fprintf(logfile,"  sort/surf2grid/ghost/inout/particle percent = "
	      "%g %g %g %g %g\n",
              100.0*(time2-time1)/time_total,100.0*(time3-time2)/time_total,
              100.0*(time4-time3)/time_total,100.0*(time5-time4)/time_total,
	      100.0*(time6-time5)/time_total);
    }
  }
}

/* ----------------------------------------------------------------------
   process command args for both move_surf and fix move/surf
------------------------------------------------------------------------- */

void MoveSurf::process_args(int narg, char **arg)
{
  if (narg < 1) error->all(FLERR,"Illegal move surf command");

  int iarg;
  if (strcmp(arg[0],"file") == 0) {
    if (narg < 3) error->all(FLERR,"Illegal move surf command");
    action = READFILE;
    int n = strlen(arg[1]) + 1;
    file = new char[n];
    strcpy(file,arg[1]);
    n = strlen(arg[2]) + 1;
    entry = new char[n];
    strcpy(entry,arg[2]);
    iarg = 3;
  } else if (strcmp(arg[0],"trans") == 0) {
    if (narg < 4) error->all(FLERR,"Illegal move surf command");
    action = TRANSLATE;
    delta[0] = input->numeric(FLERR,arg[1]);
    delta[1] = input->numeric(FLERR,arg[2]);
    delta[2] = input->numeric(FLERR,arg[3]);
    if (domain->dimension == 2 && delta[2] != 0.0) 
      error->all(FLERR,"Invalid move surf translation for 2d simulation");
    iarg = 4;
  } else if (strcmp(arg[0],"rotate") == 0) {
    if (narg < 8) error->all(FLERR,"Illegal move surf command");
    action = ROTATE;
    theta = input->numeric(FLERR,arg[1]);
    rvec[0] = input->numeric(FLERR,arg[2]);
    rvec[1] = input->numeric(FLERR,arg[3]);
    rvec[2] = input->numeric(FLERR,arg[4]);
    origin[0] = input->numeric(FLERR,arg[5]);
    origin[1] = input->numeric(FLERR,arg[6]);
    origin[2] = input->numeric(FLERR,arg[7]);
    if (domain->dimension == 2 && (rvec[0] != 0.0 || rvec[1] != 0.0)) 
      error->all(FLERR,"Invalid move surf rotation for 2d simulation");
    if (rvec[0] == 0.0 && rvec[1] == 0.0 && rvec[2] == 0.0)
      error->all(FLERR,"Invalid move surf rotation");
    theta *= MY_PI/180.0;
    MathExtra::norm3(rvec);
    iarg = 8;
  } else error->all(FLERR,"Illegal move surf command");

  // no optional args

  if (iarg < narg) error->all(FLERR,"Illegal move surf command");
}

/* ----------------------------------------------------------------------
   move points via specified action
   each method sets pselect = 1 for moved points
   fraction = portion of full distance points should move
------------------------------------------------------------------------- */

void MoveSurf::move_points(double fraction, Surf::Point *origpts)
{
  if (domain->dimension == 2) {
    if (action == READFILE) {
      readfile();
      update_points(fraction);
    } 
    else if (action == TRANSLATE) translate_2d(fraction,origpts);
    else if (action == ROTATE) rotate_2d(fraction,origpts);
    surf->compute_line_normal(0,surf->nline);
  } else {
    if (action == READFILE) {
      readfile();
      update_points(fraction);
    } 
    else if (action == TRANSLATE) translate_3d(fraction,origpts);
    else if (action == ROTATE) rotate_3d(fraction,origpts);
    surf->compute_tri_normal(0,surf->ntri);
  }

  // check that all points are still inside simulation box

  surf->check_point_inside(0,surf->npoint);
}

/* ----------------------------------------------------------------------
   read entry of new point coords from file
------------------------------------------------------------------------- */

void MoveSurf::readfile()
{
  int i;
  char line[MAXLINE];
  char *word,*eof;

  // open point file if necessary
  // if already open, will just continue scanning below
  // NOTE: allow for file name with wildcard char

  if (me == 0 && fp == NULL) {
    fp = fopen(file,"r");
    if (fp == NULL) {
      char str[128];
      sprintf(str,"Cannot open move surf file %s",file);
      error->one(FLERR,str);
    }
  }

  // loop until section found that matches entry

  if (me == 0) {
    while (1) {
      if (fgets(line,MAXLINE,fp) == NULL)
	error->one(FLERR,"Did not find entry in move surf file");
      if (strspn(line," \t\n\r") == strlen(line)) continue;  // blank line
      if (line[0] == '#') continue;                          // comment
      word = strtok(line," \t\n\r");
      if (strcmp(word,entry) != 0) continue;          // non-matching entry
      if (fgets(line,MAXLINE,fp) == NULL)
	error->one(FLERR,"Incompete entry in move surf file");
      word = strtok(line," \t\n\r");        // npoints value after entry
      nread = input->inumeric(FLERR,word);
    }
  }

  // allocate index and coord arrays for nread points

  MPI_Bcast(&nread,1,MPI_INT,0,world);

  if (oldcoord) {
    memory->destroy(readindex);
    memory->destroy(oldcoord);
    memory->destroy(newcoord);
    memory->create(readindex,nread,"move_surf:readindex");
    memory->create(oldcoord,nread,3,"move_surf:oldcoord");
    memory->create(newcoord,nread,3,"move_surf:newcoord");
  }

  // read nread point coords in entry
  // store old current point and new point coords so can move by fraction
  // rindex = ID (index) of this read-in point in master list
  // skip points that are out-of-range

  Surf::Point *pts = surf->pts;
  int npoint = surf->npoint;

  if (me == 0) {
    int id;
    double x,y,z;

    for (int i = 0; i < nread; i++) {
      eof = fgets(line,MAXLINE,fp);
      if (eof == NULL) error->one(FLERR,"Incomplete entry in move surf file");
      id = input->inumeric(FLERR,strtok(line," \t\n\r"));
      x = input->numeric(FLERR,strtok(NULL," \t\n\r"));
      y = input->numeric(FLERR,strtok(NULL," \t\n\r"));
      if (dim == 3) z = input->numeric(FLERR,strtok(NULL," \t\n\r"));
      else z = 0.0;
      if (id < 1 || id > npoint) 
	error->one(FLERR,"Invalid point index in move surf file");
      id--;
      readindex[i] = id;
      oldcoord[i][0] = pts[id].x[0];
      oldcoord[i][1] = pts[id].x[1];
      oldcoord[i][2] = pts[id].x[2];
      newcoord[i][0] = x;
      newcoord[i][1] = y;
      newcoord[i][2] = z;
    }
  }

  // broadcast point info to all procs

  MPI_Bcast(readindex,nread,MPI_INT,0,world);
  MPI_Bcast(&oldcoord[0][0],3*nread,MPI_DOUBLE,0,world);
  MPI_Bcast(&newcoord[0][0],3*nread,MPI_DOUBLE,0,world);

  // pselect[I] = index of Ith surf point in nread points (for now)
  // NOTE: check that same surf point does not appear twice in nread list?

  for (i = 0; i < npoint; i++) pselect[i] = -1;
  for (i = 0; i < nread; i++) pselect[readindex[i]] = i;

  int *rflag;
  memory->create(rflag,nread,"move_surf:rflag");
  for (i =0; i < nread; i++) rflag[i] = 0;

  Surf::Line *lines = surf->lines;
  Surf::Tri *tris = surf->tris;
  int nline = surf->nline;
  int ntri = surf->ntri;
  int p1,p2,p3;

  if (dim == 2) {
    for (i = 0; i < nline; i++) {
      if (!(lines[i].mask & groupbit)) continue;
      p1 = lines[i].p1;
      p2 = lines[i].p2;
      if (pselect[p1] >= 0) rflag[pselect[p1]] = 1;
      if (pselect[p2] >= 0) rflag[pselect[p2]] = 1;
    }
  } else {
    for (i = 0; i < ntri; i++) {
      if (!(tris[i].mask & groupbit)) continue;
      p1 = tris[i].p1;
      p2 = tris[i].p2;
      p3 = tris[i].p3;
      if (pselect[p1] >= 0) rflag[pselect[p1]] = 1;
      if (pselect[p2] >= 0) rflag[pselect[p2]] = 1;
      if (pselect[p3] >= 0) rflag[pselect[p3]] = 1;
    }
  }


  // pselect[I] = 1 if Ith surf point is moved by nread points, else 0

  for (i = 0; i < npoint; i++) pselect[i] = 0;
  for (i = 0; i < nread; i++)
    if (rflag[i]) pselect[readindex[i]] = 1;

  // clean up

  memory->destroy(rflag);
}

/* ----------------------------------------------------------------------
   update points using info from file
------------------------------------------------------------------------- */

void MoveSurf::update_points(double fraction)
{
  int i;

  // update points by fraction of old to new move

  Surf::Point *pts = surf->pts;
  Surf::Point *p;

  for (i = 0; i < nread; i++) {
    if (pselect[readindex[i]] == 0) continue;
    p = &pts[readindex[i]];
    p->x[0] = oldcoord[i][0] + fraction * (newcoord[i][0]-oldcoord[i][0]);
    p->x[1] = oldcoord[i][1] + fraction * (newcoord[i][1]-oldcoord[i][1]);
    p->x[2] = oldcoord[i][2] + fraction * (newcoord[i][2]-oldcoord[i][2]);
  }
}

/* ----------------------------------------------------------------------
   translate surf points in 2d
------------------------------------------------------------------------- */

void MoveSurf::translate_2d(double fraction, Surf::Point *origpts)
{
  int i,p1,p2;

  Surf::Line *lines = surf->lines;
  Surf::Point *pts = surf->pts;
  int nline = surf->nline;
  int npoint = surf->npoint;

  for (i = 0; i < npoint; i++) pselect[i] = 0;

  double dx = fraction * delta[0];
  double dy = fraction * delta[1];

  for (i = 0; i < nline; i++) {
    if (!(lines[i].mask & groupbit)) continue;
    p1 = lines[i].p1;
    p2 = lines[i].p2;
    if (pselect[p1] == 0) {
      pts[p1].x[0] = origpts[p1].x[0] + dx;
      pts[p1].x[1] = origpts[p1].x[1] + dy;
      pselect[p1] = 1;
    }
    if (pselect[p2] == 0) {
      pts[p2].x[0] = origpts[p2].x[0] + dx;
      pts[p2].x[1] = origpts[p2].x[1] + dy;
      pselect[p2] = 1;
    }
  }
}

/* ----------------------------------------------------------------------
   translate surf points in 3d
------------------------------------------------------------------------- */

void MoveSurf::translate_3d(double fraction, Surf::Point *origpts)
{
  int i,p1,p2,p3;

  Surf::Tri *tris = surf->tris;
  Surf::Point *pts = surf->pts;
  int ntri = surf->ntri;
  int npoint = surf->npoint;

  for (i = 0; i < npoint; i++) pselect[i] = 0;

  double dx = fraction * delta[0];
  double dy = fraction * delta[1];
  double dz = fraction * delta[2];

  for (i = 0; i < ntri; i++) {
    if (!(tris[i].mask & groupbit)) continue;
    p1 = tris[i].p1;
    p2 = tris[i].p2;
    p3 = tris[i].p3;
    if (pselect[p1] == 0) {
      pts[p1].x[0] = origpts[p1].x[0] + dx;
      pts[p1].x[1] = origpts[p1].x[1] + dy;
      pts[p1].x[2] = origpts[p1].x[2] + dz;
      pselect[p1] = 1;
    }
    if (pselect[p2] == 0) {
      pts[p2].x[0] = origpts[p2].x[0] + dx;
      pts[p2].x[1] = origpts[p2].x[1] + dy;
      pts[p2].x[2] = origpts[p2].x[2] + dz;
      pselect[p2] = 1;
    }
    if (pselect[p3] == 0) {
      pts[p3].x[0] = origpts[p3].x[0] + dx;
      pts[p3].x[1] = origpts[p3].x[1] + dy;
      pts[p3].x[2] = origpts[p3].x[2] + dz;
      pselect[p3] = 1;
    }
  }
}

/* ----------------------------------------------------------------------
   rotate surf points in 2d
------------------------------------------------------------------------- */

void MoveSurf::rotate_2d(double fraction, Surf::Point *origpts)
{
  int i,p1,p2;
  double q[4],d[3],dnew[3];
  double rotmat[3][3];

  Surf::Line *lines = surf->lines;
  Surf::Point *pts = surf->pts;
  int nline = surf->nline;
  int npoint = surf->npoint;

  for (i = 0; i < npoint; i++) pselect[i] = 0;

  double angle = fraction * theta;
  MathExtra::axisangle_to_quat(rvec,angle,q);
  MathExtra::quat_to_mat(q,rotmat);

  for (i = 0; i < nline; i++) {
    if (!(lines[i].mask & groupbit)) continue;
    p1 = lines[i].p1;
    p2 = lines[i].p2;

    if (pselect[p1] == 0) {
      d[0] = origpts[p1].x[0] - origin[0];
      d[1] = origpts[p1].x[1] - origin[1];
      d[2] = origpts[p1].x[2] - origin[2];
      MathExtra::matvec(rotmat,d,dnew);
      pts[p1].x[0] = dnew[0] + origin[0];
      pts[p1].x[1] = dnew[1] + origin[1];
      pselect[p1] = 1;
    }
    if (pselect[p2] == 0) {
      d[0] = origpts[p2].x[0] - origin[0];
      d[1] = origpts[p2].x[1] - origin[1];
      d[2] = origpts[p2].x[2] - origin[2];
      MathExtra::matvec(rotmat,d,dnew);
      pts[p2].x[0] = dnew[0] + origin[0];
      pts[p2].x[1] = dnew[1] + origin[1];
      pselect[p2] = 1;
    }
  }
}

/* ----------------------------------------------------------------------
   rotate surf points in 2d
------------------------------------------------------------------------- */

void MoveSurf::rotate_3d(double fraction, Surf::Point *origpts)
{
  int i,p1,p2,p3;
  double q[4],d[3],dnew[3];
  double rotmat[3][3];

  Surf::Tri *tris = surf->tris;
  Surf::Point *pts = surf->pts;
  int ntri = surf->ntri;
  int npoint = surf->npoint;

  for (i = 0; i < npoint; i++) pselect[i] = 0;

  double angle = fraction * theta;
  MathExtra::axisangle_to_quat(rvec,angle,q);
  MathExtra::quat_to_mat(q,rotmat);

  for (i = 0; i < ntri; i++) {
    if (!(tris[i].mask & groupbit)) continue;
    p1 = tris[i].p1;
    p2 = tris[i].p2;
    p3 = tris[i].p3;

    if (pselect[p1] == 0) {
      d[0] = origpts[p1].x[0] - origin[0];
      d[1] = origpts[p1].x[1] - origin[1];
      d[2] = origpts[p1].x[2] - origin[2];
      MathExtra::matvec(rotmat,d,dnew);
      pts[p1].x[0] = dnew[0] + origin[0];
      pts[p1].x[1] = dnew[1] + origin[1];
      pts[p1].x[2] = dnew[2] + origin[2];
      pselect[p1] = 1;
    }
    if (pselect[p2] == 0) {
      d[0] = origpts[p2].x[0] - origin[0];
      d[1] = origpts[p2].x[1] - origin[1];
      d[2] = origpts[p2].x[2] - origin[2];
      MathExtra::matvec(rotmat,d,dnew);
      pts[p2].x[0] = dnew[0] + origin[0];
      pts[p2].x[1] = dnew[1] + origin[1];
      pts[p2].x[2] = dnew[2] + origin[2];
      pselect[p2] = 1;
    }
    if (pselect[p3] == 0) {
      d[0] = origpts[p3].x[0] - origin[0];
      d[1] = origpts[p3].x[1] - origin[1];
      d[2] = origpts[p3].x[2] - origin[2];
      MathExtra::matvec(rotmat,d,dnew);
      pts[p3].x[0] = dnew[0] + origin[0];
      pts[p3].x[1] = dnew[1] + origin[1];
      pts[p3].x[2] = dnew[2] + origin[2];
      pselect[p3] = 1;
    }
  }
}

/* ----------------------------------------------------------------------
   remove particles in any cell that is now INSIDE or contains moved surfs
   surfs that moved determined by pselect for any of its points
   reassign particles in split cells to sub cell owner
   compress particles if any flagged for deletion
   NOTE: doc this logic better
------------------------------------------------------------------------- */

bigint MoveSurf::remove_particles()
{
  dim = domain->dimension;
  Surf::Line *lines = surf->lines;
  Surf::Tri *tris = surf->tris;
  Grid::ChildCell *cells = grid->cells;
  Grid::ChildInfo *cinfo = grid->cinfo;
  int nglocal = grid->nlocal;
  int delflag = 0;

  for (int icell = 0; icell < nglocal; icell++) {
    if (cinfo[icell].type == INSIDE) {
      if (cinfo[icell].count) delflag = 1;
      particle->remove_all_from_cell(cinfo[icell].first);
      cinfo[icell].count = 0;
      cinfo[icell].first = -1;
      continue;
    }
    if (cells[icell].nsurf && cells[icell].nsplit >= 1) {
      int nsurf = cells[icell].nsurf;
      int *csurfs = cells[icell].csurfs;
      int m;
      if (dim == 2) {
	for (m = 0; m < nsurf; m++) {
	  if (pselect[lines[csurfs[m]].p1]) break;
	  if (pselect[lines[csurfs[m]].p2]) break;
	}
      } else {
	for (m = 0; m < nsurf; m++) {
	  if (pselect[tris[csurfs[m]].p1]) break;
	  if (pselect[tris[csurfs[m]].p2]) break;
	  if (pselect[tris[csurfs[m]].p3]) break;
	}
      }
      if (m < nsurf) {
	if (cinfo[icell].count) delflag = 1;
	particle->remove_all_from_cell(cinfo[icell].first);
	cinfo[icell].count = 0;
	cinfo[icell].first = -1;
      }
    }
    if (cells[icell].nsplit > 1)
      grid->assign_split_cell_particles(icell);
  }

  int nlocal_old = particle->nlocal;
  if (delflag) particle->compress_rebalance();
  bigint delta = nlocal_old - particle->nlocal;
  bigint ndeleted;
  MPI_Allreduce(&delta,&ndeleted,1,MPI_SPARTA_BIGINT,MPI_SUM,world);
  return ndeleted;
}
