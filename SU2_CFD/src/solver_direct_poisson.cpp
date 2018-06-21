/*!
 * \file solution_direct_poisson.cpp
 * \brief Main subrotuines for solving direct problems
 * \author F. Palacios
 * \version 6.0.1 "Falcon"
 *
 * The current SU2 release has been coordinated by the
 * SU2 International Developers Society <www.su2devsociety.org>
 * with selected contributions from the open-source community.
 *
 * The main research teams contributing to the current release are:
 *  - Prof. Juan J. Alonso's group at Stanford University.
 *  - Prof. Piero Colonna's group at Delft University of Technology.
 *  - Prof. Nicolas R. Gauger's group at Kaiserslautern University of Technology.
 *  - Prof. Alberto Guardone's group at Polytechnic University of Milan.
 *  - Prof. Rafael Palacios' group at Imperial College London.
 *  - Prof. Vincent Terrapon's group at the University of Liege.
 *  - Prof. Edwin van der Weide's group at the University of Twente.
 *  - Lab. of New Concepts in Aeronautics at Tech. Institute of Aeronautics.
 *
 * Copyright 2012-2018, Francisco D. Palacios, Thomas D. Economon,
 *                      Tim Albring, and the SU2 contributors.
 *
 * SU2 is free software; you can redistribute it and/or
 * modify it under the terms of the GNU Lesser General Public
 * License as published by the Free Software Foundation; either
 * version 2.1 of the License, or (at your option) any later version.
 *
 * SU2 is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE. See the GNU
 * Lesser General Public License for more details.
 *
 * You should have received a copy of the GNU Lesser General Public
 * License along with SU2. If not, see <http://www.gnu.org/licenses/>.
 */

#include "../include/solver_structure.hpp"

CPoissonSolverFVM::CPoissonSolverFVM(void) : CSolver() { }

CPoissonSolverFVM::CPoissonSolverFVM(CGeometry *geometry, CConfig *config) : CSolver() {
  
  unsigned long  iPoint;
  unsigned short iVar, iDim;
  
  int rank = MASTER_NODE;
#ifdef HAVE_MPI
  MPI_Comm_rank(MPI_COMM_WORLD, &rank);
#endif
  
  nDim =          geometry->GetnDim();
  nPoint =        geometry->GetnPoint();
  nPointDomain =  geometry->GetnPointDomain();
  nVar =          1;
  node =          new CVariable*[nPoint];
  
  /*--- Initialize nVarGrad for deallocation ---*/
  
  nVarGrad = nVar;
  
  Residual = new su2double[nVar]; Residual_RMS = new su2double[nVar];
  Solution = new su2double[nVar];
  Residual_Max = new su2double[nVar];
  

  /*--- Define some structures for locating max residuals ---*/
  
  Point_Max = new unsigned long[nVar];
  for (iVar = 0; iVar < nVar; iVar++) Point_Max[iVar] = 0;
  Point_Max_Coord = new su2double*[nVar];
  for (iVar = 0; iVar < nVar; iVar++) {
    Point_Max_Coord[iVar] = new su2double[nDim];
    for (iDim = 0; iDim < nDim; iDim++) Point_Max_Coord[iVar][iDim] = 0.0;
  }
  
  
  
  /*--- Define some auxiliar vector related with the solution ---*/

  Solution_i = new su2double[nVar]; Solution_j = new su2double[nVar];

  /*--- Define some auxiliary vectors related to the geometry ---*/

  Vector   = new su2double[nDim]; for (iDim = 0; iDim < nDim; iDim++) Vector[iDim]   = 0.0;
  Vector_i = new su2double[nDim]; for (iDim = 0; iDim < nDim; iDim++) Vector_i[iDim] = 0.0;
  Vector_j = new su2double[nDim]; for (iDim = 0; iDim < nDim; iDim++) Vector_j[iDim] = 0.0;

  
 
  Jacobian_i = new su2double* [nVar];
  Jacobian_j = new su2double* [nVar];
  for (iVar = 0; iVar < nVar; iVar++) {
    Jacobian_i[iVar] = new su2double [nVar];
    Jacobian_j[iVar] = new su2double [nVar];
  }
  
 
  
  /*--- Initialization of the structure of the whole Jacobian ---*/
  if (rank == MASTER_NODE) cout << "Initialize Jacobian structure (Poisson equation)." << endl;
  Jacobian.Initialize(nPoint, nPointDomain, nVar, nVar, true, geometry, config);
  
  /*--- Solution and residual vectors ---*/
  
  LinSysSol.Initialize(nPoint, nPointDomain, nVar, 0.0);
  LinSysRes.Initialize(nPoint, nPointDomain, nVar, 0.0);
  LinSysAux.Initialize(nPoint, nPointDomain, nVar, 0.0);

  /*--- Computation of gradients by least squares ---*/
  
  Smatrix = new su2double* [nDim]; // S matrix := inv(R)*traspose(inv(R))
  for (iDim = 0; iDim < nDim; iDim++)
    Smatrix[iDim] = new su2double [nDim];
  
  Cvector = new su2double* [nVar]; // c vector := transpose(WA)*(Wb)
  for (iVar = 0; iVar < nVar; iVar++)
    Cvector[iVar] = new su2double [nDim];
  
  /*--- Always instantiate and initialize the variable to a zero value. ---*/
  
  for (iPoint = 0; iPoint < nPoint; iPoint++)
    if (config->GetKind_Incomp_System()==PRESSURE_BASED) node[iPoint] = new CPotentialVariable(0.0, nDim, nVar, config);
    else node[iPoint] = new CPotentialVariable(0.0, nDim, nVar, config);
}

CPoissonSolverFVM::~CPoissonSolverFVM(void) {
  
  unsigned int iVar;
  iVar = 1;
}


void CPoissonSolverFVM::Source_Template(CGeometry *geometry, CSolver **solver_container, CNumerics *numerics,
                                     CConfig *config, unsigned short iMesh) {
}


void CPoissonSolverFVM::Preprocessing(CGeometry *geometry, CSolver **solver_container,
                                   CConfig *config, unsigned short iMesh, unsigned short iRKStep, unsigned short RunTime_EqSystem, bool Output) {
  unsigned long iPoint;
  
  for (iPoint = 0; iPoint < nPoint; iPoint ++) {

    /*--- Initialize the residual vector ---*/

    LinSysRes.SetBlock_Zero(iPoint);

  }

  /*--- Initialize the Jacobian matrices ---*/

  Jacobian.SetValZero();

  if (config->GetKind_Gradient_Method() == GREEN_GAUSS) SetSolution_Gradient_GG(geometry, config);

  if (config->GetKind_Gradient_Method() == WEIGHTED_LEAST_SQUARES) SetSolution_Gradient_LS(geometry, config);
  
}




void CPoissonSolverFVM:: SetUndivided_Laplacian(CGeometry *geometry, CConfig *config) {


  unsigned long iPoint, jPoint, iEdge;
  su2double *Diff;
  unsigned short iVar;
  bool boundary_i, boundary_j;

  Diff = new su2double[nVar];

  for (iPoint = 0; iPoint < nPointDomain; iPoint++)
    node[iPoint]->SetUnd_LaplZero();

  for (iEdge = 0; iEdge < geometry->GetnEdge(); iEdge++) {

    iPoint = geometry->edge[iEdge]->GetNode(0);
    jPoint = geometry->edge[iEdge]->GetNode(1);

    /*--- Solution differences ---*/

    for (iVar = 0; iVar < nVar; iVar++)
      Diff[iVar] = node[iPoint]->GetSolution(iVar) - node[jPoint]->GetSolution(iVar);

    boundary_i = geometry->node[iPoint]->GetPhysicalBoundary();
    boundary_j = geometry->node[jPoint]->GetPhysicalBoundary();

    /*--- Both points inside the domain, or both in the boundary ---*/

    if ((!boundary_i && !boundary_j) || (boundary_i && boundary_j)) {
      if (geometry->node[iPoint]->GetDomain()) node[iPoint]->SubtractUnd_Lapl(Diff);
      if (geometry->node[jPoint]->GetDomain()) node[jPoint]->AddUnd_Lapl(Diff);
    }

    /*--- iPoint inside the domain, jPoint on the boundary ---*/

    if (!boundary_i && boundary_j)
      if (geometry->node[iPoint]->GetDomain()) node[iPoint]->SubtractUnd_Lapl(Diff);

    /*--- jPoint inside the domain, iPoint on the boundary ---*/

    if (boundary_i && !boundary_j)
      if (geometry->node[jPoint]->GetDomain()) node[jPoint]->AddUnd_Lapl(Diff);

  }

  /*--- MPI parallelization ---*/

  Set_MPI_Undivided_Laplacian(geometry, config);

delete [] Diff;

}


void CPoissonSolverFVM:: Set_MPI_Undivided_Laplacian(CGeometry *geometry, CConfig *config) {


  unsigned short iVar, iMarker, iPeriodic_Index, MarkerS, MarkerR;
  unsigned long iVertex, iPoint, nVertexS, nVertexR, nBufferS_Vector, nBufferR_Vector;
  su2double rotMatrix[3][3], *angles, theta, cosTheta, sinTheta, phi, cosPhi, sinPhi, psi, cosPsi, sinPsi,
  *Buffer_Receive_Undivided_Laplacian = NULL, *Buffer_Send_Undivided_Laplacian = NULL;

#ifdef HAVE_MPI
  int send_to, receive_from;
  MPI_Status status;
#endif

  for (iMarker = 0; iMarker < config->GetnMarker_All(); iMarker++) {

    if ((config->GetMarker_All_KindBC(iMarker) == SEND_RECEIVE) &&
        (config->GetMarker_All_SendRecv(iMarker) > 0)) {

      MarkerS = iMarker;  MarkerR = iMarker+1;

#ifdef HAVE_MPI
      send_to = config->GetMarker_All_SendRecv(MarkerS)-1;
      receive_from = abs(config->GetMarker_All_SendRecv(MarkerR))-1;
#endif

      nVertexS = geometry->nVertex[MarkerS];  nVertexR = geometry->nVertex[MarkerR];
      nBufferS_Vector = nVertexS*nVar;        nBufferR_Vector = nVertexR*nVar;

      /*--- Allocate Receive and send buffers  ---*/
      Buffer_Receive_Undivided_Laplacian = new su2double [nBufferR_Vector];
      Buffer_Send_Undivided_Laplacian = new su2double[nBufferS_Vector];

      /*--- Copy the solution old that should be sended ---*/
      for (iVertex = 0; iVertex < nVertexS; iVertex++) {
        iPoint = geometry->vertex[MarkerS][iVertex]->GetNode();
        for (iVar = 0; iVar < nVar; iVar++)
          Buffer_Send_Undivided_Laplacian[iVar*nVertexS+iVertex] = node[iPoint]->GetUndivided_Laplacian(iVar);
      }

#ifdef HAVE_MPI

      /*--- Send/Receive information using Sendrecv ---*/
      SU2_MPI::Sendrecv(Buffer_Send_Undivided_Laplacian, nBufferS_Vector, MPI_DOUBLE, send_to, 0,
                        Buffer_Receive_Undivided_Laplacian, nBufferR_Vector, MPI_DOUBLE, receive_from, 0, MPI_COMM_WORLD, &status);

#else

      /*--- Receive information without MPI ---*/
      for (iVertex = 0; iVertex < nVertexR; iVertex++) {
        for (iVar = 0; iVar < nVar; iVar++)
          Buffer_Receive_Undivided_Laplacian[iVar*nVertexR+iVertex] = Buffer_Send_Undivided_Laplacian[iVar*nVertexR+iVertex];
      }

#endif

      /*--- Deallocate send buffer ---*/
      delete [] Buffer_Send_Undivided_Laplacian;

      /*--- Do the coordinate transformation ---*/
      for (iVertex = 0; iVertex < nVertexR; iVertex++) {

        /*--- Find point and its type of transformation ---*/
        iPoint = geometry->vertex[MarkerR][iVertex]->GetNode();

        /*--- Only copy conserved variables - no transformation necessary. ---*/
        for (iVar = 0; iVar < nVar; iVar++)
          Solution[iVar] = Buffer_Receive_Undivided_Laplacian[iVar*nVertexR+iVertex];

        /*--- Copy transformed conserved variables back into buffer. ---*/
        for (iVar = 0; iVar < nVar; iVar++)
          node[iPoint]->SetUndivided_Laplacian(iVar, Solution[iVar]);

      }

      /*--- Deallocate receive buffer ---*/
      delete [] Buffer_Receive_Undivided_Laplacian;

    }

  }


}


