#!/usr/bin/env python

## \file phys_problem.py
#  \brief python package for reading in the physics of the problem
#  \author R. Sanchez
#  \version 5.0.0 "Raven"
#
# SU2 Lead Developers: Dr. Francisco Palacios (Francisco.D.Palacios@boeing.com).
#                      Dr. Thomas D. Economon (economon@stanford.edu).
#
# SU2 Developers: Prof. Juan J. Alonso's group at Stanford University.
#                 Prof. Piero Colonna's group at Delft University of Technology.
#                 Prof. Nicolas R. Gauger's group at Kaiserslautern University of Technology.
#                 Prof. Alberto Guardone's group at Polytechnic University of Milan.
#                 Prof. Rafael Palacios' group at Imperial College London.
#                 Prof. Edwin van der Weide's group at the University of Twente.
#                 Prof. Vincent Terrapon's group at the University of Liege.
#
# Copyright (C) 2012-2017 SU2, the open-source CFD code.
#
# SU2 is free software; you can redistribute it and/or
# modify it under the terms of the GNU Lesser General Public
# License as published by the Free Software Foundation; either
# version 2.1 of the License, or (at your option) any later version.
#
# SU2 is distributed in the hope that it will be useful,
# but WITHOUT ANY WARRANTY; without even the implied warranty of
# MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE. See the GNU
# Lesser General Public License for more details.
#
# You should have received a copy of the GNU Lesser General Public
# License along with SU2. If not, see <http://www.gnu.org/licenses/>.

import os, sys, shutil, copy
import numpy as np
from ..util import bunch, ordered_bunch, switch
from .tools import *
from config_options import *

try:
    from collections import OrderedDict
except ImportError:
    from ..util.ordered_dict import OrderedDict

inf = 1.0e20

# ----------------------------------------------------------------------
#  Problem Class
# ----------------------------------------------------------------------

class physics(object):
    """ config = SU2.io.phys_problem()

        Starts a generic problem class, an object that hosts
        filenames and other properties that are problem-dependent

        use 1: initialize by reading config object
            dv = SU2.io.problem(config_direct, config_adjoint)

        Parameters can be accessed by item
        ie: config['MESH_FILENAME']

        Methods:
            read()       - read from a dv file
            write()      - write to a dv file (requires existing file)
    """

    def __init__(self, config, oFunction = 'NONE'):

        self.name = 'No_physics'

    def merge_solution(self, config):

        if config.MATH_PROBLEM == 'DIRECT':
            config.SOLUTION_FLOW_FILENAME = config.RESTART_FLOW_FILENAME
        elif config.MATH_PROBLEM in ['CONTINUOUS_ADJOINT', 'DISCRETE_ADJOINT']:
            config.SOLUTION_ADJ_FILENAME = config.RESTART_ADJ_FILENAME

    def get_direct_files(self):

        restarts = self.files['RESTART_DIRECT']
        solutions = self.files['DIRECT']

        if self.timeDomain:
            n_time = self.nTime
            restarts = []
            solutions = []
            for j in range(0,len(restarts)):
                # Add suffix to restart and solution files
                restart_pat = add_suffix(restarts[j], '%05d')
                solution_pat = add_suffix(solutions[j], '%05d')
                # Append indices corresponding to time instances
                res  = [restart_pat % i for i in range(n_time)]
                sols = [solution_pat % i for i in range(n_time)]
                # Append to final restart and solution vectors
                restarts.append(res)
                sols.append(sols)

        return restarts, solutions

    def get_adjoint_files(self, suffix):

        restarts = self.files['RESTART_ADJOINT']
        solutions = self.files['ADJOINT']

        if self.timeDomain:
            n_time = self.nTime
            restarts = []
            solutions = []
            for j in range(0,len(restarts)):
                # Add suffix relative to the kind of objective function
                # TODO: this if, only until binaries are modified
                if self.kind != 'FEM_ELASTICITY':
                    restart = add_suffix(restart, suffix)
                    solution = add_suffix(solution, suffix)
                # Add time suffix to restart and solution files
                restart_pat = add_suffix(restarts[j], '%05d')
                solution_pat = add_suffix(solutions[j], '%05d')
                # Append indices corresponding to time instances
                res  = [restart_pat % i for i in range(n_time)]
                sols = [solution_pat % i for i in range(n_time)]
                # Append to final restart and solution vectors
                restarts.append(res)
                sols.append(sols)

        return restarts, solutions