void CPoissonSolverFVM:: Set_MPI_Solution(CGeometry *geometry, CConfig *config) {

  unsigned short iVar, iMarker, iPeriodic_Index, MarkerS, MarkerR;
  unsigned long iVertex, iPoint, nVertexS, nVertexR, nBufferS_Vector, nBufferR_Vector;
  su2double rotMatrix[3][3], *angles, theta, cosTheta, sinTheta, phi, cosPhi, sinPhi, psi, cosPsi, sinPsi, *Buffer_Receive_U = NULL, *Buffer_Send_U = NULL;
#ifdef HAVE_MPI
  int send_to, receive_from;
  MPI_Status status;
#endif

  for (iMarker = 0; iMarker < config->GetnMarker_All(); iMarker++) {

    if ((config->GetMarker_All_KindBC(iMarker) == SEND_RECEIVE) &&
        (config->GetMarker_All_SendRecv(iMarker) > 0)) {

      MarkerS = iMarker;  MarkerR = iMarker+1;

#ifdef HAVE_MPI
      send_to = config->GetMarker_All_SendRecv(MarkerS)-1;
      receive_from = abs(config->GetMarker_All_SendRecv(MarkerR))-1;
#endif

      nVertexS = geometry->nVertex[MarkerS];  nVertexR = geometry->nVertex[MarkerR];
      nBufferS_Vector = nVertexS*nVar;        nBufferR_Vector = nVertexR*nVar;

      /*--- Allocate Receive and send buffers  ---*/
      Buffer_Receive_U = new su2double [nBufferR_Vector];
      Buffer_Send_U = new su2double[nBufferS_Vector];

      /*--- Copy the solution that should be sended ---*/
      for (iVertex = 0; iVertex < nVertexS; iVertex++) {
        iPoint = geometry->vertex[MarkerS][iVertex]->GetNode();
        for (iVar = 0; iVar < nVar; iVar++) {
          Buffer_Send_U[iVar*nVertexS+iVertex] = node[iPoint]->GetSolution(iVar);
        }
      }

#ifdef HAVE_MPI

      /*--- Send/Receive information using Sendrecv ---*/
      SU2_MPI::Sendrecv(Buffer_Send_U, nBufferS_Vector, MPI_DOUBLE, send_to, 0,
                        Buffer_Receive_U, nBufferR_Vector, MPI_DOUBLE, receive_from, 0, MPI_COMM_WORLD, &status);

#else

      /*--- Receive information without MPI ---*/
      for (iVertex = 0; iVertex < nVertexR; iVertex++) {
        for (iVar = 0; iVar < nVar; iVar++)
          Buffer_Receive_U[iVar*nVertexR+iVertex] = Buffer_Send_U[iVar*nVertexR+iVertex];
      }

#endif

      /*--- Deallocate send buffer ---*/
      delete [] Buffer_Send_U;

      /*--- Do the coordinate transformation ---*/
      for (iVertex = 0; iVertex < nVertexR; iVertex++) {
        iPoint = geometry->vertex[MarkerR][iVertex]->GetNode();
        for (iVar = 0; iVar < nVar; iVar++)
          node[iPoint]->SetSolution(iVar, Buffer_Receive_U[iVar*nVertexR+iVertex]);

      }

      /*--- Deallocate receive buffer ---*/
      delete [] Buffer_Receive_U;

    }

}

}

void CPoissonSolverFVM:: Set_MPI_Solution_Old(CGeometry *geometry, CConfig *config) {


  unsigned short iVar, iMarker, iPeriodic_Index, MarkerS, MarkerR;
  unsigned long iVertex, iPoint, nVertexS, nVertexR, nBufferS_Vector, nBufferR_Vector;
  su2double rotMatrix[3][3], *angles, theta, cosTheta, sinTheta, phi, cosPhi, sinPhi, psi, cosPsi, sinPsi,
  *Buffer_Receive_U = NULL, *Buffer_Send_U = NULL;

#ifdef HAVE_MPI
  int send_to, receive_from;
  MPI_Status status;
#endif

  for (iMarker = 0; iMarker < config->GetnMarker_All(); iMarker++) {

    if ((config->GetMarker_All_KindBC(iMarker) == SEND_RECEIVE) &&
        (config->GetMarker_All_SendRecv(iMarker) > 0)) {

      MarkerS = iMarker;  MarkerR = iMarker+1;

#ifdef HAVE_MPI
      send_to = config->GetMarker_All_SendRecv(MarkerS)-1;
      receive_from = abs(config->GetMarker_All_SendRecv(MarkerR))-1;
#endif

      nVertexS = geometry->nVertex[MarkerS];  nVertexR = geometry->nVertex[MarkerR];
      nBufferS_Vector = nVertexS*nVar;        nBufferR_Vector = nVertexR*nVar;

      /*--- Allocate Receive and send buffers  ---*/
      Buffer_Receive_U = new su2double [nBufferR_Vector];
      Buffer_Send_U = new su2double[nBufferS_Vector];

      /*--- Copy the solution old that should be sended ---*/
      for (iVertex = 0; iVertex < nVertexS; iVertex++) {
        iPoint = geometry->vertex[MarkerS][iVertex]->GetNode();
        for (iVar = 0; iVar < nVar; iVar++)
          Buffer_Send_U[iVar*nVertexS+iVertex] = node[iPoint]->GetSolution_Old(iVar);
      }

#ifdef HAVE_MPI

      /*--- Send/Receive information using Sendrecv ---*/
      SU2_MPI::Sendrecv(Buffer_Send_U, nBufferS_Vector, MPI_DOUBLE, send_to, 0,
                        Buffer_Receive_U, nBufferR_Vector, MPI_DOUBLE, receive_from, 0, MPI_COMM_WORLD, &status);

#else

      /*--- Receive information without MPI ---*/
      for (iVertex = 0; iVertex < nVertexR; iVertex++) {
        for (iVar = 0; iVar < nVar; iVar++)
          Buffer_Receive_U[iVar*nVertexR+iVertex] = Buffer_Send_U[iVar*nVertexR+iVertex];
      }

#endif

      /*--- Deallocate send buffer ---*/
      delete [] Buffer_Send_U;

      /*--- Do the coordinate transformation ---*/
      for (iVertex = 0; iVertex < nVertexR; iVertex++) {
        iPoint = geometry->vertex[MarkerR][iVertex]->GetNode();
        for (iVar = 0; iVar < nVar; iVar++)
          node[iPoint]->SetSolution_Old(iVar, Buffer_Receive_U[iVar*nVertexR+iVertex]);
      }

      /*--- Deallocate receive buffer ---*/
      delete [] Buffer_Receive_U;

    }

  }


}

void CPoissonSolverFVM:: Set_MPI_Solution_Gradient(CGeometry *geometry, CConfig *config){


  unsigned short iVar, iDim, iMarker, iPeriodic_Index, MarkerS, MarkerR;
  unsigned long iVertex, iPoint, nVertexS, nVertexR, nBufferS_Vector, nBufferR_Vector;
  su2double rotMatrix[3][3], *angles, theta, cosTheta, sinTheta, phi, cosPhi, sinPhi, psi, cosPsi, sinPsi,
  *Buffer_Receive_Gradient = NULL, *Buffer_Send_Gradient = NULL;

  su2double **Gradient = new su2double* [nVar];
  for (iVar = 0; iVar < nVar; iVar++)
    Gradient[iVar] = new su2double[nDim];

#ifdef HAVE_MPI
  int send_to, receive_from;
  MPI_Status status;
#endif

  for (iMarker = 0; iMarker < config->GetnMarker_All(); iMarker++) {

    if ((config->GetMarker_All_KindBC(iMarker) == SEND_RECEIVE) &&
        (config->GetMarker_All_SendRecv(iMarker) > 0)) {

      MarkerS = iMarker;  MarkerR = iMarker+1;

#ifdef HAVE_MPI
      send_to = config->GetMarker_All_SendRecv(MarkerS)-1;
      receive_from = abs(config->GetMarker_All_SendRecv(MarkerR))-1;
#endif

      nVertexS = geometry->nVertex[MarkerS];  nVertexR = geometry->nVertex[MarkerR];
      nBufferS_Vector = nVertexS*nVar*nDim;        nBufferR_Vector = nVertexR*nVar*nDim;

      /*--- Allocate Receive and send buffers  ---*/
      Buffer_Receive_Gradient = new su2double [nBufferR_Vector];
      Buffer_Send_Gradient = new su2double[nBufferS_Vector];

      /*--- Copy the solution old that should be sended ---*/
      for (iVertex = 0; iVertex < nVertexS; iVertex++) {
        iPoint = geometry->vertex[MarkerS][iVertex]->GetNode();
        for (iVar = 0; iVar < nVar; iVar++)
          for (iDim = 0; iDim < nDim; iDim++)
            Buffer_Send_Gradient[iDim*nVar*nVertexS+iVar*nVertexS+iVertex] = node[iPoint]->GetGradient(iVar, iDim);
      }

#ifdef HAVE_MPI

      /*--- Send/Receive information using Sendrecv ---*/
      SU2_MPI::Sendrecv(Buffer_Send_Gradient, nBufferS_Vector, MPI_DOUBLE, send_to, 0,
                        Buffer_Receive_Gradient, nBufferR_Vector, MPI_DOUBLE, receive_from, 0, MPI_COMM_WORLD, &status);

#else

      /*--- Receive information without MPI ---*/
      for (iVertex = 0; iVertex < nVertexR; iVertex++) {
        for (iVar = 0; iVar < nVar; iVar++)
          for (iDim = 0; iDim < nDim; iDim++)
            Buffer_Receive_Gradient[iDim*nVar*nVertexR+iVar*nVertexR+iVertex] = Buffer_Send_Gradient[iDim*nVar*nVertexR+iVar*nVertexR+iVertex];
      }

#endif

      /*--- Deallocate send buffer ---*/
      delete [] Buffer_Send_Gradient;

      /*--- Do the coordinate transformation ---*/
      for (iVertex = 0; iVertex < nVertexR; iVertex++) {

        /*--- Find point and its type of transformation ---*/
        iPoint = geometry->vertex[MarkerR][iVertex]->GetNode();
        iPeriodic_Index = geometry->vertex[MarkerR][iVertex]->GetRotation_Type();

        /*--- Retrieve the supplied periodic information. ---*/
        angles = config->GetPeriodicRotation(iPeriodic_Index);

        /*--- Store angles separately for clarity. ---*/
        theta    = angles[0];   phi    = angles[1];     psi    = angles[2];
        cosTheta = cos(theta);  cosPhi = cos(phi);      cosPsi = cos(psi);
        sinTheta = sin(theta);  sinPhi = sin(phi);      sinPsi = sin(psi);

        /*--- Compute the rotation matrix. Note that the implicit
         ordering is rotation about the x-axis, y-axis,
         then z-axis. Note that this is the transpose of the matrix
         used during the preprocessing stage. ---*/
        rotMatrix[0][0] = cosPhi*cosPsi;    rotMatrix[1][0] = sinTheta*sinPhi*cosPsi - cosTheta*sinPsi;     rotMatrix[2][0] = cosTheta*sinPhi*cosPsi + sinTheta*sinPsi;
        rotMatrix[0][1] = cosPhi*sinPsi;    rotMatrix[1][1] = sinTheta*sinPhi*sinPsi + cosTheta*cosPsi;     rotMatrix[2][1] = cosTheta*sinPhi*sinPsi - sinTheta*cosPsi;
        rotMatrix[0][2] = -sinPhi;          rotMatrix[1][2] = sinTheta*cosPhi;                              rotMatrix[2][2] = cosTheta*cosPhi;

        /*--- Copy conserved variables before performing transformation. ---*/
        for (iVar = 0; iVar < nVar; iVar++)
          for (iDim = 0; iDim < nDim; iDim++)
            Gradient[iVar][iDim] = Buffer_Receive_Gradient[iDim*nVar*nVertexR+iVar*nVertexR+iVertex];

        /*--- Need to rotate the gradients for all conserved variables. ---*/
        for (iVar = 0; iVar < nVar; iVar++) {
          if (nDim == 2) {
            Gradient[iVar][0] = rotMatrix[0][0]*Buffer_Receive_Gradient[0*nVar*nVertexR+iVar*nVertexR+iVertex] + rotMatrix[0][1]*Buffer_Receive_Gradient[1*nVar*nVertexR+iVar*nVertexR+iVertex];
            Gradient[iVar][1] = rotMatrix[1][0]*Buffer_Receive_Gradient[0*nVar*nVertexR+iVar*nVertexR+iVertex] + rotMatrix[1][1]*Buffer_Receive_Gradient[1*nVar*nVertexR+iVar*nVertexR+iVertex];
          }
          else {
            Gradient[iVar][0] = rotMatrix[0][0]*Buffer_Receive_Gradient[0*nVar*nVertexR+iVar*nVertexR+iVertex] + rotMatrix[0][1]*Buffer_Receive_Gradient[1*nVar*nVertexR+iVar*nVertexR+iVertex] + rotMatrix[0][2]*Buffer_Receive_Gradient[2*nVar*nVertexR+iVar*nVertexR+iVertex];
            Gradient[iVar][1] = rotMatrix[1][0]*Buffer_Receive_Gradient[0*nVar*nVertexR+iVar*nVertexR+iVertex] + rotMatrix[1][1]*Buffer_Receive_Gradient[1*nVar*nVertexR+iVar*nVertexR+iVertex] + rotMatrix[1][2]*Buffer_Receive_Gradient[2*nVar*nVertexR+iVar*nVertexR+iVertex];
            Gradient[iVar][2] = rotMatrix[2][0]*Buffer_Receive_Gradient[0*nVar*nVertexR+iVar*nVertexR+iVertex] + rotMatrix[2][1]*Buffer_Receive_Gradient[1*nVar*nVertexR+iVar*nVertexR+iVertex] + rotMatrix[2][2]*Buffer_Receive_Gradient[2*nVar*nVertexR+iVar*nVertexR+iVertex];
          }
        }

        /*--- Store the received information ---*/
        for (iVar = 0; iVar < nVar; iVar++)
          for (iDim = 0; iDim < nDim; iDim++)
            node[iPoint]->SetGradient(iVar, iDim, Gradient[iVar][iDim]);

      }

      /*--- Deallocate receive buffer ---*/
      delete [] Buffer_Receive_Gradient;

    }

  }

  for (iVar = 0; iVar < nVar; iVar++)
    delete [] Gradient[iVar];
  
  delete [] Gradient;




}

void CPoissonSolverFVM:: LoadRestart(CGeometry **geometry, CSolver ***solver, CConfig *config, int val_iter, bool val_update_geo){

/*--- Restart the solution from file information ---*/
  unsigned short iDim, iVar, iMesh, iMeshFine;
  unsigned long iPoint, index, iChildren, Point_Fine;
  
  su2double Area_Children, Area_Parent, *Coord, *Solution_Fine;
  bool grid_movement  = config->GetGrid_Movement();
  bool dual_time = ((config->GetUnsteady_Simulation() == DT_STEPPING_1ST) ||
                    (config->GetUnsteady_Simulation() == DT_STEPPING_2ND));
  bool steady_restart = config->GetSteadyRestart();
  bool time_stepping = config->GetUnsteady_Simulation() == TIME_STEPPING;

  string UnstExt, text_line;
  ifstream restart_file;

  unsigned short iZone = config->GetiZone();
  unsigned short nZone = config->GetnZone();

  string restart_filename = config->GetSolution_FlowFileName();

  Coord = new su2double [nDim];
  for (iDim = 0; iDim < nDim; iDim++)
    Coord[iDim] = 0.0;

  int rank = MASTER_NODE;
#ifdef HAVE_MPI
  MPI_Comm_rank(MPI_COMM_WORLD, &rank);
#endif

  int counter = 0;
  long iPoint_Local = 0; unsigned long iPoint_Global = 0;
  unsigned long iPoint_Global_Local = 0;
  unsigned short rbuf_NotMatching = 0, sbuf_NotMatching = 0;

  /*--- Skip coordinates ---*/

  unsigned short skipVars = 0;

  skipVars = geometry[MESH_0]->GetnDim();;

  /*--- Multizone problems require the number of the zone to be appended. ---*/

  if (nZone > 1)
    restart_filename = config->GetMultizone_FileName(restart_filename, iZone);

  /*--- Modify file name for an unsteady restart ---*/

  if (dual_time || time_stepping)
    restart_filename = config->GetUnsteady_FileName(restart_filename, val_iter);

  /*--- Read the restart data from either an ASCII or binary SU2 file. ---*/

  if (config->GetRead_Binary_Restart()) {
    Read_SU2_Restart_Binary(geometry[MESH_0], config, restart_filename);
  } else {
    Read_SU2_Restart_ASCII(geometry[MESH_0], config, restart_filename);
  }

  /*--- Load data from the restart into correct containers. ---*/

  counter = 0;
  for (iPoint_Global = 0; iPoint_Global < geometry[MESH_0]->GetGlobal_nPointDomain(); iPoint_Global++ ) {

    /*--- Retrieve local index. If this node from the restart file lives
     on the current processor, we will load and instantiate the vars. ---*/

    iPoint_Local = geometry[MESH_0]->GetGlobal_to_Local_Point(iPoint_Global);

    if (iPoint_Local > -1) {

      /*--- We need to store this point's data, so jump to the correct
       offset in the buffer of data from the restart file and load it. ---*/

      index = counter*Restart_Vars[1] + skipVars;
      for (iVar = 0; iVar < nVar; iVar++) Solution[iVar] = Restart_Data[index+iVar];
      node[iPoint_Local]->SetSolution(Solution);
      iPoint_Global_Local++;

      /*--- Increment the overall counter for how many points have been loaded. ---*/
      counter++;
    }

  }

  /*--- Detect a wrong solution file ---*/

  if (iPoint_Global_Local < nPointDomain) { sbuf_NotMatching = 1; }

#ifndef HAVE_MPI
  rbuf_NotMatching = sbuf_NotMatching;
#else
  SU2_MPI::Allreduce(&sbuf_NotMatching, &rbuf_NotMatching, 1, MPI_UNSIGNED_SHORT, MPI_SUM, MPI_COMM_WORLD);
#endif
  if (rbuf_NotMatching != 0) {
    if (rank == MASTER_NODE) {
      cout << endl << "The solution file " << restart_filename.data() << " doesn't match with the mesh file!" << endl;
      cout << "It could be empty lines at the end of the file." << endl << endl;
    }
#ifndef HAVE_MPI
    exit(EXIT_FAILURE);
#else
    MPI_Barrier(MPI_COMM_WORLD);
    MPI_Abort(MPI_COMM_WORLD,1);
    MPI_Finalize();
#endif
  }

  /*--- Communicate the loaded solution on the fine grid before we transfer
   it down to the coarse levels. We alo call the preprocessing routine
   on the fine level in order to have all necessary quantities updated,
   especially if this is a turbulent simulation (eddy viscosity). ---*/

  solver[MESH_0][POISSON_SOL]->Set_MPI_Solution(geometry[MESH_0], config);
  solver[MESH_0][POISSON_SOL]->Preprocessing(geometry[MESH_0], solver[MESH_0], config, MESH_0, NO_RK_ITER, RUNTIME_POISSON_SYS, false);

  /*--- Interpolate the solution down to the coarse multigrid levels ---*/

  for (iMesh = 1; iMesh <= config->GetnMGLevels(); iMesh++) {
    for (iPoint = 0; iPoint < geometry[iMesh]->GetnPoint(); iPoint++) {
      Area_Parent = geometry[iMesh]->node[iPoint]->GetVolume();
      for (iVar = 0; iVar < nVar; iVar++) Solution[iVar] = 0.0;
      for (iChildren = 0; iChildren < geometry[iMesh]->node[iPoint]->GetnChildren_CV(); iChildren++) {
        Point_Fine = geometry[iMesh]->node[iPoint]->GetChildren_CV(iChildren);
        Area_Children = geometry[iMesh-1]->node[Point_Fine]->GetVolume();
        Solution_Fine = solver[iMesh-1][HEAT_SOL]->node[Point_Fine]->GetSolution();
        for (iVar = 0; iVar < nVar; iVar++) {
          Solution[iVar] += Solution_Fine[iVar]*Area_Children/Area_Parent;
        }
      }
      solver[iMesh][POISSON_SOL]->node[iPoint]->SetSolution(Solution);
    }
    solver[iMesh][POISSON_SOL]->Set_MPI_Solution(geometry[iMesh], config);
    solver[iMesh][POISSON_SOL]->Preprocessing(geometry[iMesh], solver[iMesh], config, iMesh, NO_RK_ITER, RUNTIME_POISSON_SYS, false);
  }

  delete [] Coord;

  /*--- Delete the class memory that is used to load the restart. ---*/

  if (Restart_Vars != NULL) delete [] Restart_Vars;
  if (Restart_Data != NULL) delete [] Restart_Data;
  Restart_Vars = NULL; Restart_Data = NULL;




}