class flow(physics):
    """ config = SU2.io.physics()

        Starts a generic problem class, an object that hosts
        filenames and other properties that are problem-dependent

        use 1: initialize by reading config object
            dv = SU2.io.problem(config_direct, config_adjoint)

        Parameters can be accessed by item
        ie: config['MESH_FILENAME']

        Methods:
            read()       - read from a dv file
            write()      - write to a dv file (requires existing file)
    """

    def __init__(self, config, oFunction = 'NONE'):

        self.nZone = 1
        self.kind = config.PHYSICAL_PROBLEM
        self.math = config.MATH_PROBLEM
        restart = config.RESTART_SOL == 'YES'
        self.restart = restart

        if oFunction == 'NONE':
            self.oFunction = config.OBJECTIVE_FUNCTION
        else:
            self.oFunction = oFunction

        if ('UNSTEADY_SIMULATION' in config) and (config.get('UNSTEADY_SIMULATION','NO') != 'NO'):
            self.timeDomain = True
            if 'UNST_ADJOINT_ITER' in config:
                self.nTime = config['UNST_ADJOINT_ITER']
            else:
                self.nTime = config['EXT_ITER']
        else:
            self.timeDomain = False

        files = OrderedDict()
        files['MESH'] = config.MESH_FILENAME
        files['DIRECT'] = [config.SOLUTION_FLOW_FILENAME]
        files['ADJOINT'] = [config.SOLUTION_ADJ_FILENAME]
        files['RESTART_DIRECT']  = [config.RESTART_FLOW_FILENAME]
        files['RESTART_ADJOINT'] = [config.RESTART_ADJ_FILENAME]

        special_files = {'EQUIV_AREA': 'TargetEA.dat',
                         'INV_DESIGN_CP': 'TargetCp.dat',
                         'INV_DESIGN_HEATFLUX': 'TargetHeatFlux.dat'}

        for key in special_files:
            if config.has_key(key) and config[key] == 'YES':
                files[key] = special_files[key]

        self.files = files

    def merge_solution(self, config):

        if config.MATH_PROBLEM == 'DIRECT':
            config.SOLUTION_FLOW_FILENAME = config.RESTART_FLOW_FILENAME
        elif config.MATH_PROBLEM in ['CONTINUOUS_ADJOINT', 'DISCRETE_ADJOINT']:
            config.SOLUTION_ADJ_FILENAME = config.RESTART_ADJ_FILENAME

class fea(physics):
    """ config = SU2.io.physics()

        Starts a FEA problem class, an object that hosts
        filenames and other properties that are problem-dependent

        use 1: initialize by reading config object
            dv = SU2.io.fea_problem(config_direct, config_adjoint)

        Parameters can be accessed by item
        ie: fea_problem.files['MESH']

        Methods:
            read()       - read from a dv file
            write()      - write to a dv file (requires existing file)
    """

    def __init__(self, config, oFunction = 'NONE'):

        self.nZone = 1
        self.kind = config.PHYSICAL_PROBLEM
        self.math = config.MATH_PROBLEM
        restart = config.RESTART_SOL == 'YES'
        self.restart = restart

        if oFunction == 'NONE':
            self.oFunction = config.OBJECTIVE_FUNCTION
        else:
            self.oFunction = oFunction

        if ('DYNAMIC_ANALYSIS' in config) and (config.DYNAMIC_ANALYSIS == 'YES'):
            self.timeDomain = True
            if 'UNST_ADJOINT_ITER' in config:
                self.nTime = config['UNST_ADJOINT_ITER']
            else:
                self.nTime = config['EXT_ITER']
        else:
            self.timeDomain = False

        files = OrderedDict()
        files['MESH'] = config.MESH_FILENAME
        files['DIRECT'] = [config.SOLUTION_STRUCTURE_FILENAME]
        files['ADJOINT'] = [config.SOLUTION_ADJ_STRUCTURE_FILENAME]
        files['RESTART_DIRECT']  = [config.RESTART_STRUCTURE_FILENAME]
        files['RESTART_ADJOINT'] = [config.RESTART_ADJ_STRUCTURE_FILENAME]

        special_cases = ['PRESTRETCH',
                         'REFERENCE_GEOMETRY']

        for key in special_cases:
            if config.has_key(key) and config[key] == 'YES':
                if key == 'PRESTRETCH':
                    files[key] = config.PRESTRETCH_FILENAME
                if key == 'REFERENCE_GEOMETRY':
                    files[key] = config.REFERENCE_GEOMETRY_FILENAME

        if config.has_key('FEA_FILENAME'):
            files['ELEMENT_PROP'] = config.FEA_FILENAME

        self.files = files

    def merge_solution(self, config):

        if config.MATH_PROBLEM == 'DIRECT':
            config.SOLUTION_STRUCTURE_FILENAME = config.RESTART_STRUCTURE_FILENAME
        elif config.MATH_PROBLEM in ['CONTINUOUS_ADJOINT', 'DISCRETE_ADJOINT']:
            config.SOLUTION_ADJ_STRUCTURE_FILENAME = config.RESTART_ADJ_STRUCTURE_FILENAME