void CPoissonSolverFVM::Viscous_Residual(CGeometry *geometry, CSolver **solver_container, CNumerics *numerics,
                                     CConfig *config, unsigned short iMesh, unsigned short iRKStep) {
										 
su2double Poisson_Coeff_i,Poisson_Coeff_j,**Sol_i_Grad,**Sol_j_Grad,Poissonval_i,Poissonval_j;
su2double Mom_Coeff;
unsigned long iEdge, iPoint, jPoint;

	for (iEdge = 0; iEdge < geometry->GetnEdge(); iEdge++) {

		iPoint = geometry->edge[iEdge]->GetNode(0);
		jPoint = geometry->edge[iEdge]->GetNode(1);
    
		/*--- Points coordinates, and normal vector ---*/
		numerics->SetCoord(geometry->node[iPoint]->GetCoord(),
						geometry->node[jPoint]->GetCoord());
		numerics->SetNormal(geometry->edge[iEdge]->GetNormal());

		if (config->GetKind_Incomp_System()!=PRESSURE_BASED) {
			Poisson_Coeff_i = 1.0;//config->GetPoisson_Coeff();
			Poisson_Coeff_j = 1.0;//config->GetPoisson_Coeff();
    
			Sol_i_Grad = node[iPoint]->GetGradient();
			Sol_j_Grad = node[jPoint]->GetGradient();
    
			numerics->SetConsVarGradient(Sol_i_Grad, Sol_j_Grad);
   
			/*--- Primitive variables w/o reconstruction ---*/
			Poissonval_i = node[iPoint]->GetSolution(0);
			Poissonval_j = node[jPoint]->GetSolution(0);
    
			numerics->SetPoissonval(Poissonval_i,Poissonval_j);
			
		}
		else {
			Sol_i_Grad = node[iPoint]->GetGradient();
			Sol_j_Grad = node[jPoint]->GetGradient();
    
			numerics->SetConsVarGradient(Sol_i_Grad, Sol_j_Grad);
   
			/*--- Primitive variables w/o reconstruction ---*/
			Poissonval_i = node[iPoint]->GetSolution(0);
			Poissonval_j = node[jPoint]->GetSolution(0);
    
			numerics->SetPoissonval(Poissonval_i,Poissonval_j);
			
			Mom_Coeff = solver_container[FLOW_SOL]->node[iPoint]->GetMean_Mom_Coeff();
			
			Poisson_Coeff_i = solver_container[FLOW_SOL]->GetDensity_Inf()*geometry->node[iPoint]->GetVolume()/Mom_Coeff;
			
			Mom_Coeff = solver_container[FLOW_SOL]->node[jPoint]->GetMean_Mom_Coeff();
			
			Poisson_Coeff_j = solver_container[FLOW_SOL]->GetDensity_Inf()*geometry->node[jPoint]->GetVolume()/Mom_Coeff;
			
			node[iPoint]->SetPoisson_Coeff(solver_container[FLOW_SOL]->node[iPoint]->GetMean_Mom_Coeff());
			
			node[jPoint]->SetPoisson_Coeff(solver_container[FLOW_SOL]->node[jPoint]->GetMean_Mom_Coeff());
			
			numerics->SetPoisson_Coeff(Poisson_Coeff_i,Poisson_Coeff_j);
			
		}

		/*--- Compute residual, and Jacobians ---*/
		numerics->ComputeResidual(Residual, Jacobian_i, Jacobian_j, config);

		/*--- Add and subtract residual, and update Jacobians ---*/
		LinSysRes.SubtractBlock(iPoint, Residual);
		LinSysRes.AddBlock(jPoint, Residual);


		Jacobian.SubtractBlock(iPoint, iPoint, Jacobian_i);
		Jacobian.SubtractBlock(iPoint, jPoint, Jacobian_j);
		Jacobian.AddBlock(jPoint, iPoint, Jacobian_i);
		Jacobian.AddBlock(jPoint, jPoint, Jacobian_j);
	}
	
}

void CPoissonSolverFVM::Postprocessing(CGeometry *geometry, CSolver **solver_container, CConfig *config,
                        unsigned short iMesh){
							
  
  
  if (config->GetKind_Gradient_Method() == GREEN_GAUSS) SetSolution_Gradient_GG(geometry, config);

  if (config->GetKind_Gradient_Method() == WEIGHTED_LEAST_SQUARES) SetSolution_Gradient_LS(geometry, config);							

							
}

void CPoissonSolverFVM::Source_Residual(CGeometry *geometry, CSolver **solver_container, CNumerics *numerics, CNumerics *second_numerics, CConfig *config, unsigned short iMesh) {

  unsigned short iVar;
  unsigned long iPoint;
  su2double Src_Term;

  /*--- Initialize the source residual to zero ---*/
  for (iVar = 0; iVar < nVar; iVar++) {
	  Residual[iVar] = 0.0;
  }
  
  for (iPoint = 0; iPoint < nPointDomain; iPoint++) {

    /*--- Load the volume of the dual mesh cell ---*/

    numerics->SetVolume(geometry->node[iPoint]->GetVolume());

    /*--- Compute the source term ---*/
    
    Src_Term = 0.0;
        
    if (config->GetKind_Incomp_System() == PRESSURE_BASED) {
		
		Src_Term = solver_container[FLOW_SOL]->node[iPoint]->GetMassFlux();

    }

    numerics->SetSourcePoisson(Src_Term);
    
    /*--- Compute the source residual ---*/
    
    numerics->ComputeResidual(Residual, Jacobian_i, config);

    /*--- Add the source residual to the total ---*/
    
    //cout<<"iPoint: "<<iPoint<<" Source Residual: "<<Residual[0]<<endl;
    

    LinSysRes.AddBlock(iPoint, Residual);
    
    //Source term is constant ==> jacobian is zero
  }
}

void CPoissonSolverFVM::AssembleCoeffMatrix(CGeometry *geometry, CSolver **solver_container, CNumerics *numerics,
                                     CConfig *config, unsigned short iMesh, unsigned short iRKStep) {

	su2double dist_ij_2,Edge_Vector[3],proj_vector_ij,*Normal,Area,*Src_Term,cross_prod;
	su2double **Sol_i_Grad,**Sol_j_Grad,*Correction,*Grad_Norm,*Grad_Edge;
	unsigned long iEdge, iPoint, jPoint,iDim,iMarker,Point,iVertex,iNeigh,iVar;
	bool collinear;

	Src_Term = new su2double [nVar];
	Correction = new su2double [nVar];
	Grad_Norm = new su2double [nVar];
	Grad_Edge = new su2double [nVar];
	/*--- Create the coefficient marix using FV discretization ---*/
	
	  CoeffMatrix.Initialize(nPoint, nPointDomain, nVar, nVar, true, geometry, config);
  
  /*--- Solution and residual vectors ---*/
  
  LinSysSol.Initialize(nPoint, nPointDomain, nVar, 0.0);
  LinSysRes.Initialize(nPoint, nPointDomain, nVar, 0.0);
	
	
	for (iEdge = 0; iEdge < geometry->GetnEdge(); iEdge++) {

		iPoint = geometry->edge[iEdge]->GetNode(0);
		jPoint = geometry->edge[iEdge]->GetNode(1);
    
		/*--- Points coordinates, and normal vector ---*/
		numerics->SetCoord(geometry->node[iPoint]->GetCoord(),
						geometry->node[jPoint]->GetCoord());
		numerics->SetNormal(geometry->edge[iEdge]->GetNormal());

		Normal = geometry->edge[iEdge]->GetNormal();
		Area = 0.0;
		dist_ij_2 = 0; proj_vector_ij = 0;
		for (iDim = 0; iDim < nDim; iDim++) {
			Edge_Vector[iDim] = geometry->node[jPoint]->GetCoord(iDim)-geometry->node[iPoint]->GetCoord(iDim);
			dist_ij_2 += Edge_Vector[iDim]*Edge_Vector[iDim];

			proj_vector_ij += Normal[iDim];
			Area += Normal[iDim]*Normal[iDim];
		}
		Area = sqrt(Area);
		
		if (dist_ij_2 == 0.0) proj_vector_ij = 0.0;
		else proj_vector_ij = proj_vector_ij/dist_ij_2;


		if (nDim==2) cross_prod = Edge_Vector[1]*Normal[0]-Edge_Vector[0]*Normal[1];
		else cross_prod = pow((Edge_Vector[2]*Normal[1]-Edge_Vector[1]*Normal[2]),2) + pow((Edge_Vector[2]*Normal[0]-Edge_Vector[0]*Normal[2]),2) + pow((Edge_Vector[1]*Normal[0]-Edge_Vector[0]*Normal[1]),2);
		collinear = false;
		if (fabs(cross_prod)<=1.0e-10) collinear =true;
		
		if (!collinear){
			Sol_i_Grad = node[iPoint]->GetGradient();
			Sol_j_Grad = node[jPoint]->GetGradient();

			for (iVar = 0; iVar < nVar; iVar++) {
				Grad_Edge[iVar] = 0.0;
				Grad_Norm[iVar] = 0.0;
				Correction[iVar] = 0.0;
				for (iDim = 0; iDim < nDim; iDim++) {
					Grad_Norm[iVar] += 0.5*(Sol_i_Grad[iVar][iDim]+Sol_i_Grad[iVar][iDim])*Normal[iDim];
					Grad_Edge[iVar] += 0.5*(Sol_i_Grad[iVar][iDim]+Sol_i_Grad[iVar][iDim])*Edge_Vector[iDim];
				}	
				Correction[iVar] = Correction[iVar] + Grad_Norm[iVar] ;
				Correction[iVar] = Correction[iVar] - Grad_Edge[iVar]*proj_vector_ij;
				Correction[iVar] = Correction[iVar]*Area;
			}
			cout<<"i: "<<iPoint<<"j: "<<jPoint<<"Correction: "<<Correction[0]<<endl;
			LinSysRes.AddBlock(iPoint, Correction);
			LinSysRes.SubtractBlock(jPoint, Correction);
		}
		
		
		CoeffMatrix_Node[0][0] = -Area*proj_vector_ij;
		
		CoeffMatrix.SubtractBlock(iPoint, iPoint, CoeffMatrix_Node);
		CoeffMatrix.AddBlock(iPoint, jPoint, CoeffMatrix_Node);
		
		CoeffMatrix_Node[0][0] = Area*proj_vector_ij;
		
		CoeffMatrix.SubtractBlock(jPoint, iPoint, CoeffMatrix_Node);
		CoeffMatrix.AddBlock(jPoint, jPoint, CoeffMatrix_Node);
		//cout<<"iPoint: "<<iPoint<<"jPoint: "<<jPoint<<" normals "<<Normal[0]<<" , "<<Normal[1]<<" Area "<<Area<<" proj_vector_ij: "<<proj_vector_ij<<" entry "<<CoeffMatrix_Node[0][0]<<endl;
		//cout<<"iPoint: "<<iPoint<<"jPoint: "<<jPoint<<" edge vector "<<Edge_Vector[0]<<" , "<<Edge_Vector[1]<<" dist_ij_2: "<<dist_ij_2<<endl;
		
		
		
	}

	/*--- Modify the RHS to account for dirichlet/neumann boundaries ---*/
	for (iMarker = 0; iMarker < config->GetnMarker_All(); iMarker++)
    switch (config->GetMarker_All_KindBC(iMarker)) {
      case DIRICHLET:
		for (iVertex = 0; iVertex < geometry->nVertex[iMarker]; iVertex++) {
			Point = geometry->vertex[iMarker][iVertex]->GetNode();
			for (iNeigh = 0; iNeigh < geometry->node[Point]->GetnPoint(); iNeigh++) {
				iPoint = geometry->node[Point]->GetPoint(iNeigh);
				Src_Term[0] = 1+2*(geometry->node[Point]->GetCoord(0))*(geometry->node[Point]->GetCoord(0))+3*(geometry->node[Point]->GetCoord(1))*(geometry->node[Point]->GetCoord(1));
				Src_Term[0] = Src_Term[0]*CoeffMatrix.GetBlock(iPoint,Point,0,0);
				LinSysRes.SubtractBlock(iPoint, Src_Term);
				//cout<<"Normal value of "<<iPoint<<" and "<<Point<<" is "<<Normal[0]<<" , "<<Normal[1]<<endl;
				Src_Term[0]=0.0;
				CoeffMatrix.SetBlock(iPoint,Point,Src_Term);
			}
		}
        break;
	}
	delete Src_Term;
	delete Grad_Edge;
	delete Grad_Norm;
	delete Correction;

}
void CPoissonSolverFVM::ImplicitEuler_Iteration(CGeometry *geometry, CSolver **solver_container, CConfig *config) {
  
  unsigned long iPoint, total_index;
  unsigned short iVar;
  su2double *local_Residual, *local_Res_TruncError, Vol, Delta, Res;
  
	/*--- Build implicit system ---*/
  

	/*--- Set maximum residual to zero ---*/

  for (iVar = 0; iVar < nVar; iVar++) {
    SetRes_RMS(iVar, 0.0);
    SetRes_Max(iVar, 0.0, 0);
    //local_Res_TruncError[iVar] = 0.0;
}
  
	/*--- Initialize residual and solution at the ghost points ---*/
  
  for (iPoint = 0; iPoint < nPointDomain; iPoint++) {
	  
	 /*--- Read the residual ---*/
     //if (config->GetnMGLevels() > 0) local_Res_TruncError = node[iPoint]->GetResTruncError();

	/*--- Read the volume ---*/

    Vol = geometry->node[iPoint]->GetVolume();

	/*--- Modify matrix diagonal to assure diagonal dominance ---*/

    if (node[iPoint]->GetDelta_Time() != 0.0) {
      Delta = Vol / node[iPoint]->GetDelta_Time();
      Jacobian.AddVal2Diag(iPoint, Delta);
     }
    else {
      Jacobian.SetVal2Diag(iPoint, 1.0);
      for (iVar = 0; iVar < nVar; iVar++) {
        total_index = iPoint*nVar + iVar;
        LinSysRes[total_index] = 0.0;
        local_Res_TruncError[iVar] = 0.0;
      }
    }

 
	/*--- Right hand side of the system (-Residual) and initial guess (x = 0) ---*/
    for (iVar = 0; iVar < nVar; iVar++) {
      total_index = iPoint*nVar+iVar;
      LinSysRes[total_index] = - (LinSysRes[total_index] );//+ local_Res_TruncError[iVar] );
      LinSysSol[total_index] = 0.0;
      AddRes_RMS(iVar, LinSysRes[total_index]*LinSysRes[total_index]);
      AddRes_Max(iVar, fabs(LinSysRes[total_index]), geometry->node[iPoint]->GetGlobalIndex(), geometry->node[iPoint]->GetCoord());
    }
  }
  
	/*--- Initialize residual and solution at the ghost points ---*/
   for (iPoint = nPointDomain; iPoint < nPoint; iPoint++) {
    for (iVar = 0; iVar < nVar; iVar++) {
      total_index = iPoint*nVar + iVar;
      LinSysRes[total_index] = 0.0;
      LinSysSol[total_index] = 0.0;
    }
  }
  
  /*--- Solve or smooth the linear system ---*/
  
  CSysSolve system;
  system.Solve(Jacobian, LinSysRes, LinSysSol, geometry, config);
  
  for (iPoint = 0; iPoint < nPointDomain; iPoint++) {
    for (iVar = 0; iVar < nVar; iVar++) {
      node[iPoint]->AddSolution(iVar, LinSysSol[iPoint*nVar+iVar]);
     }
  }


  /*--- MPI solution ---*/

  Set_MPI_Solution(geometry, config);

  /*--- Compute the root mean square residual ---*/

  SetResidual_RMS(geometry, config);
  
}


void CPoissonSolverFVM::ExplicitEuler_Iteration(CGeometry *geometry, CSolver **solver_container, CConfig *config) {

  su2double *local_Residual, *local_Res_TruncError, Vol, Delta, Res;
  unsigned short iVar;
  unsigned long iPoint;

  bool adjoint = false;//config->GetContinuous_Adjoint();

  for (iVar = 0; iVar < nVar; iVar++) {
    SetRes_RMS(iVar, 0.0);
    SetRes_Max(iVar, 0.0, 0);
    local_Res_TruncError[iVar] = 0.0;
  }

  /*--- Update the solution ---*/
  
  for (iPoint = 0; iPoint < nPointDomain; iPoint++) {
	Vol = geometry->node[iPoint]->GetVolume();
    Delta = node[iPoint]->GetDelta_Time() / Vol;
    
    if (config->GetnMGLevels() > 0) local_Res_TruncError = node[iPoint]->GetResTruncError();
    local_Residual = LinSysRes.GetBlock(iPoint);

    if (!adjoint) {
      for (iVar = 0; iVar < nVar; iVar++) {
        Res = local_Residual[iVar] + local_Res_TruncError[iVar];
        node[iPoint]->AddSolution(iVar, -Res*Delta);
        AddRes_RMS(iVar, Res*Res);
        AddRes_Max(iVar, fabs(Res), geometry->node[iPoint]->GetGlobalIndex(), geometry->node[iPoint]->GetCoord());
      }
    }
  }

  /*--- MPI solution ---*/

  Set_MPI_Solution(geometry, config);

  /*--- Compute the root mean square residual ---*/

  SetResidual_RMS(geometry, config);

}

void CPoissonSolverFVM::Direct_Solve(CGeometry *geometry, CSolver **solver_container, CConfig *config) {

  unsigned long  iPoint, total_index,j,Point, iVertex,iDim,iNeigh,maxNeigh,iMarker;
  su2double      *Normal,Area,dist_ij_2,Edge_Vector[3],proj_vector_ij,*Src_Term;
  unsigned short iVar;


  /*--- Build implicit system ---*/
  
   for (iPoint = 0; iPoint < geometry->GetnPoint(); iPoint++) {
    
    /*--- Right hand side of the system (-Residual) and initial guess (x = 0) ---*/
    for (iVar = 0; iVar < nVar; iVar++) {
      total_index = iPoint*nVar+iVar;
      LinSysRes[total_index] =  (LinSysRes[total_index]);
      LinSysSol[total_index] = 0.0;
    }
    
  }
  
  /*--- Initialize residual and solution at the ghost points ---*/
  
  for (iPoint = geometry->GetnPointDomain(); iPoint < geometry->GetnPoint(); iPoint++) {
    for (iVar = 0; iVar < nVar; iVar++) {
      total_index = iPoint*nVar + iVar;
      LinSysRes[total_index] = 0.0;
      LinSysSol[total_index] = 0.0;
    }
  }
  
  cout<<"Jacobian matrix"<<endl;
  for (iPoint=0;iPoint<geometry->GetnPoint();iPoint++){
	  cout<<iPoint<<" : ";
   for (j=0;j<geometry->GetnPointDomain();j++)
       cout<<CoeffMatrix.GetBlock(iPoint,j,0,0)<<" , ";
   cout<<endl;
  }
  
  
   cout<<"RHS"<<endl;
  for (iPoint=0;iPoint<geometry->GetnPoint();iPoint++){
	 cout<<iPoint<<" : ";
    cout<<LinSysRes.GetBlock(iPoint,0);
   cout<<endl;
  }
  
     /* cout<<"BC"<<endl;
  for (iPoint=0;iPoint<geometry->GetnPoint();iPoint++){
	  cout<<iPoint<<" : ";
      cout<<LinSysSol.GetBlock(iPoint,0);
   cout<<endl;
  }*/
   
  
  /*--- Solve or smooth the linear system ---*/
  
  CSysSolve system;
  system.Solve(CoeffMatrix, LinSysRes, LinSysSol, geometry, config);
  
  /*--- Update solution (system written in terms of increments) ---*/
  
  for (iPoint = 0; iPoint < nPointDomain; iPoint++) {
    for (iVar = 0; iVar < nVar; iVar++) {
      node[iPoint]->AddSolution(iVar, LinSysSol[iPoint*nVar+iVar]);
    }
  }
  
  /*--- MPI solution ---*/
  
  Set_MPI_Solution(geometry, config);
  
  /*---  Compute the residual Ax-f ---*/
  
  CoeffMatrix.ComputeResidual(LinSysSol, LinSysRes, LinSysAux);
  
  /*--- Set maximum residual to zero ---*/
  
  for (iVar = 0; iVar < nVar; iVar++) {
    SetRes_RMS(iVar, 0.0);
    SetRes_Max(iVar, 0.0, 0);
  }
  
  /*--- Compute the residual ---*/
  
  for (iPoint = 0; iPoint < geometry->GetnPoint(); iPoint++) {
    for (iVar = 0; iVar < nVar; iVar++) {
      total_index = iPoint*nVar+iVar;
      AddRes_RMS(iVar, LinSysAux[total_index]*LinSysAux[total_index]);
      AddRes_Max(iVar, fabs(LinSysAux[total_index]), geometry->node[iPoint]->GetGlobalIndex(), geometry->node[iPoint]->GetCoord());
    }
  }
  
  /*--- Compute the root mean square residual ---*/
  
  //SetResidual_RMS(geometry, config);
  

	
	
}