class fsi(physics):

    """ config = SU2.io.physics()

        Starts a FSI problem class, an object that hosts
        filenames and other properties that are problem-dependent

        use 1: initialize by reading config object
            dv = SU2.io.fea_problem(config_direct, config_adjoint)

        Parameters can be accessed by item
        ie: fea_problem.files['MESH']

        Methods:
            read()       - read from a dv file
            write()      - write to a dv file (requires existing file)
    """

    def __init__(self, config, oFunction = 'NONE'):

        self.nZone = 2
        self.kind = config.PHYSICAL_PROBLEM
        self.math = config.MATH_PROBLEM
        restart = config.RESTART_SOL == 'YES'
        self.restart = restart

        # Record objective function
        if oFunction == 'NONE':
            self.oFunction = config.OBJECTIVE_FUNCTION
        else:
            self.oFunction = oFunction

        if (('UNSTEADY_SIMULATION' in config) and (config.get('UNSTEADY_SIMULATION','NO') != 'NO')) \
            and (('DYNAMIC_ANALYSIS' in config) and (config.DYNAMIC_ANALYSIS == 'YES')):
            self.timeDomain = True
            if 'UNST_ADJOINT_ITER' in config:
                self.nTime = config['UNST_ADJOINT_ITER']
            else:
                self.nTime = config['EXT_ITER']
        else:
            self.timeDomain = False

        files = OrderedDict()
        # Mesh file
        files['MESH'] = config.MESH_FILENAME
        # Direct files
        files['DIRECT'] = [add_suffix(config.SOLUTION_FLOW_FILENAME, '0')]
        files['DIRECT'].append(add_suffix(config.SOLUTION_STRUCTURE_FILENAME, '1'))
        # Adjoint files
        files['ADJOINT'] = [add_suffix(config.SOLUTION_ADJ_FILENAME, '0')]
        files['ADJOINT'].append(add_suffix(config.SOLUTION_ADJ_STRUCTURE_FILENAME, '1'))
        # Restart (direct) files
        files['RESTART_DIRECT'] = [add_suffix(config.RESTART_FLOW_FILENAME, '0')]
        files['RESTART_DIRECT'].append(add_suffix(config.RESTART_STRUCTURE_FILENAME, '1'))
        # Restart (direct) files
        files['RESTART_ADJOINT'] = [add_suffix(config.RESTART_ADJ_FILENAME, '0')]
        files['RESTART_ADJOINT'].append(add_suffix(config.RESTART_ADJ_STRUCTURE_FILENAME, '1'))

        special_cases = ['PRESTRETCH',
                         'REFERENCE_GEOMETRY']

        for key in special_cases:
            if config.has_key(key) and config[key] == 'YES':
                if key == 'PRESTRETCH':
                    files[key] = add_suffix(config.PRESTRETCH_FILENAME,'1')
                if key == 'REFERENCE_GEOMETRY':
                    files[key] = add_suffix(config.REFERENCE_GEOMETRY_FILENAME,'1')

        if config.has_key('FEA_FILENAME'):
            files['ELEMENT_PROP'] = add_suffix(config.FEA_FILENAME,'1')

        self.files = files

    def merge_solution(self, config):

        if config.MATH_PROBLEM == 'DIRECT':
            config.SOLUTION_STRUCTURE_FILENAME = config.RESTART_STRUCTURE_FILENAME
            config.SOLUTION_FLOW_FILENAME = config.RESTART_FLOW_FILENAME
        elif config.MATH_PROBLEM in ['CONTINUOUS_ADJOINT', 'DISCRETE_ADJOINT']:
            config.SOLUTION_ADJ_STRUCTURE_FILENAME = config.RESTART_ADJ_STRUCTURE_FILENAME
            config.SOLUTION_ADJ_FILENAME = config.RESTART_ADJ_FILENAME

    def get_adjoint_files(self, suffix):

        res = self.files['RESTART_ADJOINT']
        sol = self.files['ADJOINT']

        # TODO: this is only until binaries are modified
        # Difficult to sort out the order of multizone and objective function

        restarts = [res[1]]
        solutions = [sol[1]]

        restart_flow = res[0]
        restart_fea = res[1]
        restart_flow = add_suffix(restart_flow, suffix)

        solution_flow = sol[0]
        solution_fea = sol[1]
        solution_flow = add_suffix(restart_flow, suffix)

        return restarts, solutions


def read_physics(config, oFunction = 'NONE'):
    """ reads the properties of a problem from config files """

    # Reads the kind of problem that we want to solve
    physics = config.PHYSICAL_PROBLEM

    for case in switch(physics):
        if case("FLUID_STRUCTURE_INTERACTION"):
            problem = fsi(config, oFunction)
            break
        if case("FEM_ELASTICITY"):
            problem = fea(config, oFunction)
            break
        if case():
            problem = flow(config, oFunction)

    return problem