void CPoissonSolverFVM::SetTime_Step(CGeometry *geometry, CSolver **solver_container, CConfig *config,
                        unsigned short iMesh, unsigned long Iteration){
							
   unsigned short iDim, iMarker;
   unsigned long iEdge, iVertex, iPoint = 0, jPoint = 0;
   su2double *Normal, Area, Poisson_Coeff, Lambda;
   su2double Global_Delta_Time, Local_Delta_Time,Vol, CFL_Reduction;
   su2double Edge_Vector[3],dist_ij_2,Volume;
   su2double *Coord_i,*Coord_j,Mom_Coeff;
   
   
   Min_Delta_Time = 1.E6; Max_Delta_Time = 0.0;Global_Delta_Time = 1.E6;
   
   
   /*---------Compute eigen value-------------*/
   
   for (iPoint = 0; iPoint < nPointDomain; iPoint++) {
       node[iPoint]->SetMax_Lambda_Visc(0.0);
   }
   
   /*--- Loop interior edges ---*/
  for (iEdge = 0; iEdge < geometry->GetnEdge(); iEdge++) {

    iPoint = geometry->edge[iEdge]->GetNode(0);
    jPoint = geometry->edge[iEdge]->GetNode(1);
    Coord_i = geometry->node[iPoint]->GetCoord();
    Coord_j = geometry->node[jPoint]->GetCoord();

    /*--- get the edge's normal vector to compute the edge's area ---*/
    Normal = geometry->edge[iEdge]->GetNormal();
    
    Area = 0;
    for (iDim = 0; iDim < nDim; iDim++) {
		Edge_Vector[iDim] = Coord_j[iDim]-Coord_i[iDim];
		dist_ij_2 += Edge_Vector[iDim]*Edge_Vector[iDim];
		Area += Normal[iDim]*Normal[iDim]; Area = sqrt(Area);
	}
	
    Volume = 0.5*(geometry->node[iPoint]->GetVolume() + geometry->node[jPoint]->GetVolume());
    
    //Mom_Coeff = 0.5*(solver_container[FLOW_SOL]->node[iPoint]->GetMean_Mom_Coeff()+solver_container[FLOW_SOL]->node[iPoint]->GetMean_Mom_Coeff());//config->GetPoisson_Coeff
    Poisson_Coeff = 0.5 * (node[iPoint]->GetPoisson_Coeff() + node[iPoint]->GetPoisson_Coeff());
    Lambda = Poisson_Coeff*Area*Area;
    
    if (geometry->node[iPoint]->GetDomain()) node[iPoint]->AddMax_Lambda_Visc(Lambda);
    if (geometry->node[jPoint]->GetDomain()) node[jPoint]->AddMax_Lambda_Visc(Lambda);
    
   }
   
    /*--- Loop boundary edges ---*/
   for (iMarker = 0; iMarker < geometry->GetnMarker(); iMarker++) {
    for (iVertex = 0; iVertex < geometry->GetnVertex(iMarker); iVertex++) {

      /*--- Point identification, Normal vector and area ---*/

      iPoint = geometry->vertex[iMarker][iVertex]->GetNode();
      Normal = geometry->vertex[iMarker][iVertex]->GetNormal();
      Area = 0.0; for (iDim = 0; iDim < nDim; iDim++) Area += Normal[iDim]*Normal[iDim]; Area = sqrt(Area);
      Volume = geometry->node[iPoint]->GetVolume(); 
      
      
      Poisson_Coeff = node[iPoint]->GetPoisson_Coeff();
      Lambda = Poisson_Coeff*Area*Area;
     
      if (geometry->node[iPoint]->GetDomain()) node[iPoint]->AddMax_Lambda_Visc(Lambda);
      
      
    }
   }

   /*--- Each element uses their own speed, steady state simulation ---*/

   for (iPoint = 0; iPoint < nPointDomain; iPoint++) {

    Vol = geometry->node[iPoint]->GetVolume();

	if (Vol != 0.0) {
		Local_Delta_Time = config->GetCFL(iMesh)*Vol*Vol/node[iPoint]->GetMax_Lambda_Visc();
		//cout<<"CFL, Lambda and time step: "<<config->GetCFL(iMesh)<<", "<<node[iPoint]->GetMax_Lambda_Visc()<<" and "<<Local_Delta_Time<<endl;

		/*--- Min-Max-Logic ---*/
		Global_Delta_Time = min(Global_Delta_Time, Local_Delta_Time);
		Min_Delta_Time = min(Min_Delta_Time, Local_Delta_Time);
		Max_Delta_Time = max(Max_Delta_Time, Local_Delta_Time);
		if (Local_Delta_Time > config->GetMax_DeltaTime())
			Local_Delta_Time = config->GetMax_DeltaTime();
			node[iPoint]->SetDelta_Time(Local_Delta_Time);
		}
		else {
			node[iPoint]->SetDelta_Time(0.0);
		}

   }


}


su2double CPoissonSolverFVM::GetDirichlet_BC(CGeometry *geometry, CConfig *config, unsigned long Point){
	
	su2double dirichlet_bc;
	
	if (config->GetKind_Incomp_System() == PRESSURE_BASED ) 
	    dirichlet_bc = 0.0;
	else 
	    dirichlet_bc = cos(geometry->node[Point]->GetCoord(0))*cosh(geometry->node[Point]->GetCoord(1));
	
	return dirichlet_bc;
	
}


void CPoissonSolverFVM::BC_Dirichlet(CGeometry *geometry, CSolver **solver_container,
                                  CConfig *config, unsigned short val_marker) {
  unsigned long Point, iVertex;
  su2double val_res,pi;
  string Marker_Tag = config->GetMarker_All_TagBound(val_marker);
  unsigned short iVar = 0;
  pi = 4.0*atan(1.0);

  for (iVertex = 0; iVertex < geometry->nVertex[val_marker]; iVertex++) {
    Point = geometry->vertex[val_marker][iVertex]->GetNode();
   
    Solution[0] = GetDirichlet_BC(geometry,config,Point);

    
 /*--- Assign the dirichlet BC value to the solution ---*/
    node[Point]->SetSolution(Solution);
    node[Point]->Set_OldSolution();
    
	Jacobian.DeleteValsRowi(Point);
    LinSysRes.SetBlock_Zero(Point, iVar);
    if (config->GetnMGLevels() > 0) node[Point]->SetVal_ResTruncError_Zero(iVar);
    LinSysSol.SetBlock(Point, Solution);
  }

  
  
  
}


void CPoissonSolverFVM::BC_Neumann(CGeometry *geometry, CSolver **solver_container, CNumerics *numerics, CConfig *config,
                                unsigned short val_marker) { 
									
									
  unsigned short iDim;
  unsigned long iVertex, iPoint;
  su2double NeumannFlux, Area, *Normal,*Res_Visc;

  string Marker_Tag = config->GetMarker_All_TagBound(val_marker);

  NeumannFlux = 0.0;

  Res_Visc = new su2double[nVar];

  for (iVertex = 0; iVertex < geometry->nVertex[val_marker]; iVertex++) {

    iPoint = geometry->vertex[val_marker][iVertex]->GetNode();

    if (geometry->node[iPoint]->GetDomain()) {

      Normal = geometry->vertex[val_marker][iVertex]->GetNormal();
      Area = 0.0;
      for (iDim = 0; iDim < nDim; iDim++)
        Area += Normal[iDim]*Normal[iDim];
      Area = sqrt (Area);

      Res_Visc[0] = NeumannFlux * Area;

      /*--- Viscous contribution to the residual at the wall ---*/

      LinSysRes.SubtractBlock(iPoint, Res_Visc);
    }

   }
}


void CPoissonSolverFVM::BC_Far_Field(CGeometry *geometry, CSolver **solver_container, CNumerics *conv_numerics,
                                CNumerics *visc_numerics, CConfig *config, unsigned short val_marker) {
						
	BC_Dirichlet(geometry, solver_container, config, val_marker);
									}
                                
                                
                                
void CPoissonSolverFVM::BC_Euler_Wall(CGeometry *geometry, CSolver **solver_container,
                                 CNumerics *numerics, CConfig *config, unsigned short val_marker) {
									
	BC_Neumann(geometry, solver_container, numerics, config, val_marker) ;
									 }
                                 
                                 
void CPoissonSolverFVM::BC_Inlet(CGeometry *geometry, CSolver **solver_container,
                            CNumerics *conv_numerics, CNumerics *visc_numerics, CConfig *config, unsigned short val_marker) {
								
   BC_Dirichlet(geometry, solver_container, config, val_marker);
}


void CPoissonSolverFVM::BC_Outlet(CGeometry *geometry, CSolver **solver_container,
                            CNumerics *conv_numerics, CNumerics *visc_numerics, CConfig *config, unsigned short val_marker) {
								
   BC_Dirichlet(geometry, solver_container, config, val_marker);
}

/*-----------------------------------------------------------------------------------------------*/
/*---------------------------Finite Element Solver (will be deleted soon)------------------------*/
/*-----------------------------------------------------------------------------------------------*/
CPoissonSolver::CPoissonSolver(void) : CSolver() { }

CPoissonSolver::CPoissonSolver(CGeometry *geometry, CConfig *config) : CSolver() {
  
  unsigned long nPoint, iPoint;
  unsigned short iVar, iDim;
  
  
  nDim =          geometry->GetnDim();
  nPoint =        geometry->GetnPoint();
  nPointDomain =  geometry->GetnPointDomain();
  nVar =          1;
  node =          new CVariable*[nPoint];
  
  /*--- Initialize nVarGrad for deallocation ---*/
  
  nVarGrad = nVar;
  
  Residual = new su2double[nVar]; Residual_RMS = new su2double[nVar];
  Solution = new su2double[nVar];
  Residual_Max = new su2double[nVar];
  
  /*--- Define some structures for locating max residuals ---*/
  
  Point_Max = new unsigned long[nVar];
  for (iVar = 0; iVar < nVar; iVar++) Point_Max[iVar] = 0;
  Point_Max_Coord = new su2double*[nVar];
  for (iVar = 0; iVar < nVar; iVar++) {
    Point_Max_Coord[iVar] = new su2double[nDim];
    for (iDim = 0; iDim < nDim; iDim++) Point_Max_Coord[iVar][iDim] = 0.0;
  }
  
  /*--- Point to point stiffness matrix ---*/
  
  if (nDim == 2) {
    StiffMatrix_Elem = new su2double* [3];
    Source_Vector   = new su2double [3];
    for (unsigned short iVar = 0; iVar < 3; iVar++) {
      StiffMatrix_Elem[iVar] = new su2double [3];
    }
  }
  
  if (nDim == 3) {
    StiffMatrix_Elem = new su2double* [4];
    Source_Vector   = new su2double [4];
    for (unsigned short iVar = 0; iVar < 4; iVar++) {
      StiffMatrix_Elem[iVar] = new su2double [4];
    }
  }
  
  StiffMatrix_Node = new su2double* [1];
  for (unsigned short iVar = 0; iVar < 1; iVar++) {
    StiffMatrix_Node[iVar] = new su2double [1];
  }
  
  /*--- Initialization of the structure of the whole Jacobian ---*/
  if (rank == MASTER_NODE) cout << "Initialize Jacobian structure (Poisson equation)." << endl;
  StiffMatrix.Initialize(nPoint, nPointDomain, nVar, nVar, true, geometry, config);
  
  /*--- Solution and residual vectors ---*/
  
  LinSysSol.Initialize(nPoint, nPointDomain, nVar, 0.0);
  LinSysRes.Initialize(nPoint, nPointDomain, nVar, 0.0);
  LinSysAux.Initialize(nPoint, nPointDomain, nVar, 0.0);

  /*--- Computation of gradients by least squares ---*/
  		//CoeffMatrixPoisson.SubtractBlock(iPoint, jPoint, proj_vector_ij);
  Smatrix = new su2double* [nDim]; // S matrix := inv(R)*traspose(inv(R))
  for (iDim = 0; iDim < nDim; iDim++)
    Smatrix[iDim] = new su2double [nDim];
  
  Cvector = new su2double* [nVar]; // c vector := transpose(WA)*(Wb)
  for (iVar = 0; iVar < nVar; iVar++)
    Cvector[iVar] = new su2double [nDim];
  
  /*--- Always instantiate and initialize the variable to a zero value. ---*/
  
  for (iPoint = 0; iPoint < nPoint; iPoint++)
    node[iPoint] = new CPotentialVariable(0.0, nDim, nVar, config);
  
}

CPoissonSolver::~CPoissonSolver(void) {
  
  unsigned short iVar;
  
  delete [] Source_Vector;
  
  if (nDim == 2) {
    for (iVar = 0; iVar < 3; iVar++)
      delete [] StiffMatrix_Elem[iVar];
  }
  
  if (nDim == 3) {
    for (iVar = 0; iVar < 4; iVar++)
      delete [] StiffMatrix_Elem[iVar];
  }
  
  for (iVar = 0; iVar < 1; iVar++)
    delete [] StiffMatrix_Node[iVar];
  
  delete [] StiffMatrix_Elem;
  delete [] StiffMatrix_Node;
  
}

void CPoissonSolver::Preprocessing(CGeometry *geometry, CSolver **solver_container,
                                   CConfig *config, unsigned short iMesh, unsigned short iRKStep, unsigned short RunTime_EqSystem, bool Output) {
  unsigned long iPoint;
  
  for (iPoint = 0; iPoint < geometry->GetnPoint(); iPoint ++)
    LinSysRes.SetBlock_Zero(iPoint);
  
  StiffMatrix.SetValZero();
  
}

void CPoissonSolver::Compute_Residual(CGeometry *geometry, CSolver **solver_container, CConfig *config,
                                      unsigned short iMesh) {
  
  unsigned long iPoint;
  unsigned short iVar = 0;
  
  /*--- Build linear system ---*/
  for (iPoint = 0; iPoint < geometry->GetnPointDomain(); iPoint++) {
    LinSysRes[iPoint] = LinSysRes.GetBlock(iPoint, iVar);
    LinSysSol[iPoint] = node[iPoint]->GetSolution(iVar);
  }
  
  StiffMatrix.MatrixVectorProduct(LinSysSol, LinSysRes);
  
  /*--- Update residual ---*/
  for (iPoint = 0; iPoint < geometry->GetnPointDomain(); iPoint++) {
    LinSysRes.SetBlock(iPoint, 0, LinSysRes[iPoint]);
  }
}

/*!
 * \method Source_Residual
 * \brief Source terms of the poisson solver
 * \author A. Lonkar
 */
void CPoissonSolver::Source_Residual(CGeometry *geometry, CSolver **solver_container, CNumerics *numerics, CNumerics *second_numerics,
                                     CConfig *config, unsigned short iMesh) {
//  
//  unsigned long iElem, Point_0 = 0, Point_1 = 0, Point_2 = 0, Point_3 = 0;
//  su2double a[3], b[3], c[3], d[3], Area_Local, Volume_Local;
//  //  su2double Local_Delta_Time;
//  su2double **Gradient_0, **Gradient_1, **Gradient_2, **Gradient_3;
//  su2double *Coord_0 = NULL, *Coord_1= NULL, *Coord_2= NULL, *Coord_3= NULL;;
//  unsigned short iDim;
//  su2double  dt;
//  //  su2double  dx, u, c;
//  bool MacCormack_relaxation = (config->GetMacCormackRelaxation());
//  
//  if (nDim == 2) {
//    if (config->GetPoissonSolver()) {
//      
//      for (iElem = 0; iElem < geometry->GetnElem(); iElem++) {
//        
//        Point_0 = geometry->elem[iElem]->GetNode(0);
//        Point_1 = geometry->elem[iElem]->GetNode(1);
//        Point_2 = geometry->elem[iElem]->GetNode(2);
//        
//        Coord_0 = geometry->node[Point_0]->GetCoord();
//        Coord_1 = geometry->node[Point_1]->GetCoord();
//        Coord_2 = geometry->node[Point_2]->GetCoord();
//
//        for (iDim = 0; iDim < nDim; iDim++) {
//          a[iDim] = Coord_0[iDim]-Coord_2[iDim];
//          b[iDim] = Coord_1[iDim]-Coord_2[iDim];
//        }
//        
//        Area_Local = 0.5*fabs(a[0]*b[1]-a[1]*b[0]);
//        
//        Gradient_0 = node[Point_0]->GetPlasmaRhoUGradient();
//        Gradient_1 = node[Point_1]->GetPlasmaRhoUGradient();
//        Gradient_2 = node[Point_2]->GetPlasmaRhoUGradient();
//        
//        numerics->SetVolume(Area_Local);
//        
//        dt = node[Point_0]->GetPlasmaTimeStep();
//        
//        /*    u = 4800;
//         c = 87110;
//         c = 800;
//         dx = 0.004/81;
//         Local_Delta_Time = config->GetCFL(iMesh) * dx/(u+c);
//         numerics->SetTimeStep(Local_Delta_Time);
//         */
//        
//        numerics->SetCoord(Coord_0, Coord_1, Coord_2);
//        numerics->SetTimeStep(dt);
//        numerics->SetChargeDensity(node[Point_0]->GetChargeDensity(), node[Point_1]->GetChargeDensity(), node[Point_2]->GetChargeDensity(), node[Point_3]->GetChargeDensity());
//        numerics->SetConsVarGradient(Gradient_0, Gradient_1, Gradient_2 );
//        numerics->ComputeResidual_MacCormack(Source_Vector, config);
//        
//        LinSysRes.AddBlock(Point_0, &Source_Vector[0]);
//        LinSysRes.AddBlock(Point_1, &Source_Vector[1]);
//        LinSysRes.AddBlock(Point_2, &Source_Vector[2]);
//
//        if (geometry->elem[iElem]->GetVTK_Type() == QUADRILATERAL) {
//          
//          Point_0 = geometry->elem[iElem]->GetNode(3);
//          Point_1 = geometry->elem[iElem]->GetNode(0);
//          Point_2 = geometry->elem[iElem]->GetNode(2);
//          
//          Coord_0 = geometry->node[Point_0]->GetCoord();
//          Coord_1 = geometry->node[Point_1]->GetCoord();
//          Coord_2 = geometry->node[Point_2]->GetCoord();
//
//          for (iDim = 0; iDim < nDim; iDim++) {
//            a[iDim] = Coord_0[iDim]-Coord_2[iDim];
//            b[iDim] = Coord_1[iDim]-Coord_2[iDim];
//          }
//          
//          Area_Local = 0.5*fabs(a[0]*b[1]-a[1]*b[0]);
//          
//          Gradient_0 = node[Point_0]->GetPlasmaRhoUGradient();
//          Gradient_1 = node[Point_1]->GetPlasmaRhoUGradient();
//          Gradient_2 = node[Point_2]->GetPlasmaRhoUGradient();
//          
//          numerics->SetVolume(Area_Local);
//          
//          /*    u = 4800;
//           c = 87110;
//           c = 732.0;
//           dx = 0.004/81;
//           Local_Delta_Time = config->GetCFL(iMesh) * dx/(u+c);
//           numerics->SetTimeStep(Local_Delta_Time);
//           */
//          
//          dt = node[Point_0]->GetPlasmaTimeStep();
//          numerics->SetCoord(Coord_0, Coord_1, Coord_2);
//          numerics->SetTimeStep(dt);
//          numerics->SetChargeDensity(node[Point_0]->GetChargeDensity(), node[Point_1]->GetChargeDensity(), node[Point_2]->GetChargeDensity(), node[Point_3]->GetChargeDensity());
//          numerics->SetConsVarGradient(Gradient_0, Gradient_1, Gradient_2 );
//          numerics->ComputeResidual_MacCormack(Source_Vector, config);
//          LinSysRes.AddBlock(Point_0, &Source_Vector[0]);
//          LinSysRes.AddBlock(Point_1, &Source_Vector[1]);
//          LinSysRes.AddBlock(Point_2, &Source_Vector[2]);
//        }
//      }
//    }
//  }
//  if (nDim == 3) {
//    if (config->GetPoissonSolver()) {
//      for (iElem = 0; iElem < geometry->GetnElem(); iElem++) {
//        Point_0 = geometry->elem[iElem]->GetNode(0);  Coord_0 = geometry->node[Point_0]->GetCoord();
//        Point_1 = geometry->elem[iElem]->GetNode(1);  Coord_1 = geometry->node[Point_1]->GetCoord();
//        Point_2 = geometry->elem[iElem]->GetNode(2);  Coord_2 = geometry->node[Point_2]->GetCoord();
//        Point_3 = geometry->elem[iElem]->GetNode(3);  Coord_3 = geometry->node[Point_3]->GetCoord();
//        
//        for (iDim = 0; iDim < nDim; iDim++) {
//          a[iDim] = Coord_0[iDim]-Coord_2[iDim];
//          b[iDim] = Coord_1[iDim]-Coord_2[iDim];
//          c[iDim] = Coord_3[iDim]-Coord_2[iDim];
//        }
//        
//        d[0] = a[1]*b[2]-a[2]*b[1];
//        d[1] = -(a[0]*b[2]-a[2]*b[0]);
//        d[2] = a[0]*b[1]-a[1]*b[0];
//        
//        /*--- Compute element volume ---*/
//        Volume_Local = fabs(c[0]*d[0] + c[1]*d[1] + c[2]*d[2])/6.0;
//        numerics->SetVolume(Volume_Local);
//        numerics->SetChargeDensity(node[Point_0]->GetChargeDensity(), node[Point_1]->GetChargeDensity(), node[Point_2]->GetChargeDensity(), node[Point_3]->GetChargeDensity());
//        
//        if (MacCormack_relaxation) {
//          
//          Gradient_0 = node[Point_0]->GetPlasmaRhoUGradient();
//          Gradient_1 = node[Point_1]->GetPlasmaRhoUGradient();
//          Gradient_2 = node[Point_2]->GetPlasmaRhoUGradient();
//          Gradient_3 = node[Point_3]->GetPlasmaRhoUGradient();
//          numerics->SetCoord(Coord_0, Coord_1, Coord_2, Coord_3);
//          numerics->SetConsVarGradient(Gradient_0, Gradient_1, Gradient_2, Gradient_3 );
//          numerics->ComputeResidual_MacCormack(Source_Vector, config);
//        }
//        else numerics->ComputeResidual(Source_Vector, config);
//        
//        LinSysRes.AddBlock(Point_0, &Source_Vector[0]);
//        LinSysRes.AddBlock(Point_1, &Source_Vector[1]);
//        LinSysRes.AddBlock(Point_2, &Source_Vector[2]);
//        LinSysRes.AddBlock(Point_3, &Source_Vector[3]);
//      }
//    }
//  }
}

void CPoissonSolver::Source_Template(CGeometry *geometry, CSolver **solver_container, CNumerics *numerics,
                                     CConfig *config, unsigned short iMesh) {
}


/*!
 * \method Copy_Zone_Solution
 * \brief Copy solution from solver 1 into solver 2
 * \author A. Lonkar
 */
void CPoissonSolver::Copy_Zone_Solution(CSolver ***solver1_solution,
                                        CGeometry **solver1_geometry,
                                        CConfig *solver1_config,
                                        CSolver ***solver2_solution,
                                        CGeometry **solver2_geometry,
                                        CConfig *solver2_config) {
  unsigned long iPoint;
  unsigned short iDim;
  su2double neg_EFvalue;
  su2double *E_field = new su2double [nDim];
  
  for (iPoint = 0; iPoint < solver1_geometry[MESH_0]->GetnPointDomain(); iPoint++) {
    for (iDim =0; iDim < nDim; iDim ++) {
      neg_EFvalue = solver1_solution[MESH_0][POISSON_SOL]->node[iPoint]->GetGradient(0, iDim);
      E_field[iDim] = -1.0*neg_EFvalue;
    }
  }
};

/*!
 * \method Galerkin_Method
 * \brief calculate the element stiffness matrix
 * \author A. Lonkar
 */
void CPoissonSolver::Viscous_Residual(CGeometry *geometry, CSolver **solver_container, CNumerics *numerics,
                                     CConfig *config, unsigned short iMesh, unsigned short iRKStep) {
  
  unsigned long iElem, Point_0 = 0, Point_1 = 0, Point_2 = 0, Point_3 = 0;
  su2double *Coord_0 = NULL, *Coord_1= NULL, *Coord_2= NULL, *Coord_3 = NULL;
  
  if (nDim == 2 ) {
    for (iElem = 0; iElem < geometry->GetnElem(); iElem++) {
      
      Point_0 = geometry->elem[iElem]->GetNode(0);
      Point_1 = geometry->elem[iElem]->GetNode(1);
      Point_2 = geometry->elem[iElem]->GetNode(2);
      
      Coord_0 = geometry->node[Point_0]->GetCoord();
      Coord_1 = geometry->node[Point_1]->GetCoord();
      Coord_2 = geometry->node[Point_2]->GetCoord();
      
      numerics->SetCoord(Coord_0, Coord_1, Coord_2);
      numerics->ComputeResidual(StiffMatrix_Elem, config);
      AddStiffMatrix(StiffMatrix_Elem, Point_0, Point_1, Point_2, Point_3);
    }
  }
  
  if (nDim == 3 ) {
    
    for (iElem = 0; iElem < geometry->GetnElem(); iElem++) {
      
      Point_0 = geometry->elem[iElem]->GetNode(0);   Coord_0 = geometry->node[Point_0]->GetCoord();
      Point_1 = geometry->elem[iElem]->GetNode(1);  Coord_1 = geometry->node[Point_1]->GetCoord();
      Point_2 = geometry->elem[iElem]->GetNode(2);   Coord_2 = geometry->node[Point_2]->GetCoord();
      Point_3 = geometry->elem[iElem]->GetNode(3);  Coord_3 = geometry->node[Point_3]->GetCoord();
      
      numerics->SetCoord(Coord_0, Coord_1, Coord_2, Coord_3);
      numerics->ComputeResidual(StiffMatrix_Elem, config);
      AddStiffMatrix(StiffMatrix_Elem, Point_0, Point_1, Point_2, Point_3);
      
    }
  }
}

/*!
 * \method AddStiffMatrix
 * \brief Assemble the Global stiffness matrix
 * \author A. Lonkar
 */
void CPoissonSolver::AddStiffMatrix(su2double **StiffMatrix_Elem, unsigned long Point_0, unsigned long Point_1, unsigned long Point_2, unsigned long Point_3) {
  
  if (nDim == 2 ) {
    StiffMatrix_Node[0][0] = StiffMatrix_Elem[0][0]; StiffMatrix.AddBlock(Point_0, Point_0, StiffMatrix_Node);
    StiffMatrix_Node[0][0] = StiffMatrix_Elem[0][1]; StiffMatrix.AddBlock(Point_0, Point_1, StiffMatrix_Node);
    StiffMatrix_Node[0][0] = StiffMatrix_Elem[0][2]; StiffMatrix.AddBlock(Point_0, Point_2, StiffMatrix_Node);
    StiffMatrix_Node[0][0] = StiffMatrix_Elem[1][0]; StiffMatrix.AddBlock(Point_1, Point_0, StiffMatrix_Node);
    StiffMatrix_Node[0][0] = StiffMatrix_Elem[1][1]; StiffMatrix.AddBlock(Point_1, Point_1, StiffMatrix_Node);
    StiffMatrix_Node[0][0] = StiffMatrix_Elem[1][2]; StiffMatrix.AddBlock(Point_1, Point_2, StiffMatrix_Node);
    StiffMatrix_Node[0][0] = StiffMatrix_Elem[2][0]; StiffMatrix.AddBlock(Point_2, Point_0, StiffMatrix_Node);
    StiffMatrix_Node[0][0] = StiffMatrix_Elem[2][1]; StiffMatrix.AddBlock(Point_2, Point_1, StiffMatrix_Node);
    StiffMatrix_Node[0][0] = StiffMatrix_Elem[2][2]; StiffMatrix.AddBlock(Point_2, Point_2, StiffMatrix_Node);
  }
  if (nDim == 3) {
    
    StiffMatrix_Node[0][0] = StiffMatrix_Elem[0][0]; StiffMatrix.AddBlock(Point_0, Point_0, StiffMatrix_Node);
    StiffMatrix_Node[0][0] = StiffMatrix_Elem[0][1]; StiffMatrix.AddBlock(Point_0, Point_1, StiffMatrix_Node);
    StiffMatrix_Node[0][0] = StiffMatrix_Elem[0][2]; StiffMatrix.AddBlock(Point_0, Point_2, StiffMatrix_Node);
    StiffMatrix_Node[0][0] = StiffMatrix_Elem[0][3]; StiffMatrix.AddBlock(Point_0, Point_3, StiffMatrix_Node);
    StiffMatrix_Node[0][0] = StiffMatrix_Elem[1][0]; StiffMatrix.AddBlock(Point_1, Point_0, StiffMatrix_Node);
    StiffMatrix_Node[0][0] = StiffMatrix_Elem[1][1]; StiffMatrix.AddBlock(Point_1, Point_1, StiffMatrix_Node);
    StiffMatrix_Node[0][0] = StiffMatrix_Elem[1][2]; StiffMatrix.AddBlock(Point_1, Point_2, StiffMatrix_Node);
    StiffMatrix_Node[0][0] = StiffMatrix_Elem[1][3]; StiffMatrix.AddBlock(Point_1, Point_3, StiffMatrix_Node);
    StiffMatrix_Node[0][0] = StiffMatrix_Elem[2][0]; StiffMatrix.AddBlock(Point_2, Point_0, StiffMatrix_Node);
    StiffMatrix_Node[0][0] = StiffMatrix_Elem[2][1]; StiffMatrix.AddBlock(Point_2, Point_1, StiffMatrix_Node);
    StiffMatrix_Node[0][0] = StiffMatrix_Elem[2][2]; StiffMatrix.AddBlock(Point_2, Point_2, StiffMatrix_Node);
    StiffMatrix_Node[0][0] = StiffMatrix_Elem[2][3]; StiffMatrix.AddBlock(Point_2, Point_3, StiffMatrix_Node);
    StiffMatrix_Node[0][0] = StiffMatrix_Elem[3][0]; StiffMatrix.AddBlock(Point_3, Point_0, StiffMatrix_Node);
    StiffMatrix_Node[0][0] = StiffMatrix_Elem[3][1]; StiffMatrix.AddBlock(Point_3, Point_1, StiffMatrix_Node);
    StiffMatrix_Node[0][0] = StiffMatrix_Elem[3][2]; StiffMatrix.AddBlock(Point_3, Point_2, StiffMatrix_Node);
    StiffMatrix_Node[0][0] = StiffMatrix_Elem[3][3]; StiffMatrix.AddBlock(Point_3, Point_3, StiffMatrix_Node);
    
  }
}

void CPoissonSolver::BC_Dirichlet(CGeometry *geometry, CSolver **solver_container,
                                  CConfig *config, unsigned short val_marker) {
  unsigned long Point, iVertex;
  
  /*--- Identify if a boundary is Dirichlet or Neumman ---*/
  for (iVertex = 0; iVertex < geometry->nVertex[val_marker]; iVertex++) {
    Point = geometry->vertex[val_marker][iVertex]->GetNode();
    Solution[0]= 10.0;
    node[Point]->SetSolution(Solution);

    LinSysRes.SetBlock(Point, Solution);
    LinSysSol.SetBlock(Point, Solution);

    StiffMatrix.DeleteValsRowi(Point); // & includes 1 in the diagonal
  }
  
}

void CPoissonSolver::BC_Neumann(CGeometry *geometry, CSolver **solver_container, CNumerics *numerics, CConfig *config,
                                unsigned short val_marker) { }

void CPoissonSolver::ImplicitEuler_Iteration(CGeometry *geometry, CSolver **solver_container, CConfig *config) {
  
  unsigned long iPoint, total_index;
  unsigned short iVar;
  
  /*--- Build implicit system ---*/
  
  for (iPoint = 0; iPoint < geometry->GetnPoint(); iPoint++) {
    
    /*--- Right hand side of the system (-Residual) and initial guess (x = 0) ---*/
    for (iVar = 0; iVar < nVar; iVar++) {
      total_index = iPoint*nVar+iVar;
      LinSysSol[total_index] = 0.0;
    }
    
  }
  
  /*--- Initialize residual and solution at the ghost points ---*/
  
  for (iPoint = geometry->GetnPointDomain(); iPoint < geometry->GetnPoint(); iPoint++) {
    for (iVar = 0; iVar < nVar; iVar++) {
      total_index = iPoint*nVar + iVar;
      LinSysRes[total_index] = 0.0;
      LinSysSol[total_index] = 0.0;
    }
  }
  
  /*--- Solve or smooth the linear system ---*/
  
  CSysSolve system;
  system.Solve(StiffMatrix, LinSysRes, LinSysSol, geometry, config);
  
  /*--- Update solution (system written in terms of increments) ---*/
  
  for (iPoint = 0; iPoint < geometry->GetnPoint(); iPoint++) {
    for (iVar = 0; iVar < nVar; iVar++) {
      node[iPoint]->SetSolution(iVar, LinSysSol[iPoint*nVar+iVar]);
    }
  }
  
  /*--- MPI solution ---*/
  
  Set_MPI_Solution(geometry, config);
  
  /*---  Compute the residual Ax-f ---*/
  
  StiffMatrix.ComputeResidual(LinSysSol, LinSysRes, LinSysAux);
  
  /*--- Set maximum residual to zero ---*/
  
  for (iVar = 0; iVar < nVar; iVar++) {
    SetRes_RMS(iVar, 0.0);
    SetRes_Max(iVar, 0.0, 0);
  }
  
  /*--- Compute the residual ---*/
  
  for (iPoint = 0; iPoint < geometry->GetnPoint(); iPoint++) {
    for (iVar = 0; iVar < nVar; iVar++) {
      total_index = iPoint*nVar+iVar;
      AddRes_RMS(iVar, LinSysAux[total_index]*LinSysAux[total_index]);
      AddRes_Max(iVar, fabs(LinSysAux[total_index]), geometry->node[iPoint]->GetGlobalIndex(), geometry->node[iPoint]->GetCoord());
    }
  }
  
  /*--- Compute the root mean square residual ---*/
  
  SetResidual_RMS(geometry, config);
  
}